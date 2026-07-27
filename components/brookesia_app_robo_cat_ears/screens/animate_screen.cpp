/*
 * Description: Animation control screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "animate_screen.hpp"
#include "animation_mode_service.hpp"
#include "bluetooth_service.hpp"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:AnimateScreen"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::screens {

AnimateScreen::AnimateScreen(lv_obj_t *parent_screen,
                             std::function<void(const std::string&)> on_command_clicked)
    : _container(nullptr),
      _status_label(nullptr),
      _auto_animate_switch(nullptr),
      _loading_from_device(false),
      _on_command_clicked(on_command_clicked)
{
    ESP_UTILS_LOGD("Creating animate screen");

    // Create a container for the animate screen
    _container = lv_obj_create(parent_screen);
    lv_obj_set_size(_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);

    // Create a connection status label at the top (same as scan screen)
    _status_label = lv_label_create(_container);
    lv_label_set_text(_status_label, "Not connected");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_status_label, LV_ALIGN_TOP_MID, 0, 36);

    // Calculate screen dimensions
    int screen_width = lv_obj_get_width(parent_screen);
    int screen_height = lv_obj_get_height(parent_screen);

    // Create Auto-animate toggle row (below status label)
    lv_obj_t *auto_row = lv_obj_create(_container);
    lv_obj_set_size(auto_row, screen_width - 20, 50);
    lv_obj_align(auto_row, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_opa(auto_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(auto_row, 0, 0);
    lv_obj_set_style_pad_hor(auto_row, 10, 0);
    lv_obj_set_style_pad_ver(auto_row, 5, 0);
    lv_obj_set_flex_flow(auto_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(auto_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(auto_row, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *auto_label = lv_label_create(auto_row);
    lv_label_set_text(auto_label, "Auto-animate");
    lv_obj_set_style_text_font(auto_label, &lv_font_montserrat_24, 0);
    lv_obj_add_flag(auto_label, LV_OBJ_FLAG_GESTURE_BUBBLE);

    _auto_animate_switch = lv_switch_create(auto_row);
    lv_obj_set_size(_auto_animate_switch, 60, 30);

    // Toggle event: send animation mode data over BLE
    lv_obj_add_event_cb(_auto_animate_switch, [](lv_event_t *e) {
        AnimateScreen *screen = (AnimateScreen *)lv_event_get_user_data(e);
        if (!screen || screen->_loading_from_device) {
            return;
        }

        robo_cat_ears::AnimationModeService *anim_service = robo_cat_ears::AnimationModeService::getInstance();
        if (!anim_service || !anim_service->init()) {
            return;
        }

        bool checked = lv_obj_has_state(screen->_auto_animate_switch, LV_STATE_CHECKED);
        robo_cat_ears::AnimationModeData data;
        if (checked) {
            data.mode_id = 1;
            data.frequency = 100;
        } else {
            data.mode_id = 0;
            data.frequency = 0;
        }

        ESP_UTILS_LOGI("Auto-animate toggled: %s (mode_id=%d, frequency=%d)",
                       checked ? "ON" : "OFF", data.mode_id, data.frequency);
        anim_service->writeAnimationModeData(&data);
    }, LV_EVENT_VALUE_CHANGED, this);

    // Button grid layout: account for toggle row height
    int btn_width = (screen_width - 30) / 2;  // 30 = padding + gap
    int btn_height = (screen_height - 160) / 2;  // 160 = top padding for status + toggle + gaps

    // Create a scrollable container for the buttons (below toggle row)
    lv_obj_t *scroll_container = lv_obj_create(_container);
    lv_obj_set_size(scroll_container, screen_width, screen_height - 136);
    lv_obj_set_pos(scroll_container, 0, 136);
    lv_obj_set_style_bg_opa(scroll_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 0, 0);
    lv_obj_set_scroll_dir(scroll_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_OFF);

    // Create 10 buttons in a 2-column grid (5 rows)
    const char *button_labels[] = {"Right", "Left", "Happy :)", "Sad :(", "Wiggle", "Radar", "Curious", "Alert", "Timid", "Bounce"};
    const char *button_commands[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};  // Commands to send for each button

    for (int i = 0; i < 10; i++) {
        int row = i / 2;
        int col = i % 2;
        int x_offset = col * (btn_width + 10) + 10;
        int y_offset = row * (btn_height + 10) + 10;

        lv_obj_t *btn = lv_btn_create(scroll_container);
        lv_obj_set_size(btn, btn_width, btn_height);
        lv_obj_set_pos(btn, x_offset, y_offset);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, button_labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_obj_center(label);

        // Create a command string for this button and store it
        std::string *cmd = new std::string(button_commands[i]);
        
        // Add event handler for command button
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            // Ignore click if a gesture (swipe) was detected
            lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());
            if (gesture_dir != LV_DIR_NONE) {
                return; // Swipe detected, don't process as click
            }
            
            lv_obj_t *btn = (lv_obj_t*)lv_event_get_current_target(e);
            AnimateScreen *screen = (AnimateScreen *)lv_obj_get_user_data(btn);
            std::string *cmd = (std::string *)lv_event_get_user_data(e);
            if (screen && screen->_on_command_clicked && cmd) {
                screen->_on_command_clicked(*cmd);
            }
        }, LV_EVENT_CLICKED, cmd);

        // Store screen pointer in button for callback
        lv_obj_set_user_data(btn, this);
    }

    ESP_UTILS_LOGD("Animate screen created successfully");
}

AnimateScreen::~AnimateScreen()
{
    // LVGL objects are automatically cleaned up when parent is deleted
    // Note: The command strings allocated with 'new' should ideally be cleaned up,
    // but since LVGL doesn't provide a way to iterate through event callbacks,
    // we rely on the fact that this destructor is called when the app is destroyed
}

void AnimateScreen::loadAnimationModeData()
{
    robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
    if (!bt_service || !bt_service->isConnected() || bt_service->getCharHandleABF2() == 0) {
        ESP_UTILS_LOGD("Not ready to load animation mode data (not connected or ABF2 not discovered)");
        return;
    }

    robo_cat_ears::AnimationModeService *anim_service = robo_cat_ears::AnimationModeService::getInstance();
    if (!anim_service || !anim_service->init()) {
        ESP_UTILS_LOGE("Failed to get/init animation mode service");
        return;
    }

    ESP_UTILS_LOGI("Loading animation mode data from device");

    robo_cat_ears::AnimationModeData anim_data;
    anim_service->readAnimationModeData(&anim_data, [this](const robo_cat_ears::AnimationModeData &data) {
        ESP_UTILS_LOGI("Animation mode data loaded: mode_id=%d, frequency=%d", data.mode_id, data.frequency);

        _loading_from_device = true;
        if (data.mode_id == 0) {
            lv_obj_clear_state(_auto_animate_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(_auto_animate_switch, LV_STATE_CHECKED);
        }
        _loading_from_device = false;
    });
}

} // namespace esp_brookesia::apps::screens
