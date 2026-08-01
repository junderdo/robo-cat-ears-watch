/*
 * Description: Modes screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "modes_screen.hpp"
#include <cstring>

namespace esp_brookesia {
namespace apps {
namespace screens {

ModesScreen::ModesScreen(lv_obj_t *parent)
    : _container(nullptr)
    , _panel(nullptr)
    , _modes_list(nullptr)
    , _speed_slider(nullptr)
    , _cancel_btn(nullptr)
    , _ok_btn(nullptr)
    , _selected_mode_btn(nullptr)
    , _speed_debounce_timer(nullptr)
    , _initial_speed(50)
    , _on_mode_selected(nullptr)
    , _on_confirmed(nullptr)
    , _on_speed_changed(nullptr)
    , _on_modal_shown(nullptr)
    , _on_modal_hidden(nullptr)
{
    // Initialize mode buttons array
    for (int i = 0; i < 5; i++) {
        _mode_buttons[i] = nullptr;
    }
    
    // Initialize initial mode to "Solid"
    strncpy(_initial_mode, "Solid", sizeof(_initial_mode) - 1);
    _initial_mode[sizeof(_initial_mode) - 1] = '\0';
    
    // Create full-screen container (modal overlay)
    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_container, LV_OPA_90, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_container, LV_OBJ_FLAG_HIDDEN); // Initially hidden

    // Create panel for modes
    _panel = lv_obj_create(_container);
    lv_obj_set_size(_panel, lv_pct(90), lv_pct(90));
    lv_obj_center(_panel);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(_panel, 2, 0);
    lv_obj_set_style_border_color(_panel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(_panel, 10, 0);
    lv_obj_clear_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(_panel);
    lv_label_set_text(title, "Lighting Modes");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Create scrollable list for modes
    _modes_list = lv_obj_create(_panel);
    lv_obj_set_size(_modes_list, lv_pct(90), 200);
    lv_obj_align(_modes_list, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_flex_flow(_modes_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_modes_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(_modes_list, 8, 0);
    lv_obj_set_style_pad_gap(_modes_list, 8, 0);
    lv_obj_set_scroll_dir(_modes_list, LV_DIR_VER);

    // Add mode buttons
    const char *mode_names[] = {"Solid", "Breathing", "Marquee", "Chasing", "Rain"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *mode_btn = lv_btn_create(_modes_list);
        lv_obj_set_size(mode_btn, lv_pct(100), 45);
        lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x404040), 0);
        lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x00AA00), LV_STATE_CHECKED);

        lv_obj_t *mode_label = lv_label_create(mode_btn);
        lv_label_set_text(mode_label, mode_names[i]);
        lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_20, 0);
        lv_obj_center(mode_label);
        
        // Store button reference
        _mode_buttons[i] = mode_btn;

        // Add click handler for mode selection
        lv_obj_add_event_cb(mode_btn, [](lv_event_t *e) {
            ModesScreen *screen = (ModesScreen *)lv_event_get_user_data(e);
            lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
            lv_obj_t *label = lv_obj_get_child(btn, 0);
            const char *mode_text = lv_label_get_text(label);
            
            // Update visual state
            screen->updateModeButtonStates(btn);
            
            if (screen && screen->_on_mode_selected && mode_text) {
                screen->_on_mode_selected(mode_text);
            }
        }, LV_EVENT_CLICKED, this);
    }

    // Set "Solid" as default active mode
    updateModeButtonStates(_mode_buttons[0]);

    // Speed slider at bottom of panel, with a label above it
    lv_obj_t *speed_label = lv_label_create(_panel);
    lv_label_set_text(speed_label, "Animation Speed");
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(speed_label, LV_ALIGN_BOTTOM_LEFT, 20, -122);

    _speed_slider = lv_slider_create(_panel);
    lv_obj_set_size(_speed_slider, lv_pct(75), 24);
    lv_obj_align(_speed_slider, LV_ALIGN_BOTTOM_MID, 0, -90);
    lv_slider_set_range(_speed_slider, 1, 100);
    lv_slider_set_value(_speed_slider, 50, LV_ANIM_OFF);

    // Add speed change handler with debouncing
    lv_obj_add_event_cb(_speed_slider, [](lv_event_t *e) {
        ModesScreen *screen = (ModesScreen *)lv_event_get_user_data(e);
        if (!screen) return;
        
        // Delete existing timer if it exists
        if (screen->_speed_debounce_timer) {
            lv_timer_del(screen->_speed_debounce_timer);
            screen->_speed_debounce_timer = nullptr;
        }
        
        // Create a new timer that will fire after 300ms
        screen->_speed_debounce_timer = lv_timer_create([](lv_timer_t *timer) {
            ModesScreen *screen = (ModesScreen *)lv_timer_get_user_data(timer);
            if (screen && screen->_on_speed_changed) {
                int speed = lv_slider_get_value(screen->_speed_slider);
                screen->_on_speed_changed(speed);
            }
            // Timer is automatically deleted after one-shot execution
            screen->_speed_debounce_timer = nullptr;
        }, 300, screen);
        lv_timer_set_repeat_count(screen->_speed_debounce_timer, 1);  // One-shot timer
    }, LV_EVENT_VALUE_CHANGED, this);

    // Create button container at bottom
    lv_obj_t *btn_container = lv_obj_create(_panel);
    lv_obj_set_size(btn_container, lv_pct(100), 60);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Cancel button
    _cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(_cancel_btn, 100, 50);
    lv_obj_set_style_bg_color(_cancel_btn, lv_color_hex(0x606060), 0);

    lv_obj_t *cancel_label = lv_label_create(_cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_add_event_cb(_cancel_btn, [](lv_event_t *e) {
        ModesScreen *screen = (ModesScreen *)lv_event_get_user_data(e);
        if (screen) {
            // Restore initial state
            screen->setMode(screen->_initial_mode);
            screen->setSpeed(screen->_initial_speed);
            screen->hide();
        }
    }, LV_EVENT_CLICKED, this);

    // OK button
    _ok_btn = lv_btn_create(btn_container);
    lv_obj_set_size(_ok_btn, 100, 50);
    lv_obj_set_style_bg_color(_ok_btn, lv_color_hex(0x00AA00), 0);
    lv_obj_t *ok_label = lv_label_create(_ok_btn);
    lv_label_set_text(ok_label, "OK");
    lv_obj_center(ok_label);

    lv_obj_add_event_cb(_ok_btn, [](lv_event_t *e) {
        ModesScreen *screen = (ModesScreen *)lv_event_get_user_data(e);
        if (screen) {
            if (screen->_on_confirmed) {
                screen->_on_confirmed();
            }
            screen->hide();
        }
    }, LV_EVENT_CLICKED, this);
}

void ModesScreen::updateModeButtonStates(lv_obj_t *selected_btn)
{
    _selected_mode_btn = selected_btn;
    
    // Update all button states
    for (int i = 0; i < 5; i++) {
        if (_mode_buttons[i]) {
            if (_mode_buttons[i] == selected_btn) {
                // Active state - green background
                lv_obj_set_style_bg_color(_mode_buttons[i], lv_color_hex(0x00AA00), 0);
            } else {
                // Inactive state - gray background
                lv_obj_set_style_bg_color(_mode_buttons[i], lv_color_hex(0x404040), 0);
            }
        }
    }
}

ModesScreen::~ModesScreen()
{
    // Clean up debounce timer if it exists
    if (_speed_debounce_timer) {
        lv_timer_del(_speed_debounce_timer);
        _speed_debounce_timer = nullptr;
    }
    
    if (_container) {
        lv_obj_del(_container);
        _container = nullptr;
    }
}

void ModesScreen::show()
{
    if (_container) {
        // Capture current state before showing modal
        if (_selected_mode_btn) {
            lv_obj_t *label = lv_obj_get_child(_selected_mode_btn, 0);
            const char *current_mode = lv_label_get_text(label);
            if (current_mode) {
                strncpy(_initial_mode, current_mode, sizeof(_initial_mode) - 1);
                _initial_mode[sizeof(_initial_mode) - 1] = '\0';
            }
        }
        if (_speed_slider) {
            _initial_speed = lv_slider_get_value(_speed_slider);
        }
        
        lv_obj_clear_flag(_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_container);
        
        // Restore visual state of selected mode
        if (_selected_mode_btn) {
            updateModeButtonStates(_selected_mode_btn);
        }
    }
    
    // Notify that modal is shown
    if (_on_modal_shown) {
        _on_modal_shown();
    }
}

void ModesScreen::hide()
{
    if (_container) {
        lv_obj_add_flag(_container, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Notify that modal is hidden
    if (_on_modal_hidden) {
        _on_modal_hidden();
    }
}

void ModesScreen::setMode(const char *mode)
{
    if (!mode) return;
    
    // Find the matching button and select it
    const char *mode_names[] = {"Solid", "Breathing", "Marquee", "Chasing", "Rain"};
    for (int i = 0; i < 5; i++) {
        if (_mode_buttons[i] && strcmp(mode_names[i], mode) == 0) {
            updateModeButtonStates(_mode_buttons[i]);
            
            // Don't trigger callback when setting programmatically
            // Callbacks should only be triggered by user interaction (button clicks)
            break;
        }
    }
}

void ModesScreen::setSpeed(int speed)
{
    if (_speed_slider && speed >= 1 && speed <= 100) {
        lv_slider_set_value(_speed_slider, speed, LV_ANIM_OFF);
        
        // Don't trigger callback when setting programmatically
        // Callbacks should only be triggered by user interaction (slider changes)
    }
}

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
