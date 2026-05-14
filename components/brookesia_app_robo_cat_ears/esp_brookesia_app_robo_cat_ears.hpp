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
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "screens/scan_screen.hpp"
#include "screens/animate_screen.hpp"

namespace esp_brookesia::apps {

struct BleDevice {
    std::string name;
    std::string address;
    esp_ble_addr_type_t address_type;
    int rssi;
};

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
    static RoboCatEars *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destructor for the phone app
     *
     */
    ~RoboCatEars();

    /**
     * @brief Handle BLE GAP events (static callback)
     *
     * @param event Event type
     * @param param Event parameters
     */
    static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    /**
     * @brief Add a discovered device to the list
     *
     * @param name Device name
     * @param address Device address
     * @param address_type Address type (PUBLIC or RANDOM)
     * @param rssi Signal strength
     */
    void addDevice(const std::string &name, const std::string &address, esp_ble_addr_type_t address_type, int rssi);

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
     * @brief Initialize Bluetooth LE
     *
     * @return true if successful, otherwise false
     */
    bool initBLE();

    /**
     * @brief Deinitialize Bluetooth LE
     */
    void deinitBLE();

    /**
     * @brief Start BLE scanning
     *
     * @return true if successful, otherwise false
     */
    bool startScan();

    /**
     * @brief Stop BLE scanning
     */
    void stopScan();

    /**
     * @brief Connect to a BLE device
     *
     * @param address Device MAC address
     * @return true if connection initiated successfully, otherwise false
     */
    bool connectToDevice(const std::string &address);

    /**
     * @brief Disconnect from current BLE device
     */
    void disconnect();

    /**
     * @brief Update the UI list with discovered devices
     */
    void updateDeviceList();

    /**
     * @brief Update the connection status UI
     */
    void updateConnectionStatus();

    /**
     * @brief Handle BLE GAP events (instance method)
     *
     * @param event Event type
     * @param param Event parameters
     */
    void handleGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    /**
     * @brief Switch to a specific screen
     *
     * @param screen_index 0 for scan screen, 1 for control screen
     */
    void switchToScreen(int screen_index);

    /**
     * @brief Write data to the BLE characteristic
     *
     * @param data Data string to send
     * @return true if write initiated successfully, otherwise false
     */
    bool writeCharacteristic(const std::string &data);

    /**
     * @brief Save the last connected device to NVS
     *
     * @return true if successful, otherwise false
     */
    bool saveLastConnectedDevice();

    /**
     * @brief Load the last connected device from NVS
     *
     * @return true if successful, otherwise false
     */
    bool loadLastConnectedDevice();

    /**
     * @brief Attempt to reconnect to the last connected device
     */
    void attemptAutoReconnect();

    static RoboCatEars *_instance;
    screens::ScanScreen *_scan_screen;
    screens::AnimateScreen *_animate_screen;
    int _current_screen;
    std::vector<BleDevice> _discovered_devices;
    bool _ble_initialized;
    bool _scanning;
    bool _connected;
    std::string _connected_address;
    std::string _connected_device_name;
    esp_ble_addr_type_t _connected_address_type;
    uint16_t _conn_id;
    esp_gatt_if_t _gattc_if;
    uint16_t _char_handle;
    bool _service_discovered;
    
    // Auto-reconnect support
    std::string _last_connected_address;
    esp_ble_addr_type_t _last_connected_address_type;
    std::string _last_connected_name;
    bool _auto_reconnect_attempted;
};

} // namespace esp_brookesia::apps
