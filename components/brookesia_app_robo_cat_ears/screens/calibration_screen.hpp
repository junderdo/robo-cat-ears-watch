/*
 * Description: Servo calibration screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <functional>
#include "lvgl.h"
#include "bluetooth_service.hpp"
#include "calibration_service.hpp"

namespace esp_brookesia::apps::screens {

class CalibrationScreen {
public:
    CalibrationScreen(lv_obj_t *parent);
    ~CalibrationScreen();

    lv_obj_t *getContainer() const { return _container; }
    void show();
    void hide();
    
    // Callback when OK button is clicked
    void setOnConfirmed(std::function<void(int left_azi, int left_lat, int right_azi, int right_lat)> callback) {
        _on_confirmed = callback;
    }

    void setOnCalibrationChanged(std::function<void(int left_azi, int left_lat, int right_azi, int right_lat)> callback) {
        _on_calibration_changed = callback;
    }

    // Callbacks for modal visibility changes
    void setOnModalShown(std::function<void()> callback) {
        _on_modal_shown = callback;
    }
    
    void setOnModalHidden(std::function<void()> callback) {
        _on_modal_hidden = callback;
    }

    void loadCalibrationData();
    void saveCalibrationDataToDevice();

private:
    lv_obj_t *_container;
    lv_obj_t *_panel;
    lv_obj_t *_left_azi_slider;
    lv_obj_t *_left_lat_slider;
    lv_obj_t *_right_azi_slider;
    lv_obj_t *_right_lat_slider;
    lv_obj_t *_cancel_btn;
    lv_obj_t *_ok_btn;

    bool _loading_from_device;
    
    // Store initial values for cancel functionality
    int _initial_left_azi;
    int _initial_left_lat;
    int _initial_right_azi;
    int _initial_right_lat;

    // Store current values for real-time updates
    int _current_left_azi;
    int _current_left_lat;
    int _current_right_azi;
    int _current_right_lat;
    
    std::function<void(int left_azi, int left_lat, int right_azi, int right_lat)> _on_confirmed;
    std::function<void(int left_azi, int left_lat, int right_azi, int right_lat)> _on_calibration_changed;
    std::function<void()> _on_modal_shown;
    std::function<void()> _on_modal_hidden;

    // Add a member variable for the debounce timer
    lv_timer_t *_calibration_debounce_timer;
};

} // namespace esp_brookesia::apps::screens
