/*
 * Description: Scan screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "scan_screen.hpp"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:ScanScreen"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::screens {

ScanScreen::ScanScreen(lv_obj_t *parent_screen,
                       std::function<void()> on_scan_clicked,
                       std::function<void()> on_disconnect_clicked,
                       std::function<void(const char*)> on_device_clicked)
    : _container(nullptr),
      _status_label(nullptr),
      _device_list(nullptr),
      _scan_btn(nullptr),
      _disconnect_btn(nullptr),
      _on_scan_clicked(on_scan_clicked),
      _on_disconnect_clicked(on_disconnect_clicked),
      _on_device_clicked(on_device_clicked)
{
    ESP_UTILS_LOGD("Creating scan screen");

    // Create a container for the scan screen
    _container = lv_obj_create(parent_screen);
    lv_obj_set_size(_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);

    // Create a connection status label (larger, no title)
    _status_label = lv_label_create(_container);
    lv_label_set_text(_status_label, "Not connected");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_status_label, LV_ALIGN_TOP_MID, 0, 15);

    // Create a list to display BLE devices
    _device_list = lv_list_create(_container);
    lv_obj_set_size(_device_list, lv_pct(90), lv_pct(50));
    lv_obj_align(_device_list, LV_ALIGN_CENTER, 0, 0);
    
    // Allow gestures to bubble up from the list for swipe navigation
    lv_obj_add_flag(_device_list, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Add initial message
    lv_obj_t *btn = lv_list_add_btn(_device_list, LV_SYMBOL_REFRESH, "Scanning for ears...");
    lv_obj_set_style_text_color(btn, lv_color_hex(0x808080), 0);

    // Set user data for device click callback
    lv_obj_set_user_data(_device_list, this);

    // Create a scan button (centered at bottom, shown when not connected)
    _scan_btn = lv_btn_create(_container);
    lv_obj_set_size(_scan_btn, 280, 80);
    lv_obj_align(_scan_btn, LV_ALIGN_BOTTOM_MID, 0, -24);

    lv_obj_t *scan_label = lv_label_create(_scan_btn);
    lv_label_set_text(scan_label, LV_SYMBOL_REFRESH " Scan for Ears");
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_24, 0);
    lv_obj_center(scan_label);

    // Add event handler for scan button
    lv_obj_add_event_cb(_scan_btn, [](lv_event_t *e) {
        // Ignore click if a gesture (swipe) was detected
        lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (gesture_dir != LV_DIR_NONE) {
            return; // Swipe detected, don't process as click
        }
        
        ScanScreen *screen = (ScanScreen *)lv_event_get_user_data(e);
        if (screen && screen->_on_scan_clicked) {
            screen->_on_scan_clicked();
        }
    }, LV_EVENT_CLICKED, this);

    // Create a disconnect button (same position as scan, shown when connected)
    _disconnect_btn = lv_btn_create(_container);
    lv_obj_set_size(_disconnect_btn, 280, 80);
    lv_obj_align(_disconnect_btn, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_t *disconnect_label = lv_label_create(_disconnect_btn);
    lv_label_set_text(disconnect_label, LV_SYMBOL_CLOSE " Disconnect");
    lv_obj_set_style_text_font(disconnect_label, &lv_font_montserrat_24, 0);
    lv_obj_center(disconnect_label);

    // Add event handler for disconnect button
    lv_obj_add_event_cb(_disconnect_btn, [](lv_event_t *e) {
        // Ignore click if a gesture (swipe) was detected
        lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (gesture_dir != LV_DIR_NONE) {
            return; // Swipe detected, don't process as click
        }
        
        ScanScreen *screen = (ScanScreen *)lv_event_get_user_data(e);
        if (screen && screen->_on_disconnect_clicked) {
            screen->_on_disconnect_clicked();
        }
    }, LV_EVENT_CLICKED, this);

    // Initially show scan button, hide disconnect button
    lv_obj_add_flag(_disconnect_btn, LV_OBJ_FLAG_HIDDEN);

    ESP_UTILS_LOGD("Scan screen created successfully");
}

ScanScreen::~ScanScreen()
{
    // LVGL objects are automatically cleaned up when parent is deleted
    // No need to explicitly delete child objects
}

} // namespace esp_brookesia::apps::screens
