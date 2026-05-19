/*
 * Description: Settings screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "settings_screen.hpp"
#include "calibration_screen.hpp"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:SettingsScreen"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::screens {

SettingsScreen::SettingsScreen(lv_obj_t *parent_screen)
    : _container(nullptr),
    _status_label(nullptr),
    _calibration_screen(nullptr),
    _on_servo_calib_confirmed(nullptr)
{
    ESP_UTILS_LOGD("Creating settings screen");

    // Create a container for the settings screen
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

    // Create "Servo Calibration" button
    lv_obj_t *servo_calib_btn = lv_btn_create(_container);
    lv_obj_set_size(servo_calib_btn, 260, 80);
    lv_obj_align(servo_calib_btn, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *servo_label = lv_label_create(servo_calib_btn);
    lv_label_set_text(servo_label, "Servo Calibration");
    lv_obj_set_style_text_font(servo_label, &lv_font_montserrat_24, 0);
    lv_obj_center(servo_label);

    // Create calibration screen (modal)
    _calibration_screen = std::make_unique<CalibrationScreen>(parent_screen);
    
    // Wire up calibration screen callbacks
    _calibration_screen->setOnCalibrationChanged([this](int left_azi, int left_lat, int right_azi, int right_lat) {
        ESP_UTILS_LOGI("Calibration changed: Left Azi=%d, Left Lat=%d, Right Azi=%d, Right Lat=%d",
                       left_azi, left_lat, right_azi, right_lat);
        // Send an update packet to the device with the new calibration values
        robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
        if (!bt_service || !bt_service->isConnected()) {
            ESP_UTILS_LOGW("Cannot send update: Not connected to device");
            return;
        }

        robo_cat_ears::CalibrationService *calibration_service = robo_cat_ears::CalibrationService::getInstance();
        if (!calibration_service) {
            ESP_UTILS_LOGE("Cannot send update: Calibration service not available");
            return;
        }

        // Ensure the calibration service is initialized before sending updates
        if (!calibration_service->init()) {
            ESP_UTILS_LOGE("Failed to initialize calibration service");
            return;
        }

        robo_cat_ears::CalibrationData data;
        data.left_azi = left_azi;
        data.left_lat = left_lat;
        data.right_azi = right_azi;
        data.right_lat = right_lat;

        if (!calibration_service->writeCalibrationData(&data)) {
            ESP_UTILS_LOGE("Failed to send calibration update to device");
        } else {
            ESP_UTILS_LOGI("Calibration update sent successfully");
        }
    });

    _calibration_screen->setOnConfirmed([this](int left_azi, int left_lat, int right_azi, int right_lat) {
        ESP_UTILS_LOGI("Servo calibration confirmed: Left Azi=%d, Left Lat=%d, Right Azi=%d, Right Lat=%d",
                       left_azi, left_lat, right_azi, right_lat);
        if (_on_servo_calib_confirmed) {
            _on_servo_calib_confirmed(left_azi, left_lat, right_azi, right_lat);
        }
    });
    
    _calibration_screen->setOnModalShown([this]() {
        ESP_UTILS_LOGD("Calibration modal shown");
        // Disable parent gestures when modal is open
        lv_obj_add_flag(_container, LV_OBJ_FLAG_GESTURE_BUBBLE);  // Keep bubbling to parent
    });
    
    _calibration_screen->setOnModalHidden([this]() {
        ESP_UTILS_LOGD("Calibration modal hidden");
    });

    // Add event handler for servo calibration button
    lv_obj_add_event_cb(servo_calib_btn, [](lv_event_t *e) {
        // Ignore click if a gesture (swipe) was detected
        lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (gesture_dir != LV_DIR_NONE) {
            return; // Swipe detected, don't process as click
        }

        SettingsScreen *screen = (SettingsScreen *)lv_event_get_user_data(e);
        if (screen && screen->_calibration_screen) {
            ESP_UTILS_LOGD("Showing servo calibration modal");
            screen->_calibration_screen->show();
        }
    }, LV_EVENT_CLICKED, this);

    ESP_UTILS_LOGD("Settings screen created successfully");
}

SettingsScreen::~SettingsScreen()
{
    // LVGL objects are automatically cleaned up when parent is deleted
}

} // namespace esp_brookesia::apps::screens
