/*
 * Description: System Info app header - displays AXP2101 PMU data
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief System Info app displays AXP2101 chip data
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
    static SystemInfo *_instance;
    
    lv_obj_t *_info_label;  // Label to display system information
};

} // namespace esp_brookesia::apps
