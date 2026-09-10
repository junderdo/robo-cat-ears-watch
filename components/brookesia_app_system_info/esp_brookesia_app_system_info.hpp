/*
 * Description: System Info app header - displays AXP2101 PMU data
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

// Forward declaration
class SystemStatus;

namespace esp_brookesia::apps {

/**
 * @brief System Info app: AXP2101 diagnostics plus the watch's own settings
 *
 */
class SystemInfo: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of SystemInfo
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     * @return Pointer to the singleton instance
     */
    static SystemInfo *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destructor for the system info app
     *
     */
    ~SystemInfo();

    /**
     * @brief Update the displayed system information
     *        Called by external timer to refresh the display
     */
    void updateSystemInfo();

protected:
    /**
     * @brief Private constructor to enforce singleton pattern
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     */
    SystemInfo(bool use_status_bar, bool use_navigation_bar);

    /**
     * @brief Called when the app starts running
     *
     * @return true if successful, otherwise false
     */
    bool run(void) override;

    /**
     * @brief Called when the app receives a back event
     *
     * @return true if successful, otherwise false
     */
    bool back(void) override;

private:
    /**
     * @brief Build the settings section, currently just screen brightness
     *
     * @param screen Parent object to build into
     * @param y_offset Vertical offset below the diagnostics readout
     */
    void createSettings(lv_obj_t *screen, int16_t y_offset);

    /**
     * @brief Redraw the screen brightness label from the service's value
     */
    void updateScreenBrightnessLabel();

    static SystemInfo *_instance;
    
    lv_obj_t *_info_label;  // Label to display system information
    lv_obj_t *_screen_brightness_label;
    lv_obj_t *_screen_brightness_slider;
    SystemStatus *_system_status;  // System status instance for polling PMU data
};

} // namespace esp_brookesia::apps
