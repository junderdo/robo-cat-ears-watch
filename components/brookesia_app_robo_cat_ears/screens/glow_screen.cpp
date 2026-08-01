/*
 * Description: Glow screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "glow_screen.hpp"
#include "lighting_service.hpp"
#include "bluetooth_service.hpp"
#include "nvs.h"
#include <cstdio>
#include <cmath>

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:GlowScreen"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::screens {

namespace {

constexpr const char *NVS_NAMESPACE = "glow";
constexpr const char *NVS_KEY_COLORS = "colors";
constexpr const char *NVS_KEY_BRIGHT = "brightness";

// Perceived LED output rises roughly as the 2.2 power of drive level, so the
// slider position is raised to that power to make its travel feel even. If the
// peripheral firmware already gamma-corrects incoming RGB, this double-applies
// and the low end of the slider will be unusable - drop this to 1.0 if so.
constexpr float BRIGHTNESS_GAMMA = 2.2f;

// The translation layer: a color the user picked -> the color actually sent.
uint32_t applyBrightness(uint32_t rgb, int brightness)
{
    if (brightness >= 100) return rgb;
    if (brightness <= 0) return 0;

    const float f = powf(brightness / 100.0f, BRIGHTNESS_GAMMA);
    const uint32_t r = (uint32_t)lroundf(((rgb >> 16) & 0xFF) * f);
    const uint32_t g = (uint32_t)lroundf(((rgb >> 8) & 0xFF) * f);
    const uint32_t b = (uint32_t)lroundf((rgb & 0xFF) * f);
    return (r << 16) | (g << 8) | b;
}

} // namespace

GlowScreen::GlowScreen(lv_obj_t *parent_screen)
    : _container(nullptr),
      _status_label(nullptr),
      _add_color_btn(nullptr),
      _color_list_container(nullptr),
      _trash_icon(nullptr),
      _modes_btn_label(nullptr),
      _on_add_color_clicked(nullptr),
      _current_mode("Solid"),
      _current_speed(50),
      _last_reorder_from_index(-1),
      _last_reorder_to_index(-1),
      _loading_from_device(false),
      _brightness_slider(nullptr),
      _brightness_label(nullptr),
      _brightness_debounce_timer(nullptr),
      _brightness(100),
      _loaded_from_nvs(false)
{
    ESP_UTILS_LOGD("Creating glow screen");

    // Create a container for the glow screen
    _container = lv_obj_create(parent_screen);
    lv_obj_set_size(_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);

    // Allow gestures to bubble up to parent for swipe navigation
    lv_obj_add_flag(_container, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Create a connection status label at the top
    _status_label = lv_label_create(_container);
    lv_label_set_text(_status_label, "Not connected");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_status_label, LV_ALIGN_TOP_MID, 0, 36);

    // Create a scrollable container for the color list
    _color_list_container = lv_obj_create(_container);
    lv_obj_set_size(_color_list_container, lv_pct(90), 150);
    lv_obj_align(_color_list_container, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_opa(_color_list_container, LV_OPA_10, 0);
    lv_obj_set_style_border_width(_color_list_container, 1, 0);
    lv_obj_set_style_border_color(_color_list_container, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(_color_list_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(_color_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(_color_list_container, 10, 0);
    lv_obj_set_style_pad_gap(_color_list_container, 10, 0);
    
    // Enable layout animations (600ms for smooth, visible animation)
    lv_obj_set_style_anim_time(_color_list_container, 600, 0);

    // Overall brightness. Applied only to the colors written over BLE - the
    // swatches above always show what the user actually picked.
    _brightness_label = lv_label_create(_container);
    lv_obj_set_style_text_font(_brightness_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_brightness_label, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(_brightness_label, LV_ALIGN_TOP_LEFT, 20, 228);

    _brightness_slider = lv_slider_create(_container);
    lv_obj_set_size(_brightness_slider, lv_pct(80), 24);
    lv_obj_align(_brightness_slider, LV_ALIGN_TOP_MID, 0, 256);
    lv_slider_set_range(_brightness_slider, 0, 100);
    lv_slider_set_value(_brightness_slider, _brightness, LV_ANIM_OFF);

    // Debounced so dragging doesn't flood the BLE link - matches the speed
    // slider's pattern in modes_screen.
    lv_obj_add_event_cb(_brightness_slider, [](lv_event_t *e) {
        GlowScreen *screen = (GlowScreen *)lv_event_get_user_data(e);
        if (!screen) return;

        // Track the label live so dragging gives immediate feedback.
        screen->_brightness = lv_slider_get_value(screen->_brightness_slider);
        screen->updateBrightnessLabel();

        if (screen->_brightness_debounce_timer) {
            lv_timer_del(screen->_brightness_debounce_timer);
            screen->_brightness_debounce_timer = nullptr;
        }

        screen->_brightness_debounce_timer = lv_timer_create([](lv_timer_t *timer) {
            GlowScreen *screen = (GlowScreen *)lv_timer_get_user_data(timer);
            if (screen) {
                screen->_brightness_debounce_timer = nullptr;
                // _brightness was already updated live by the slider callback, so
                // setBrightness() would see no change and skip the send - write
                // to the device directly instead.
                ESP_UTILS_LOGI("Brightness set to %d%%", screen->_brightness);
                screen->saveLightingDataToDevice();
            }
        }, 300, screen);
        lv_timer_set_repeat_count(screen->_brightness_debounce_timer, 1);
    }, LV_EVENT_VALUE_CHANGED, this);

    // Create "Add Color" button (below color list)
    _add_color_btn = lv_btn_create(_container);
    lv_obj_set_size(_add_color_btn, 260, 60);
    lv_obj_align(_add_color_btn, LV_ALIGN_TOP_MID, 0, 290);

    lv_obj_t *add_label = lv_label_create(_add_color_btn);
    lv_label_set_text(add_label, LV_SYMBOL_PLUS " Add Color");
    lv_obj_set_style_text_font(add_label, &lv_font_montserrat_24, 0);
    lv_obj_center(add_label);

    // Add event handler for add color button
    lv_obj_add_event_cb(_add_color_btn, [](lv_event_t *e) {
        // Ignore click if a gesture (swipe) was detected
        lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (gesture_dir != LV_DIR_NONE) {
            return; // Swipe detected, don't process as click
        }

        GlowScreen *screen = (GlowScreen *)lv_event_get_user_data(e);
        if (screen && screen->_on_add_color_clicked) {
            screen->_on_add_color_clicked();
        }
    }, LV_EVENT_CLICKED, this);

    // Create "Modes" button at bottom
    lv_obj_t *modes_btn = lv_btn_create(_container);
    lv_obj_set_size(modes_btn, 260, 60);
    lv_obj_align(modes_btn, LV_ALIGN_BOTTOM_MID, 0, -80);
    
    _modes_btn_label = lv_label_create(modes_btn);
    lv_label_set_text(_modes_btn_label, LV_SYMBOL_SETTINGS " Solid | " LV_SYMBOL_PLAY " 50%");
    lv_obj_set_style_text_font(_modes_btn_label, &lv_font_montserrat_22, 0);
    lv_obj_center(_modes_btn_label);
    
    // Create modes screen
    _modes_screen = std::make_unique<ModesScreen>(_container);
    
    // Set up callbacks
    _modes_screen->setOnModeSelected([this](const char *mode) {
        ESP_UTILS_LOGI("Mode selected: %s", mode);
        
        // Check if mode actually changed
        bool changed = (_current_mode != mode);
        _current_mode = mode;
        updateModesButtonLabel();
        
        // Save to device if mode changed
        if (changed) {
            saveLightingDataToDevice();
        }
    });
    
    _modes_screen->setOnSpeedChanged([this](int speed) {
        ESP_UTILS_LOGI("Speed changed: %d", speed);
        
        // Check if speed actually changed
        bool changed = (_current_speed != speed);
        _current_speed = speed;
        updateModesButtonLabel();
        
        // Save to device if speed changed
        if (changed) {
            saveLightingDataToDevice();
        }
    });
    
    _modes_screen->setOnConfirmed([this]() {
        ESP_UTILS_LOGI("Modes confirmed - Mode: %s, Speed: %d", _current_mode.c_str(), _current_speed);
        // Mode and speed are already saved via callbacks above
    });
    
    // Event handler for modes button - show modal
    lv_obj_add_event_cb(modes_btn, [](lv_event_t *e) {
        GlowScreen *screen = (GlowScreen *)lv_event_get_user_data(e);
        if (screen && screen->_modes_screen) {
            screen->_modes_screen->show();
        }
    }, LV_EVENT_CLICKED, this);
    
    // Create trash icon at bottom (initially hidden)
    _trash_icon = lv_obj_create(_container);
    lv_obj_set_size(_trash_icon, 80, 80);
    lv_obj_align(_trash_icon, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(_trash_icon, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(_trash_icon, LV_OPA_70, 0);
    lv_obj_set_style_border_width(_trash_icon, 2, 0);
    lv_obj_set_style_border_color(_trash_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(_trash_icon, 10, 0);
    lv_obj_add_flag(_trash_icon, LV_OBJ_FLAG_HIDDEN); // Initially hidden
    
    // Add trash icon label
    lv_obj_t *trash_label = lv_label_create(_trash_icon);
    lv_label_set_text(trash_label, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_font(trash_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(trash_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(trash_label);

    // Restore the user's own colors and brightness before touching the device.
    // These are authoritative: the peripheral only ever sees dimmed copies.
    _loaded_from_nvs = loadStateFromNvs();
    lv_slider_set_value(_brightness_slider, _brightness, LV_ANIM_OFF);
    updateBrightnessLabel();
    if (_loaded_from_nvs) {
        updateColorList();
        ESP_UTILS_LOGI("Restored %zu colors and brightness %d%% from NVS", _colors.size(), _brightness);
    }

    ESP_UTILS_LOGD("Glow screen created successfully");
    // Load lighting data if already connected when screen is created
    robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
    if (bt_service && bt_service->isConnected() && bt_service->isServiceDiscovered()) {
        ESP_UTILS_LOGI("Already connected and discovered, loading lighting data immediately");
        loadLightingData();
    }
}

GlowScreen::~GlowScreen()
{
    // LVGL objects are automatically cleaned up when parent is deleted, but a
    // timer is not parented to one - a pending debounce would fire into a
    // destroyed screen.
    if (_brightness_debounce_timer) {
        lv_timer_del(_brightness_debounce_timer);
        _brightness_debounce_timer = nullptr;
    }
}

void GlowScreen::addColor(uint32_t color)
{
    _colors.push_back(color);
    ESP_UTILS_LOGI("Added color: 0x%06X", color);
    updateColorList();
    
    // Save to device
    saveLightingDataToDevice();
}

void GlowScreen::removeColor(int index)
{
    if (index >= 0 && index < _colors.size()) {
        ESP_UTILS_LOGI("Removing color at index %d: 0x%06X", index, _colors[index]);
        
        // Remove from vector
        _colors.erase(_colors.begin() + index);
        
        // Find and delete the corresponding UI element
        uint32_t child_count = lv_obj_get_child_cnt(_color_list_container);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(_color_list_container, i);
            int *index_ptr = (int *)lv_obj_get_user_data(child);
            if (index_ptr && *index_ptr == index) {
                // Found the item to remove
                delete index_ptr;
                lv_obj_del(child);
                break;
            }
        }
        
        // Update indices for remaining children (all items after the deleted one need their index decremented)
        child_count = lv_obj_get_child_cnt(_color_list_container);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(_color_list_container, i);
            int *index_ptr = (int *)lv_obj_get_user_data(child);
            if (index_ptr && *index_ptr > index) {
                (*index_ptr)--;
            }
        }
        
        // The layout will automatically animate the remaining items thanks to lv_obj_set_style_anim_time
        
        // Save to device
        saveLightingDataToDevice();
    }
}

void GlowScreen::updateModesButtonLabel()
{
    if (_modes_btn_label) {
        char label_text[64];
        snprintf(label_text, sizeof(label_text), LV_SYMBOL_SETTINGS " %s | " LV_SYMBOL_PLAY " %d%%", _current_mode.c_str(), _current_speed);
        lv_label_set_text(_modes_btn_label, label_text);
    }
}
void GlowScreen::reorderColor(int from_index, int to_index)
{
    if (from_index < 0 || from_index >= _colors.size() ||
        to_index < 0 || to_index >= _colors.size() ||
        from_index == to_index) {
        return;
    }
    
    // Move the color in the vector
    uint32_t color = _colors[from_index];
    _colors.erase(_colors.begin() + from_index);
    _colors.insert(_colors.begin() + to_index, color);
    
    // Find the UI element being dragged
    lv_obj_t *dragged_obj = nullptr;
    uint32_t child_count = lv_obj_get_child_cnt(_color_list_container);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(_color_list_container, i);
        int *index_ptr = (int *)lv_obj_get_user_data(child);
        if (index_ptr && *index_ptr == from_index) {
            dragged_obj = child;
            break;
        }
    }
    
    if (dragged_obj) {
        // Move the UI element to the new position
        // LVGL uses 0-based indexing, where 0 is the first child
        lv_obj_move_to_index(dragged_obj, to_index);
        
        // Update all indices in user_data to match new positions
        child_count = lv_obj_get_child_cnt(_color_list_container);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(_color_list_container, i);
            int *index_ptr = (int *)lv_obj_get_user_data(child);
            if (index_ptr) {
                *index_ptr = i; // Update to match visual position
            }
        }
        
        // Save to device after reorder
        saveLightingDataToDevice();
    }
}
void GlowScreen::updateColorList()
{
    // Free allocated memory for indices before clearing
    uint32_t child_count = lv_obj_get_child_cnt(_color_list_container);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(_color_list_container, i);
        int *index_ptr = (int *)lv_obj_get_user_data(child);
        if (index_ptr) {
            delete index_ptr;
            lv_obj_set_user_data(child, nullptr);
        }
    }
    
    // Clear existing color squares
    lv_obj_clean(_color_list_container);

    // Create a color square for each color in the list
    for (size_t i = 0; i < _colors.size(); i++) {
        uint32_t color = _colors[i];

        // Create color square
        lv_obj_t *color_item = lv_obj_create(_color_list_container);
        lv_obj_set_size(color_item, 80, 80);
        lv_obj_set_style_bg_color(color_item, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(color_item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(color_item, 2, 0);
        lv_obj_set_style_border_color(color_item, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(color_item, 8, 0);
        lv_obj_clear_flag(color_item, LV_OBJ_FLAG_SCROLLABLE);

        // Make the color square draggable
        lv_obj_add_flag(color_item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(color_item, LV_OBJ_FLAG_GESTURE_BUBBLE);

        // Store the index in user data
        int *index_ptr = new int(i);
        lv_obj_set_user_data(color_item, index_ptr);

        // Add event handler for press (start of drag)
        lv_obj_add_event_cb(color_item, [](lv_event_t *e) {
            lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
            
            // Get the screen from the parent hierarchy
            lv_obj_t *color_list = lv_obj_get_parent(obj);
            GlowScreen *screen = (GlowScreen *)lv_obj_get_user_data(color_list);
            
            if (screen) {
                // Disable scrolling on the color list while dragging
                lv_obj_clear_flag(screen->_color_list_container, LV_OBJ_FLAG_SCROLLABLE);
                
                // Show trash icon when starting to drag
                if (screen->_trash_icon) {
                    lv_obj_clear_flag(screen->_trash_icon, LV_OBJ_FLAG_HIDDEN);
                }
                
                // Reset reorder tracking for new drag
                screen->_last_reorder_from_index = -1;
                screen->_last_reorder_to_index = -1;
                
                // Darken the original square to show it's being dragged
                lv_obj_set_style_opa(obj, LV_OPA_30, 0);
                
                // Get the color from the original square
                lv_color_t color = lv_obj_get_style_bg_color(obj, 0);
                
                // Get current position in screen coordinates
                lv_area_t obj_coords;
                lv_obj_get_coords(obj, &obj_coords);
                
                // Create a clone for dragging
                lv_obj_t *drag_clone = lv_obj_create(screen->_container);
                lv_obj_set_size(drag_clone, 80, 80);
                lv_obj_set_style_bg_color(drag_clone, color, 0);
                lv_obj_set_style_bg_opa(drag_clone, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(drag_clone, 2, 0);
                lv_obj_set_style_border_color(drag_clone, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_radius(drag_clone, 8, 0);
                
                // Position the clone at the same location
                lv_area_t parent_coords;
                lv_obj_get_coords(screen->_container, &parent_coords);
                lv_obj_set_pos(drag_clone, obj_coords.x1 - parent_coords.x1, obj_coords.y1 - parent_coords.y1);
                
                // Make the clone float and bring to foreground
                lv_obj_add_flag(drag_clone, LV_OBJ_FLAG_FLOATING);
                lv_obj_move_foreground(drag_clone);
                
                // Store reference to original object and clone in user data
                // We'll use a simple struct to pass both pointers
                struct DragData {
                    lv_obj_t *original;
                    lv_obj_t *clone;
                };
                DragData *drag_data = new DragData{obj, drag_clone};
                lv_obj_set_user_data(drag_clone, drag_data);
            }
        }, LV_EVENT_PRESSED, nullptr);

        // Add event handler for pressing (dragging movement)
        lv_obj_add_event_cb(color_item, [](lv_event_t *e) {
            lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
            lv_indev_t *indev = lv_indev_get_act();
            
            // Get the screen to find the drag clone
            lv_obj_t *color_list = lv_obj_get_parent(obj);
            GlowScreen *screen = (GlowScreen *)lv_obj_get_user_data(color_list);
            
            if (indev && screen) {
                // Find the drag clone - it's a child of _container with matching opacity on original
                // We need to search for it
                lv_obj_t *drag_clone = nullptr;
                uint32_t child_count = lv_obj_get_child_cnt(screen->_container);
                for (uint32_t i = 0; i < child_count; i++) {
                    lv_obj_t *child = lv_obj_get_child(screen->_container, i);
                    void *user_data = lv_obj_get_user_data(child);
                    if (user_data) {
                        struct DragData {
                            lv_obj_t *original;
                            lv_obj_t *clone;
                        };
                        DragData *drag_data = (DragData *)user_data;
                        if (drag_data->original == obj) {
                            drag_clone = drag_data->clone;
                            break;
                        }
                    }
                }
                
                if (drag_clone) {
                    lv_point_t point;
                    lv_indev_get_point(indev, &point);
                    
                    // Get parent coordinates to convert to relative position
                    lv_area_t parent_coords;
                    lv_obj_get_coords(screen->_container, &parent_coords);
                    
                    // Convert screen coordinates to parent-relative coordinates
                    lv_coord_t rel_x = point.x - parent_coords.x1;
                    lv_coord_t rel_y = point.y - parent_coords.y1;
                    
                    // Center the clone under the pointer
                    lv_obj_set_pos(drag_clone, rel_x - lv_obj_get_width(drag_clone) / 2,
                                       rel_y - lv_obj_get_height(drag_clone) / 2);
                    
                    // Keep clone on top of everything
                    lv_obj_move_foreground(drag_clone);
                    
                    // Check if clone is hovering over another color square
                    lv_area_t clone_coords;
                    lv_obj_get_coords(drag_clone, &clone_coords);
                    
                    // Get the current dragged item's index
                    int *dragged_index_ptr = (int *)lv_obj_get_user_data(obj);
                    if (dragged_index_ptr) {
                        int dragged_index = *dragged_index_ptr;
                        
                        // Check all color squares for overlap
                        uint32_t color_count = lv_obj_get_child_cnt(screen->_color_list_container);
                        for (uint32_t i = 0; i < color_count; i++) {
                            lv_obj_t *other_square = lv_obj_get_child(screen->_color_list_container, i);
                            
                            // Skip the dragged square itself
                            if (other_square == obj) continue;
                            
                            int *other_index_ptr = (int *)lv_obj_get_user_data(other_square);
                            if (other_index_ptr) {
                                int other_index = *other_index_ptr;
                                
                                // Check if clone overlaps with this square
                                lv_area_t other_coords;
                                lv_obj_get_coords(other_square, &other_coords);
                                
                                bool overlaps = !(clone_coords.x2 < other_coords.x1 ||
                                                 clone_coords.x1 > other_coords.x2 ||
                                                 clone_coords.y2 < other_coords.y1 ||
                                                 clone_coords.y1 > other_coords.y2);
                                
                                if (overlaps && dragged_index != other_index) {
                                    // Check if this is the same pair we just reordered (in either direction)
                                    bool is_same_pair = (dragged_index == screen->_last_reorder_from_index && 
                                                         other_index == screen->_last_reorder_to_index) ||
                                                        (dragged_index == screen->_last_reorder_to_index && 
                                                         other_index == screen->_last_reorder_from_index);
                                    
                                    if (!is_same_pair) {
                                        // Hovering over a different square - trigger reorder
                                        screen->reorderColor(dragged_index, other_index);
                                        screen->_last_reorder_from_index = dragged_index;
                                        screen->_last_reorder_to_index = other_index;
                                        break; // Only process one overlap at a time
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }, LV_EVENT_PRESSING, nullptr);

        // Add event handler for drag end
        lv_obj_add_event_cb(color_item, [](lv_event_t *e) {
            lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
            
            // Get the screen from the parent hierarchy
            lv_obj_t *color_list = lv_obj_get_parent(obj);
            GlowScreen *screen = (GlowScreen *)lv_obj_get_user_data(color_list);
            
            if (screen) {
                // Find the drag clone
                lv_obj_t *drag_clone = nullptr;
                struct DragData {
                    lv_obj_t *original;
                    lv_obj_t *clone;
                };
                DragData *drag_data = nullptr;
                
                uint32_t child_count = lv_obj_get_child_cnt(screen->_container);
                for (uint32_t i = 0; i < child_count; i++) {
                    lv_obj_t *child = lv_obj_get_child(screen->_container, i);
                    void *user_data = lv_obj_get_user_data(child);
                    if (user_data) {
                        DragData *data = (DragData *)user_data;
                        if (data->original == obj) {
                            drag_clone = data->clone;
                            drag_data = data;
                            break;
                        }
                    }
                }
                
                if (drag_clone && screen->_trash_icon) {
                    // Check if the clone was dropped on the trash icon
                    lv_area_t clone_coords;
                    lv_obj_get_coords(drag_clone, &clone_coords);
                    
                    lv_area_t trash_coords;
                    lv_obj_get_coords(screen->_trash_icon, &trash_coords);
                    
                    // Check if the areas overlap
                    bool overlaps = !(clone_coords.x2 < trash_coords.x1 ||
                                     clone_coords.x1 > trash_coords.x2 ||
                                     clone_coords.y2 < trash_coords.y1 ||
                                     clone_coords.y1 > trash_coords.y2);
                    
                    if (overlaps) {
                        // Dropped on trash - delete this color
                        int *index_ptr = (int *)lv_obj_get_user_data(obj);
                        if (index_ptr) {
                            ESP_UTILS_LOGI("Color dropped on trash, deleting index %d", *index_ptr);
                            int index_to_delete = *index_ptr;
                            
                            // Delete the clone
                            if (drag_data) {
                                delete drag_data;
                            }
                            lv_obj_del(drag_clone);
                            
                            // Remove from vector and rebuild list with animation
                            screen->removeColor(index_to_delete);
                        }
                    } else {
                        // Not dropped on trash - restore original opacity and delete clone
                        lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
                        
                        // Delete the clone
                        if (drag_data) {
                            delete drag_data;
                        }
                        lv_obj_del(drag_clone);
                    }
                    
                    // Hide trash icon
                    lv_obj_add_flag(screen->_trash_icon, LV_OBJ_FLAG_HIDDEN);
                    
                    // Re-enable scrolling on the color list
                    lv_obj_add_flag(screen->_color_list_container, LV_OBJ_FLAG_SCROLLABLE);
                    
                    // Reset reorder tracking
                    screen->_last_reorder_from_index = -1;
                    screen->_last_reorder_to_index = -1;
                }
            }
        }, LV_EVENT_RELEASED, nullptr);
    }

    // Store screen pointer in color list container for callback access
    lv_obj_set_user_data(_color_list_container, this);
}

void GlowScreen::loadLightingData()
{
    ESP_UTILS_LOGI("Attempting to load lighting data from service");
    
    // Wait for connection to fully stabilize after service discovery
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Check if we're connected to a device
    robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
    if (!bt_service || !bt_service->isConnected()) {
        ESP_UTILS_LOGD("Not connected to device, skipping lighting data load");
        return;
    }
    
    // Check if ABF2 characteristic is available (for reading)
    if (bt_service->getCharHandleABF2() == 0) {
        ESP_UTILS_LOGW("ABF2 characteristic not discovered, skipping lighting data load");
        return;
    }
    
    // Get lighting service instance
    robo_cat_ears::LightingService *lighting_service = robo_cat_ears::LightingService::getInstance();
    if (!lighting_service) {
        ESP_UTILS_LOGE("Failed to get lighting service instance");
        return;
    }
    
    // Initialize lighting service if not already done
    if (!lighting_service->init()) {
        ESP_UTILS_LOGE("Failed to initialize lighting service");
        return;
    }
    
    // Read lighting data from device
    robo_cat_ears::LightingData lighting_data;
    if (!lighting_service->readLightingData(&lighting_data, [this](const robo_cat_ears::LightingData& data) {
        // This callback runs when actual data is loaded from device (async)
        ESP_UTILS_LOGI("Data loaded callback: mode=%s, speed=%d, colors=%zu",
                       robo_cat_ears::LightingService::modeToString(data.mode),
                       data.speed,
                       data.colors.size());
        
        // Set flag to prevent saving while loading
        _loading_from_device = true;

        // 1+2. Colors. The peripheral only holds brightness-dimmed copies, so
        // adopting them would overwrite the user's originals with darker values
        // and darken them again on every reconnect. Take them only when we have
        // nothing of our own - a first run, or a device we've not seen before.
        if (!_loaded_from_nvs) {
            clearColors();
            for (const auto &color : data.colors) {
                addColor(color.toUint32());
            }
        } else {
            ESP_UTILS_LOGI("Keeping %zu colors from NVS, ignoring the device's dimmed copies",
                           _colors.size());
        }

        // 3. Set mode and speed. These round-trip losslessly, so the device
        // stays authoritative for them.
        const char *mode_str = robo_cat_ears::LightingService::modeToString(data.mode);
        setMode(mode_str);
        setSpeed(data.speed);

        // Clear flag after loading complete
        _loading_from_device = false;

        // Adopting the device's colors makes them ours from here on.
        if (!_loaded_from_nvs && !_colors.empty()) {
            _loaded_from_nvs = true;
            saveStateToNvs();
        }

        ESP_UTILS_LOGI("Lighting data applied to UI from callback");
    })) {
        ESP_UTILS_LOGW("Failed to read lighting data from device");
        return;
    }
    
    ESP_UTILS_LOGI("Read request sent, waiting for data to arrive asynchronously");
}

void GlowScreen::setMode(const std::string &mode)
{
    // Check if mode actually changed
    bool changed = (_current_mode != mode);
    
    _current_mode = mode;
    
    // Update the modes screen if it exists
    if (_modes_screen) {
        _modes_screen->setMode(mode.c_str());
    }
    
    // Update the button label
    updateModesButtonLabel();
    
    ESP_UTILS_LOGD("Mode set to: %s", mode.c_str());
    
    // Save to device if mode changed and not loading
    if (changed && !_loading_from_device) {
        saveLightingDataToDevice();
    }
}

void GlowScreen::setSpeed(int speed)
{
    if (speed < 1) speed = 1;
    if (speed > 100) speed = 100;
    
    // Check if speed actually changed
    bool changed = (_current_speed != speed);
    
    _current_speed = speed;
    
    // Update the modes screen if it exists
    if (_modes_screen) {
        _modes_screen->setSpeed(speed);
    }
    
    // Update the button label
    updateModesButtonLabel();
    
    ESP_UTILS_LOGD("Speed set to: %d", speed);
    
    // Save to device if speed changed and not loading
    if (changed && !_loading_from_device) {
        saveLightingDataToDevice();
    }
}

void GlowScreen::clearColors()
{
    _colors.clear();
    updateColorList();
    ESP_UTILS_LOGD("Colors cleared");
}

void GlowScreen::updateBrightnessLabel()
{
    if (!_brightness_label) {
        return;
    }
    char text[32];
    snprintf(text, sizeof(text), "Brightness  %d%%", _brightness);
    lv_label_set_text(_brightness_label, text);
}

void GlowScreen::saveStateToNvs()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_UTILS_LOGW("Failed to open NVS for writing: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_u8(handle, NVS_KEY_BRIGHT, (uint8_t)_brightness);

    // A zero-length blob isn't valid, so an empty list is stored as "no key".
    if (_colors.empty()) {
        nvs_erase_key(handle, NVS_KEY_COLORS);
    } else {
        nvs_set_blob(handle, NVS_KEY_COLORS, _colors.data(), _colors.size() * sizeof(uint32_t));
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_UTILS_LOGW("Failed to commit NVS: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
}

bool GlowScreen::loadStateFromNvs()
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_UTILS_LOGD("No stored glow state in NVS");
        return false;
    }

    uint8_t stored_brightness = 100;
    if (nvs_get_u8(handle, NVS_KEY_BRIGHT, &stored_brightness) == ESP_OK) {
        _brightness = (stored_brightness > 100) ? 100 : stored_brightness;
    }

    bool have_colors = false;
    size_t length = 0;
    if (nvs_get_blob(handle, NVS_KEY_COLORS, nullptr, &length) == ESP_OK &&
        length >= sizeof(uint32_t)) {
        _colors.resize(length / sizeof(uint32_t));
        if (nvs_get_blob(handle, NVS_KEY_COLORS, _colors.data(), &length) == ESP_OK) {
            have_colors = true;
        } else {
            _colors.clear();
        }
    }

    nvs_close(handle);
    return have_colors;
}

void GlowScreen::saveLightingDataToDevice()
{
    // Don't save if we're currently loading from device
    if (_loading_from_device) {
        return;
    }

    // Persist first, and unconditionally: local state is authoritative and must
    // survive even when there's nothing to send to.
    saveStateToNvs();

    // Check if we're connected to a device
    robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
    if (!bt_service || !bt_service->isConnected()) {
        ESP_UTILS_LOGD("Not connected to device, skipping save");
        return;
    }
    
    // Check if ABF1 characteristic is available
    if (bt_service->getCharHandleABF1() == 0) {
        ESP_UTILS_LOGW("ABF1 characteristic not discovered, skipping save");
        return;
    }
    
    // Get lighting service instance
    robo_cat_ears::LightingService *lighting_service = robo_cat_ears::LightingService::getInstance();
    if (!lighting_service) {
        ESP_UTILS_LOGE("Failed to get lighting service instance");
        return;
    }
    
    // Initialize lighting service if not already done
    if (!lighting_service->init()) {
        ESP_UTILS_LOGE("Failed to initialize lighting service");
        return;
    }
    
    // Create lighting data from current state
    robo_cat_ears::LightingData lighting_data;
    lighting_data.mode = robo_cat_ears::LightingService::stringToMode(_current_mode);
    lighting_data.speed = _current_speed;
    
    // Convert colors from uint32_t to RGBColor, dimming each by the brightness
    // slider on the way out. The protocol has no brightness field, so this is
    // the only place brightness exists on the wire.
    lighting_data.colors.clear();
    for (const auto &color : _colors) {
        lighting_data.colors.push_back(robo_cat_ears::RGBColor(applyBrightness(color, _brightness)));
    }

    // Write to device
    ESP_UTILS_LOGI("Saving lighting data to device: mode=%s, speed=%d, colors=%zu, brightness=%d%%",
                   _current_mode.c_str(), _current_speed, _colors.size(), _brightness);
    
    if (!lighting_service->writeLightingData(&lighting_data)) {
        ESP_UTILS_LOGE("Failed to write lighting data to device");
    } else {
        ESP_UTILS_LOGI("Successfully saved lighting data to device");
    }
}

} // namespace esp_brookesia::apps::screens
