/*
 * Description: Settings screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "lvgl.h"
#include "calibration_screen.hpp"
#include <memory>
#include <functional>

namespace esp_brookesia::apps::screens {

/**
 * @brief Settings screen containing configuration options for the robo cat ears
 */
class SettingsScreen {
public:
    /**
     * @brief Constructor - creates the settings screen UI
     *
     * @param parent_screen The parent LVGL screen object
     */
    SettingsScreen(lv_obj_t *parent_screen);

    /**
     * @brief Destructor
     */
    ~SettingsScreen();

    /**
     * @brief Get the container object for this screen
     *
     * @return The LVGL container object
     */
    lv_obj_t *getContainer() const { return _container; }

    /**
     * @brief Get the status label
     *
     * @return The LVGL label object
     */
    lv_obj_t *getStatusLabel() const { return _status_label; }

    /**
     * @brief Set the callback for when servo calibration is confirmed
     *
     * @param callback Function to call with calibration values (left_azi, left_lat, right_azi, right_lat)
     */
    void setOnServoCalibrationConfirmed(std::function<void(int, int, int, int)> callback) {
        _on_servo_calib_confirmed = callback;
    }

private:
    lv_obj_t *_container;
    lv_obj_t *_status_label;
    std::unique_ptr<CalibrationScreen> _calibration_screen;
    std::function<void(int, int, int, int)> _on_servo_calib_confirmed;
};

} // namespace esp_brookesia::apps::screens
