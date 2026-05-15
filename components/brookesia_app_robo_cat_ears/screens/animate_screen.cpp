/*
 * Description: Animation control screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "animate_screen.hpp"

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
    lv_obj_align(_status_label, LV_ALIGN_TOP_MID, 0, 15);

    // Calculate button size to fill the screen in a 2x2 grid
    int screen_width = lv_obj_get_width(parent_screen);
    int screen_height = lv_obj_get_height(parent_screen);
    int btn_width = (screen_width - 30) / 2;  // 30 = padding + gap
    int btn_height = (screen_height - 120) / 2;  // 120 = top padding for status + gaps

    // Create 4 buttons in a 2x2 grid
    const char *button_labels[] = {"Happy :)", "Sad :(", "Wiggle", "Radar"};
    const char *button_commands[] = {"1", "2", "3", "4"};  // Commands to send for each button

    for (int i = 0; i < 4; i++) {
        int row = i / 2;
        int col = i % 2;
        int x_offset = col * (btn_width + 10) + 10;
        int y_offset = row * (btn_height + 10) + 70;

        lv_obj_t *btn = lv_btn_create(_container);
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

} // namespace esp_brookesia::apps::screens
