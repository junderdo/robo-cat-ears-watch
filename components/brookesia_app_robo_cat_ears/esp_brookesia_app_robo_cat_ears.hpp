/*
 * Description: Robo cat ears controller app header
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <vector>
#include <string>
#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "screens/scan_screen.hpp"
#include "screens/animate_screen.hpp"
#include "screens/glow_screen.hpp"
#include "screens/pick_color_screen.hpp"
#include "bluetooth_service.hpp"

namespace esp_brookesia::apps {

// Use the BleDevice from the service namespace
using robo_cat_ears::BleDevice;

/**
 * @brief Robo Cat Ears app with Bluetooth LE scanning functionality
 *
 */
class RoboCatEars: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of RoboCatEars
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     * @return Pointer to the singleton instance
     */
    static RoboCatEars *requestInstance(bool use_status_bar = true, bool use_navigation_bar = false);

    /**
     * @brief Destructor for the phone app
     *
     */
    ~RoboCatEars();



protected:
    /**
     * @brief Private constructor to enforce singleton pattern
     *
     * @param use_status_bar Flag to show the status bar
     * @param use_navigation_bar Flag to show the navigation bar
     */
    RoboCatEars(bool use_status_bar, bool use_navigation_bar);

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

    /**
     * @brief Called when the app starts to initialize
     *
     * @return true if successful, otherwise false
     */
    bool init(void) override;

    /**
     * @brief Called when the app starts to deinitialize
     *
     * @return true if successful, otherwise false
     */
    bool deinit(void) override;

private:
    /**
     * @brief Update the UI list with discovered devices
     */
    void updateDeviceList();

    /**
     * @brief Update the connection status UI
     */
    void updateConnectionStatus();

    /**
     * @brief Update the Bluetooth status bar icon
     *
     * @param connected true if connected, false if disconnected
     */
    void updateBluetoothStatusIcon(bool connected);

    /**
     * @brief Switch to a specific screen
     *
     * @param screen_index 0 for scan screen, 1 for control screen
     */
    void switchToScreen(int screen_index);

    /**
     * @brief Start the auto-reconnection timer
     */
    void startReconnectionTimer();

    /**
     * @brief Stop the auto-reconnection timer
     */
    void stopReconnectionTimer();

    static RoboCatEars *_instance;
    screens::ScanScreen *_scan_screen;
    screens::AnimateScreen *_animate_screen;
    screens::GlowScreen *_glow_screen;
    screens::PickColorScreen *_pick_color_screen;
    int _current_screen;
    robo_cat_ears::BluetoothService *_bluetooth_service;
    
    // Auto-reconnection support
    lv_timer_t *_reconnection_timer;
    std::string _last_disconnected_address;
    
    // Modal state tracking
    bool _modal_is_open;
};

} // namespace esp_brookesia::apps
