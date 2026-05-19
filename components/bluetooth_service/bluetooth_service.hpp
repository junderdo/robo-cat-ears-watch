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
 * @brief Data type enumeration for packed data transmission
 */
enum class DataType : uint8_t {
    ANIMATION = 0x01,
    LIGHTING = 0x02
};

/**
 * @brief Packed data structure for transmission over ABF1 characteristic
 * 
 * This structure packs a type byte followed by the data payload.
 * Format: [type:1byte][data:variable]
 * Max total size: 512 bytes (BLE MTU limit)
 */
struct DataPacket {
    DataType type;
    std::string data;  // Payload data
    
    /**
     * @brief Pack the data packet into a byte array for transmission
     * 
     * @return Packed byte string ready for BLE transmission
     */
    std::string pack() const;
    
    /**
     * @brief Unpack a byte array into a DataPacket
     * 
     * @param packed Packed byte string from BLE
     * @param packet Output packet structure
     * @return true if unpacking successful, false otherwise
     */
    static bool unpack(const std::string &packed, DataPacket &packet);
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
    using DisconnectionCallback = std::function<void(const std::string &device_name, const std::string &address)>;
    using ScanningStatusCallback = std::function<void(bool scanning)>;
    using ServiceReadyCallback = std::function<void()>;
    using ReadDataCallback = std::function<void(bool success, DataType type, const uint8_t *data, size_t length)>;

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
     * @brief Write a data packet to the ABF1 characteristic
     *
     * @param packet Data packet containing type and data
     * @return true if write initiated successfully, otherwise false
     */
    bool writeDataPacket(const DataPacket &packet);

    /**
     * @brief Read a data packet from the ABF1 characteristic
     *
     * @param data_type Type of data to request from the device
     * @param callback Callback to invoke when data is received
     * @return true if read initiated successfully, otherwise false
     */
    bool readDataPacket(DataType data_type, ReadDataCallback callback);

    /**
     * @brief Get the ABF1 characteristic handle
     */
    uint16_t getCharHandleABF1() const { return _char_handle_abf1; }
    uint16_t getCharHandleABF2() const { return _char_handle_abf2; }

    /**
     * @brief Check if services are discovered
     */
    bool isServiceDiscovered() const { return _service_discovered; }

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
     * @brief Set disconnection callback (called when connection is lost)
     */
    void setDisconnectionCallback(DisconnectionCallback callback) { _disconnection_callback = callback; }

    /**
     * @brief Set scanning status callback
     */
    void setScanningStatusCallback(ScanningStatusCallback callback) { _scanning_status_callback = callback; }

    /**
     * @brief Set service ready callback (called after service discovery completes and device is ready for communication)
     */
    void setServiceReadyCallback(ServiceReadyCallback callback) { _service_ready_callback = callback; }

    // Read state accessors (for polling-based read)
    bool isReadComplete() const { return _read_complete; }
    bool wasReadSuccessful() const { return _read_success; }
    size_t getReadLength() const { return _read_length; }
    const uint8_t* getReadBuffer() const { return _read_buffer; }
    uint32_t getReadEventCount() const { return _read_event_count; }  // Debug

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
    uint16_t _char_handle_abf1;  // Data receive characteristic (write)
    uint8_t _char_properties_abf1;  // Properties of ABF1 characteristic
    uint16_t _char_handle_abf2;  // Data notify characteristic (read)
    uint8_t _char_properties_abf2;  // Properties of ABF2 characteristic
    bool _service_discovered;
    bool _mtu_configured;

    // Auto-reconnect support
    std::string _last_connected_address;
    esp_ble_addr_type_t _last_connected_address_type;
    std::string _last_connected_name;
    bool _auto_reconnect_attempted;

    // Callbacks
    DeviceDiscoveredCallback _device_discovered_callback;
    DeviceListUpdatedCallback _device_list_updated_callback;
    ConnectionStatusCallback _connection_status_callback;
    DisconnectionCallback _disconnection_callback;
    ScanningStatusCallback _scanning_status_callback;
    ServiceReadyCallback _service_ready_callback;
    ReadDataCallback _read_data_callback;
    
    // Simple buffer for read results (avoid callback issues in BLE context)
    volatile bool _read_pending;
    volatile bool _read_complete;
    volatile bool _read_success;
    uint8_t _read_buffer[256];
    volatile size_t _read_length;
    volatile uint32_t _read_event_count;  // Debug: count read events received
};

} // namespace robo_cat_ears
