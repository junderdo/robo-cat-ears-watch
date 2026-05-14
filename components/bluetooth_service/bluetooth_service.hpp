/*
 * Description: Bluetooth LE service for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <vector>
#include <string>
#include <functional>
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"

namespace robo_cat_ears {

struct BleDevice {
    std::string name;
    std::string address;
    esp_ble_addr_type_t address_type;
    int rssi;
};

/**
 * @brief Bluetooth LE service for scanning, connecting, and communicating with BLE devices
 */
class BluetoothService {
public:
    /**
     * @brief Callback types for communication with app layer
     */
    using DeviceDiscoveredCallback = std::function<void(const BleDevice &device)>;
    using DeviceListUpdatedCallback = std::function<void(const std::vector<BleDevice> &devices)>;
    using ConnectionStatusCallback = std::function<void(bool connected, const std::string &device_name, const std::string &address)>;
    using ScanningStatusCallback = std::function<void(bool scanning)>;

    /**
     * @brief Get the singleton instance of BluetoothService
     */
    static BluetoothService *getInstance();

    /**
     * @brief Destructor
     */
    ~BluetoothService();

    /**
     * @brief Initialize Bluetooth LE
     *
     * @return true if successful, otherwise false
     */
    bool init();

    /**
     * @brief Deinitialize Bluetooth LE
     */
    void deinit();

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

    /**
     * @brief Get the list of discovered devices
     */
    const std::vector<BleDevice> &getDiscoveredDevices() const { return _discovered_devices; }

    /**
     * @brief Check if currently connected
     */
    bool isConnected() const { return _connected; }

    /**
     * @brief Check if currently scanning
     */
    bool isScanning() const { return _scanning; }

    /**
     * @brief Get connected device name
     */
    const std::string &getConnectedDeviceName() const { return _connected_device_name; }

    /**
     * @brief Get connected device address
     */
    const std::string &getConnectedAddress() const { return _connected_address; }

    /**
     * @brief Get last connected device address
     */
    const std::string &getLastConnectedAddress() const { return _last_connected_address; }

    /**
     * @brief Get last connected device name
     */
    const std::string &getLastConnectedName() const { return _last_connected_name; }

    /**
     * @brief Check if auto-reconnect was attempted
     */
    bool wasAutoReconnectAttempted() const { return _auto_reconnect_attempted; }

    /**
     * @brief Set device discovered callback
     */
    void setDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) { _device_discovered_callback = callback; }

    /**
     * @brief Set device list updated callback
     */
    void setDeviceListUpdatedCallback(DeviceListUpdatedCallback callback) { _device_list_updated_callback = callback; }

    /**
     * @brief Set connection status callback
     */
    void setConnectionStatusCallback(ConnectionStatusCallback callback) { _connection_status_callback = callback; }

    /**
     * @brief Set scanning status callback
     */
    void setScanningStatusCallback(ScanningStatusCallback callback) { _scanning_status_callback = callback; }

    /**
     * @brief Handle BLE GAP events (static callback for ESP-IDF)
     */
    static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

private:
    /**
     * @brief Private constructor to enforce singleton pattern
     */
    BluetoothService();

    /**
     * @brief Handle BLE GAP events (instance method)
     */
    void handleGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    /**
     * @brief Add a discovered device to the list
     */
    void addDevice(const std::string &name, const std::string &address, esp_ble_addr_type_t address_type, int rssi);

    static BluetoothService *_instance;

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

    // Callbacks
    DeviceDiscoveredCallback _device_discovered_callback;
    DeviceListUpdatedCallback _device_list_updated_callback;
    ConnectionStatusCallback _connection_status_callback;
    ScanningStatusCallback _scanning_status_callback;
};

} // namespace robo_cat_ears
