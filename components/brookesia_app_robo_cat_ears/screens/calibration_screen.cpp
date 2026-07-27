/*
 * Description: Servo calibration screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "calibration_screen.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bluetooth_service.hpp"
#include "calibration_service.hpp"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::screens {

CalibrationScreen::CalibrationScreen(lv_obj_t *parent)
    : _container(nullptr)
    , _panel(nullptr)
    , _left_azi_slider(nullptr)
    , _left_lat_slider(nullptr)
    , _right_azi_slider(nullptr)
    , _right_lat_slider(nullptr)
    , _cancel_btn(nullptr)
    , _ok_btn(nullptr)
    , _initial_left_azi(0)
    , _initial_left_lat(0)
    , _initial_right_azi(0)
    , _initial_right_lat(0)
    , _current_left_azi(0)
    , _current_left_lat(0)
    , _current_right_azi(0)
    , _current_right_lat(0)
    , _on_confirmed(nullptr)
    , _on_calibration_changed(nullptr)
    , _on_modal_shown(nullptr)
    , _on_modal_hidden(nullptr)
    , _calibration_debounce_timer(nullptr)
{
    // Create full-screen container (modal overlay)
    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_container, LV_OPA_90, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_container, LV_OBJ_FLAG_HIDDEN); // Initially hidden

    // Create panel for calibration
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
    lv_label_set_text(title, "Servo Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Create scrollable container for sliders
    lv_obj_t *sliders_container = lv_obj_create(_panel);
    lv_obj_set_size(sliders_container, lv_pct(90), 280);
    lv_obj_align(sliders_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_flex_flow(sliders_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sliders_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(sliders_container, 0, 0);
    lv_obj_set_style_pad_gap(sliders_container, 5, 0);
    lv_obj_set_style_bg_opa(sliders_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sliders_container, 0, 0);
    lv_obj_set_scroll_dir(sliders_container, LV_DIR_VER);

    // Helper struct for slider data
    struct SliderData {
        const char *label;
        lv_obj_t **slider_ptr;
    };

    SliderData sliders[] = {
        {"Left Azi", &_left_azi_slider},
        {"Left Lat", &_left_lat_slider},
        {"Right Azi", &_right_azi_slider},
        {"Right Lat", &_right_lat_slider}
    };

    // Create sliders
    for (int i = 0; i < 4; i++) {
        // Label for slider
        lv_obj_t *slider_label = lv_label_create(sliders_container);
        lv_label_set_text(slider_label, sliders[i].label);
        lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_16, 0);
        lv_obj_set_width(slider_label, lv_pct(100));

        // Slider
        lv_obj_t *slider = lv_slider_create(sliders_container);
        lv_obj_set_size(slider, lv_pct(100), 20);
        lv_slider_set_range(slider, -15, 15);
        lv_slider_set_value(slider, 0, LV_ANIM_OFF);
        
        // Store slider pointer
        *sliders[i].slider_ptr = slider;

        // Add calibration change handler with debouncing
        lv_obj_add_event_cb(slider, [](lv_event_t *e) {
            CalibrationScreen *screen = (CalibrationScreen *)lv_event_get_user_data(e);
            if (!screen) return;

            // Delete existing timer if it exists
            if (screen->_calibration_debounce_timer) {
                lv_timer_del(screen->_calibration_debounce_timer);
                screen->_calibration_debounce_timer = nullptr;
            }

            // Create a new timer that will fire after 300ms
            screen->_calibration_debounce_timer = lv_timer_create([](lv_timer_t *timer) {
                CalibrationScreen *screen = (CalibrationScreen *)lv_timer_get_user_data(timer);
                if (screen && screen->_on_calibration_changed) {
                    int left_azi = lv_slider_get_value(screen->_left_azi_slider);
                    int left_lat = lv_slider_get_value(screen->_left_lat_slider);
                    int right_azi = lv_slider_get_value(screen->_right_azi_slider);
                    int right_lat = lv_slider_get_value(screen->_right_lat_slider);
                    screen->_on_calibration_changed(left_azi, left_lat, right_azi, right_lat);
                }
                // Timer is automatically deleted after one-shot execution
                screen->_calibration_debounce_timer = nullptr;
            }, 300, screen);
            lv_timer_set_repeat_count(screen->_calibration_debounce_timer, 1);  // One-shot timer
        }, LV_EVENT_VALUE_CHANGED, this);
    }

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
        CalibrationScreen *screen = (CalibrationScreen *)lv_event_get_user_data(e);
        if (screen) {
            // Restore initial values
            lv_slider_set_value(screen->_left_azi_slider, screen->_initial_left_azi, LV_ANIM_OFF);
            lv_slider_set_value(screen->_left_lat_slider, screen->_initial_left_lat, LV_ANIM_OFF);
            lv_slider_set_value(screen->_right_azi_slider, screen->_initial_right_azi, LV_ANIM_OFF);
            lv_slider_set_value(screen->_right_lat_slider, screen->_initial_right_lat, LV_ANIM_OFF);
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
        CalibrationScreen *screen = (CalibrationScreen *)lv_event_get_user_data(e);
        if (screen) {
            if (screen->_on_confirmed) {
                int left_azi = lv_slider_get_value(screen->_left_azi_slider);
                int left_lat = lv_slider_get_value(screen->_left_lat_slider);
                int right_azi = lv_slider_get_value(screen->_right_azi_slider);
                int right_lat = lv_slider_get_value(screen->_right_lat_slider);
                screen->_on_confirmed(left_azi, left_lat, right_azi, right_lat);
            }
            screen->hide();
        }
    }, LV_EVENT_CLICKED, this);
}

CalibrationScreen::~CalibrationScreen()
{
    if (_container) {
        lv_obj_del(_container);
        _container = nullptr;
    }
}

void CalibrationScreen::show()
{
    if (_container) {
        // Capture current slider values before showing modal
        _initial_left_azi = lv_slider_get_value(_left_azi_slider);
        _initial_left_lat = lv_slider_get_value(_left_lat_slider);
        _initial_right_azi = lv_slider_get_value(_right_azi_slider);
        _initial_right_lat = lv_slider_get_value(_right_lat_slider);
        
        lv_obj_clear_flag(_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_container);
    }
    
    // Notify that modal is shown
    if (_on_modal_shown) {
        _on_modal_shown();
    }

    loadCalibrationData();
}

void CalibrationScreen::hide()
{
    if (_container) {
        lv_obj_add_flag(_container, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Notify that modal is hidden
    if (_on_modal_hidden) {
        _on_modal_hidden();
    }
}

void CalibrationScreen::loadCalibrationData()
{
    ESP_UTILS_LOGI("Attempting to load calibration data from service");

    // Add connection state check before initiating a new connection
    robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
    if (!bt_service) {
        ESP_UTILS_LOGE("BluetoothService instance is null");
        return;
    }

    // Check if we're connected to a device
    if (!bt_service || !bt_service->isConnected()) {
        ESP_UTILS_LOGD("Not connected to device, skipping calibration data load");
        return;
    }

    // Get calibration service instance
    robo_cat_ears::CalibrationService *calibration_service = robo_cat_ears::CalibrationService::getInstance();
    if (!calibration_service) {
        ESP_UTILS_LOGE("Failed to get calibration service instance");
        return;
    }

    // Initialize calibration service if not already done
    if (!calibration_service->init()) {
        ESP_UTILS_LOGE("Failed to initialize calibration service");
        return;
    }

    // Read calibration data from device
    robo_cat_ears::CalibrationData calibration_data;
    if (!calibration_service->readCalibrationData(&calibration_data, [this](const robo_cat_ears::CalibrationData& data) {
        // This callback runs when actual data is loaded from device (async)
        ESP_UTILS_LOGI("Data loaded callback: Left Azi=%d, Left Lat=%d, Right Azi=%d, Right Lat=%d",
                       data.left_azi, data.left_lat, data.right_azi, data.right_lat);

        // Set flag to prevent saving while loading
        _loading_from_device = true;

        // Apply the loaded data to the UI
        lv_slider_set_value(_left_azi_slider, data.left_azi, LV_ANIM_OFF);
        lv_slider_set_value(_left_lat_slider, data.left_lat, LV_ANIM_OFF);
        lv_slider_set_value(_right_azi_slider, data.right_azi, LV_ANIM_OFF);
        lv_slider_set_value(_right_lat_slider, data.right_lat, LV_ANIM_OFF);

        // Clear flag after loading complete
        _loading_from_device = false;

        ESP_UTILS_LOGI("Calibration data applied to UI from callback");
    })) {
        ESP_UTILS_LOGW("Failed to read calibration data from device");
        return;
    }

    ESP_UTILS_LOGI("Read request sent, waiting for data to arrive asynchronously");
}

void CalibrationScreen::saveCalibrationDataToDevice()
{
    // Don't save if we're currently loading from device
    if (_loading_from_device) {
        return;
    }
    
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
    
    // Get calibration service instance
    robo_cat_ears::CalibrationService *calibration_service = robo_cat_ears::CalibrationService::getInstance();
    if (!calibration_service) {
        ESP_UTILS_LOGE("Failed to get calibration service instance");
        return;
    }
    
    // Initialize calibration service if not already done
    if (!calibration_service->init()) {
        ESP_UTILS_LOGE("Failed to initialize calibration service");
        return;
    }
    
    // Create calibration data from current state
    robo_cat_ears::CalibrationData calibration_data;
    calibration_data.left_azi = _current_left_azi;
    calibration_data.left_lat = _current_left_lat;
    calibration_data.right_azi = _current_right_azi;
    calibration_data.right_lat = _current_right_lat;
    
    // Write to device
    ESP_UTILS_LOGI("Saving calibration data to device: Left Azi=%d, Left Lat=%d, Right Azi=%d, Right Lat=%d",
                   _current_left_azi, _current_left_lat, _current_right_azi, _current_right_lat);
    
    if (!calibration_service->writeCalibrationData(&calibration_data)) {
        ESP_UTILS_LOGE("Failed to write calibration data to device");
    } else {
        ESP_UTILS_LOGI("Successfully saved calibration data to device");
    }
}

} // namespace esp_brookesia::apps::screens
