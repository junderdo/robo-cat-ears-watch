/*
 * Description: Bluetooth LE service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "bluetooth_service.hpp"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>

static const char *TAG = "BluetoothService";

namespace robo_cat_ears {

// DataPacket implementation
std::string DataPacket::pack() const
{
    std::string packed;
    packed.reserve(1 + data.length());  // type byte + data
    
    // Pack type byte
    packed.push_back(static_cast<uint8_t>(type));
    
    // Pack data
    packed.append(data);
    
    return packed;
}

bool DataPacket::unpack(const std::string &packed, DataPacket &packet)
{
    if (packed.empty()) {
        return false;
    }
    
    // Unpack type byte
    packet.type = static_cast<DataType>(packed[0]);
    
    // Validate type
    if (packet.type != DataType::ANIMATION && packet.type != DataType::LIGHTING) {
        return false;
    }
    
    // Unpack data (rest of the string)
    if (packed.length() > 1) {
        packet.data = packed.substr(1);
    } else {
        packet.data = "";
    }
    
    return true;
}

BluetoothService *BluetoothService::_instance = nullptr;

BluetoothService *BluetoothService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new BluetoothService();
    }
    return _instance;
}

BluetoothService::BluetoothService()
    : _ble_initialized(false)
    , _scanning(false)
    , _connected(false)
    , _connected_address("")
    , _connected_device_name("")
    , _connected_address_type(BLE_ADDR_TYPE_PUBLIC)
    , _conn_id(0)
    , _gattc_if(ESP_GATT_IF_NONE)
    , _char_handle_abf1(0)
    , _char_properties_abf1(0)
    , _char_handle_abf2(0)
    , _char_properties_abf2(0)
    , _service_discovered(false)
    , _mtu_configured(false)
    , _last_connected_address("")
    , _last_connected_address_type(BLE_ADDR_TYPE_PUBLIC)
    , _last_connected_name("")
    , _auto_reconnect_attempted(false)
    , _device_discovered_callback(nullptr)
    , _device_list_updated_callback(nullptr)
    , _connection_status_callback(nullptr)
    , _disconnection_callback(nullptr)
    , _scanning_status_callback(nullptr)
    , _service_ready_callback(nullptr)
    , _read_data_callback(nullptr)
    , _read_pending(false)
    , _read_complete(false)
    , _read_success(false)
    , _read_length(0)
    , _read_event_count(0)
{
}

BluetoothService::~BluetoothService()
{
    deinit();
}

bool BluetoothService::init()
{
    if (_ble_initialized) {
        ESP_LOGD(TAG, "BLE already initialized");
        return true;
    }

    ESP_LOGD(TAG, "Initializing BLE");

    // Initialize NVS (if not already initialized, we just log any errors but don't fail)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase, erasing...");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "NVS erase failed: %s (continuing anyway)", esp_err_to_name(ret));
        } else {
            ret = nvs_flash_init();
        }
    }
    // Log if there was an error, but don't fail - NVS might already be initialized
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "NVS init status: %s (may already be initialized)", esp_err_to_name(ret));
    }

    // Release classic BT memory (we only need BLE) - ignore if already released
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "BT classic mem release failed: %s", esp_err_to_name(ret));
    }

    // Check if controller is already initialized
    ret = esp_bt_controller_get_status();
    if (ret == ESP_BT_CONTROLLER_STATUS_IDLE) {
        // Initialize BT controller
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret) {
            ESP_LOGE(TAG, "Initialize controller failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Enable BT controller if not enabled
    ret = esp_bt_controller_get_status();
    if (ret != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret) {
            ESP_LOGE(TAG, "Enable controller failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Initialize Bluedroid if not already initialized
    ret = esp_bluedroid_get_status();
    if (ret == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        ret = esp_bluedroid_init();
        if (ret) {
            ESP_LOGE(TAG, "Init bluedroid failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Enable Bluedroid if not enabled
    ret = esp_bluedroid_get_status();
    if (ret != ESP_BLUEDROID_STATUS_ENABLED) {
        ret = esp_bluedroid_enable();
        if (ret) {
            ESP_LOGE(TAG, "Enable bluedroid failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Register GAP callback
    ret = esp_ble_gap_register_callback(gapEventHandler);
    if (ret && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "GAP register error: %s", esp_err_to_name(ret));
        return false;
    }

    // Register GATT client for connections
    ret = esp_ble_gattc_register_callback([](esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
        BluetoothService *service = BluetoothService::getInstance();
        if (!service) return;

        switch (event) {
        case ESP_GATTC_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                service->_gattc_if = gattc_if;
                ESP_LOGI(TAG, "GATT client registered, gattc_if: %d", gattc_if);
            } else {
                ESP_LOGE(TAG, "GATT client registration failed, status: %d", param->reg.status);
            }
            break;
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) {
                service->_connected = true;
                service->_conn_id = param->open.conn_id;
                ESP_LOGI(TAG, "Connected to device, conn_id: %d", service->_conn_id);
                
                // Notify connection status via callback
                if (service->_connection_status_callback) {
                    service->_connection_status_callback(true, service->_connected_device_name, service->_connected_address);
                }
                
                // Save to NVS for auto-reconnect
                service->saveLastConnectedDevice();
                
                // Request MTU negotiation for better throughput
                ESP_LOGI(TAG, "Requesting MTU negotiation");
                esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, param->open.conn_id);
                if (mtu_ret != ESP_OK) {
                    ESP_LOGW(TAG, "MTU request failed: %s", esp_err_to_name(mtu_ret));
                }
                
                // Start service discovery
                ESP_LOGI(TAG, "Starting service search");
                esp_ble_gattc_search_service(gattc_if, param->open.conn_id, nullptr);
            } else {
                ESP_LOGE(TAG, "Connection failed, status: %d", param->open.status);
                service->_connected = false;
                
                // Notify connection status via callback
                if (service->_connection_status_callback) {
                    service->_connection_status_callback(false, "", "");
                }
            }
            break;
        case ESP_GATTC_CLOSE_EVT: {
            ESP_LOGI(TAG, "ESP_GATTC_CLOSE_EVT: Disconnected from device (conn_id=%d, reason=%d)", 
                          service->_conn_id, param->close.reason);
            
            // Save device info before clearing for disconnection callback
            std::string disconnected_device_name = service->_connected_device_name;
            std::string disconnected_address = service->_connected_address;
            
            service->_connected = false;
            service->_conn_id = 0;
            service->_connected_address = "";
            service->_connected_device_name = "";
            service->_connected_address_type = BLE_ADDR_TYPE_PUBLIC;
            service->_char_handle_abf1 = 0;
            service->_char_properties_abf1 = 0;
            service->_char_handle_abf2 = 0;
            service->_char_properties_abf2 = 0;
            service->_service_discovered = false;
            service->_mtu_configured = false;
            
            // Notify connection status via callback
            if (service->_connection_status_callback) {
                service->_connection_status_callback(false, "", "");
            }
            
            // Notify disconnection via dedicated callback
            if (service->_disconnection_callback) {
                service->_disconnection_callback(disconnected_device_name, disconnected_address);
            }
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT:
            if (param->search_cmpl.status == ESP_GATT_OK) {
                // NO LOGGING - causes heap corruption in BLE context
                
                // Target characteristics: ABF1 (write) and ABF2 (read)
                const uint16_t target_uuid16_abf1 = 0xABF1;
                const uint16_t target_uuid16_abf2 = 0xABF2;
                uint8_t target_uuid128_abf1[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 
                                                    0x00, 0x10, 0x00, 0x00, 0xF1, 0xAB, 0x00, 0x00};
                uint8_t target_uuid128_abf2[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 
                                                    0x00, 0x10, 0x00, 0x00, 0xF2, 0xAB, 0x00, 0x00};
                
                // Get all characteristics
                uint16_t count = 0;
                esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if, 
                                                                         param->search_cmpl.conn_id,
                                                                         ESP_GATT_DB_CHARACTERISTIC,
                                                                         0, 0xFFFF,
                                                                         ESP_GATT_ILLEGAL_HANDLE,
                                                                         &count);
                
                if (status == ESP_GATT_OK && count > 0) {
                    // Allocate memory for characteristics
                    esp_gattc_char_elem_t *char_elems = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                    if (char_elems) {
                        status = esp_ble_gattc_get_all_char(gattc_if,
                                                            param->search_cmpl.conn_id,
                                                            0, 0xFFFF,
                                                            char_elems,
                                                            &count,
                                                            0);
                        
                        if (status == ESP_GATT_OK) {
                            // Search for ABF1 and ABF2 characteristics
                            for (int i = 0; i < count; i++) {
                                if (char_elems[i].uuid.len == ESP_UUID_LEN_16) {
                                    // Check for ABF1 (data receive - write)
                                    if (char_elems[i].uuid.uuid.uuid16 == target_uuid16_abf1) {
                                        service->_char_handle_abf1 = char_elems[i].char_handle;
                                        service->_char_properties_abf1 = char_elems[i].properties;
                                    }
                                    // Check for ABF2 (data notify - read)
                                    else if (char_elems[i].uuid.uuid.uuid16 == target_uuid16_abf2) {
                                        service->_char_handle_abf2 = char_elems[i].char_handle;
                                        service->_char_properties_abf2 = char_elems[i].properties;
                                    }
                                } else if (char_elems[i].uuid.len == ESP_UUID_LEN_128) {
                                    // Check for ABF1 (128-bit UUID)
                                    if (memcmp(char_elems[i].uuid.uuid.uuid128, target_uuid128_abf1, 16) == 0) {
                                        service->_char_handle_abf1 = char_elems[i].char_handle;
                                        service->_char_properties_abf1 = char_elems[i].properties;
                                    }
                                    // Check for ABF2 (128-bit UUID)
                                    else if (memcmp(char_elems[i].uuid.uuid.uuid128, target_uuid128_abf2, 16) == 0) {
                                        service->_char_handle_abf2 = char_elems[i].char_handle;
                                        service->_char_properties_abf2 = char_elems[i].properties;
                                    }
                                }
                            }
                        }
                        free(char_elems);
                    }
                }
                
                // Mark service as discovered if both ABF1 and ABF2 were found
                if (service->_char_handle_abf1 != 0 && service->_char_handle_abf2 != 0) {
                    service->_service_discovered = true;
                    
                    // Only notify service ready when BOTH service discovery AND MTU are complete
                    if (service->_mtu_configured && service->_service_ready_callback) {
                        service->_service_ready_callback();
                    }
                }
            }
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            // NO LOGGING - causes heap corruption in BLE context
            service->_mtu_configured = true;
            
            // Only notify service ready when BOTH service discovery AND MTU are complete
            if (service->_service_discovered && service->_service_ready_callback) {
                service->_service_ready_callback();
            }
            break;
        case ESP_GATTC_READ_CHAR_EVT:
            service->_read_event_count++;  // Debug: track that event was received
            
            // Only process if this is a response for our pending read request from ABF2
            if (service->_read_pending && param->read.handle == service->_char_handle_abf2) {
                if (param->read.status == ESP_GATT_OK && param->read.value_len > 0) {
                    // Copy to internal buffer - no callbacks, no logging, no allocations
                    size_t len = (param->read.value_len <= sizeof(service->_read_buffer)) ? 
                                 param->read.value_len : sizeof(service->_read_buffer);
                    for (size_t i = 0; i < len; i++) {
                        service->_read_buffer[i] = param->read.value[i];
                    }
                    service->_read_length = len;
                    service->_read_success = true;
                } else {
                    service->_read_success = false;
                    service->_read_length = 0;
                }
                service->_read_complete = true;
                service->_read_pending = false;
                
                // Invoke callback if provided (safe - just gives semaphore)
                if (service->_read_data_callback) {
                    DataType type = (service->_read_length > 0) ? 
                                   static_cast<DataType>(service->_read_buffer[0]) : DataType::LIGHTING;
                    service->_read_data_callback(service->_read_success, type, 
                                                service->_read_buffer, service->_read_length);
                    service->_read_data_callback = nullptr;  // Clear after invoking
                }
            }
            break;
        default:
            break;
        }
    });

    if (ret && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "GATT client register error: %s", esp_err_to_name(ret));
    }

    // Register GATT client app
    ret = esp_ble_gattc_app_register(0);
    if (ret) {
        ESP_LOGE(TAG, "GATT client app register failed: %s", esp_err_to_name(ret));
    }

    _ble_initialized = true;
    ESP_LOGI(TAG, "BLE initialized successfully");

    return true;
}

void BluetoothService::deinit()
{
    if (!_ble_initialized) {
        return;
    }

    ESP_LOGD(TAG, "Deinitializing BLE");

    // Stop scanning first
    stopScan();

    // If still connected, force disconnect
    if (_connected) {
        ESP_LOGW(TAG, "deinit: Still showing connected, forcing final cleanup");
        _connected = false;
        _conn_id = 0;
        _connected_address = "";
        _connected_device_name = "";
        _connected_address_type = BLE_ADDR_TYPE_PUBLIC;
        _char_handle_abf1 = 0;
        _char_properties_abf1 = 0;
        _char_handle_abf2 = 0;
        _char_properties_abf2 = 0;
        _service_discovered = false;
        _mtu_configured = false;
    }

    // Unregister the GATT client app to clean up state
    if (_gattc_if != ESP_GATT_IF_NONE) {
        ESP_LOGI(TAG, "Unregistering GATT client app (gattc_if=%d)", _gattc_if);
        esp_err_t ret = esp_ble_gattc_app_unregister(_gattc_if);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to unregister GATT client: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "GATT client unregistered successfully");
            // Give the BLE stack time to fully clean up after unregister
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        _gattc_if = ESP_GATT_IF_NONE;
    }

    _ble_initialized = false;
}

bool BluetoothService::startScan()
{
    if (!_ble_initialized) {
        ESP_LOGE(TAG, "BLE not initialized");
        return false;
    }

    if (_scanning) {
        ESP_LOGD(TAG, "Already scanning, stopping previous scan");
        stopScan();
    }

    ESP_LOGI(TAG, "Starting BLE scan (clearing %zu previous devices)", _discovered_devices.size());

    // Clear previous devices
    _discovered_devices.clear();
    
    // Notify device list updated
    if (_device_list_updated_callback) {
        _device_list_updated_callback(_discovered_devices);
    }

    // Set extended scan parameters for BLE 5.0
    static esp_ble_ext_scan_params_t ext_scan_params = {
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
        .cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK | ESP_BLE_GAP_EXT_SCAN_CFG_CODE_MASK,
        .uncoded_cfg = {
            .scan_type = BLE_SCAN_TYPE_ACTIVE,
            .scan_interval = 0x50,
            .scan_window = 0x30,
        },
        .coded_cfg = {
            .scan_type = BLE_SCAN_TYPE_ACTIVE,
            .scan_interval = 0x50,
            .scan_window = 0x30,
        },
    };

    esp_err_t ret = esp_ble_gap_set_ext_scan_params(&ext_scan_params);
    if (ret) {
        ESP_LOGE(TAG, "Set extended scan params error: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

void BluetoothService::stopScan()
{
    if (_scanning) {
        ESP_LOGD(TAG, "Stopping BLE scan");
        esp_ble_gap_stop_ext_scan();
        _scanning = false;
        
        // Notify scanning status
        if (_scanning_status_callback) {
            _scanning_status_callback(false);
        }
    }
}

bool BluetoothService::connectToDevice(const std::string &address)
{
    if (!_ble_initialized) {
        ESP_LOGE(TAG, "BLE not initialized");
        return false;
    }

    // Check if we're still connected
    if (_connected) {
        ESP_LOGW(TAG, "Still connected to previous device, forcing disconnect");
        disconnect();
        // Wait for disconnect to complete
        vTaskDelay(pdMS_TO_TICKS(300));
        
        if (_connected) {
            ESP_LOGE(TAG, "Failed to disconnect from previous device");
            return false;
        }
    }

    // Stop scanning before connecting
    if (_scanning) {
        ESP_LOGI(TAG, "Stopping scan before connection attempt");
        stopScan();
        
        // Wait a bit for scan to fully stop to avoid "Command Disallowed" error
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Connecting to device: %s", address.c_str());

    // Find the device in the discovered list to get its address type and name
    esp_ble_addr_type_t addr_type = BLE_ADDR_TYPE_PUBLIC;  // Default
    std::string device_name = "Unknown";
    
    // First, try to find the device in the discovered list (most up-to-date info)
    auto it = std::find_if(_discovered_devices.begin(), _discovered_devices.end(),
                          [&address](const BleDevice &dev) { return dev.address == address; });
    if (it != _discovered_devices.end()) {
        addr_type = it->address_type;
        device_name = it->name;
        ESP_LOGI(TAG, "Found device in discovered list: %s (%s)", device_name.c_str(), address.c_str());
    } else if (address == _last_connected_address && !_last_connected_name.empty()) {
        // Fall back to saved device info if not in discovered list (e.g., auto-reconnect)
        addr_type = _last_connected_address_type;
        device_name = _last_connected_name;
        ESP_LOGI(TAG, "Using saved device info for reconnection: %s (%s)", device_name.c_str(), address.c_str());
    } else {
        ESP_LOGW(TAG, "Device not in discovered list and no saved info available");
    }

    // Store for later saving to NVS
    _connected_address_type = addr_type;

    // Parse MAC address string to esp_bd_addr_t
    esp_bd_addr_t remote_addr;
    int values[6];
    if (sscanf(address.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) {
            remote_addr[i] = (uint8_t)values[i];
        }
    } else {
        ESP_LOGE(TAG, "Invalid MAC address format");
        return false;
    }

    // Check if GATT client is registered
    if (_gattc_if == ESP_GATT_IF_NONE) {
        ESP_LOGE(TAG, "GATT client not registered");
        return false;
    }

    // Setup connection parameters for 1M PHY
    static esp_ble_conn_params_t conn_params_1m = {
        .scan_interval = 0x50,      // 50ms
        .scan_window = 0x30,        // 30ms
        .interval_min = 0x20,       // 40ms
        .interval_max = 0x40,       // 80ms
        .latency = 0,               // No latency
        .supervision_timeout = 0x1F4, // 5000ms
        .min_ce_len = 0,            // No minimum
        .max_ce_len = 0,            // No maximum
    };

    // Setup connection parameters for BLE 5.0
    esp_ble_gatt_creat_conn_params_t conn_params = {
        .remote_addr_type = addr_type,
        .is_direct = true,
        .is_aux = true,  // Use auxiliary connections for BLE 5.0
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .phy_mask = ESP_BLE_GAP_PHY_1M_PREF_MASK,
        .phy_1m_conn_params = &conn_params_1m,
        .phy_2m_conn_params = nullptr,
        .phy_coded_conn_params = nullptr,
    };
    memcpy(conn_params.remote_bda, remote_addr, sizeof(esp_bd_addr_t));

    ESP_LOGI(TAG, "Attempting GATT connection: remote_addr_type=%d, own_addr_type=%d, is_aux=%d",
                   conn_params.remote_addr_type, conn_params.own_addr_type, conn_params.is_aux);

    // Initiate connection using enhanced API
    esp_err_t ret = esp_ble_gattc_enh_open(_gattc_if, &conn_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open GATT connection: %s", esp_err_to_name(ret));
        return false;
    }

    _connected_address = address;
    _connected_device_name = device_name;
    ESP_LOGI(TAG, "Connection initiated to %s (%s)", device_name.c_str(), address.c_str());

    return true;
}

void BluetoothService::disconnect()
{
    if (_connected && _gattc_if != ESP_GATT_IF_NONE) {
        ESP_LOGI(TAG, "Disconnecting from device, conn_id=%d, gattc_if=%d", _conn_id, _gattc_if);
        esp_err_t ret = esp_ble_gattc_close(_gattc_if, _conn_id);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close GATT connection: %s", esp_err_to_name(ret));
            // Clean up state anyway if close fails
            _connected = false;
            _conn_id = 0;
            _connected_address = "";
            _connected_device_name = "";
            
            // Notify via callback
            if (_connection_status_callback) {
                _connection_status_callback(false, "", "");
            }
        }
    } else {
        ESP_LOGW(TAG, "disconnect() called but not connected (connected=%d, gattc_if=%d, conn_id=%d)", 
                       _connected, _gattc_if, _conn_id);
    }
}

bool BluetoothService::writeDataPacket(const DataPacket &packet)
{
    if (!_connected || _gattc_if == ESP_GATT_IF_NONE) {
        ESP_LOGW(TAG, "Cannot write: not connected");
        return false;
    }
    
    if (!_service_discovered || _char_handle_abf1 == 0) {
        ESP_LOGW(TAG, "Cannot write: ABF1 characteristic not discovered yet");
        return false;
    }
    
    // Pack the data packet
    std::string packed_data = packet.pack();
    
    // Determine data type name for logging
    const char *type_name = "UNKNOWN";
    switch (packet.type) {
        case DataType::ANIMATION:
            type_name = "ANIMATION";
            break;
        case DataType::LIGHTING:
            type_name = "LIGHTING";
            break;
        case DataType::CALIBRATION:
            type_name = "CALIBRATION";
            break;
        default:
            break;
    }

    
    ESP_LOGI(TAG, "Writing DataPacket to ABF1 (0x%04x): type=%s, data_length=%zu, total_length=%zu", 
             _char_handle_abf1, type_name, packet.data.length(), packed_data.length());
    
    // esp_ble_gattc_write_char should copy the data internally, but let's be safe
    // and keep a local copy on the stack
    uint8_t write_buffer[512];  // Max MTU size
    if (packed_data.length() > sizeof(write_buffer)) {
        ESP_LOGE(TAG, "Data too large: %zu bytes (max %zu)", packed_data.length(), sizeof(write_buffer));
        return false;
    }
    std::memcpy(write_buffer, packed_data.c_str(), packed_data.length());
    
    // Write packed data to ABF1 characteristic
    esp_err_t ret = esp_ble_gattc_write_char(
        _gattc_if,
        _conn_id,
        _char_handle_abf1,
        packed_data.length(),
        write_buffer,
        ESP_GATT_WRITE_TYPE_NO_RSP,
        ESP_GATT_AUTH_REQ_NONE
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write characteristic: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "Write completed successfully");
    return true;
}

bool BluetoothService::readDataPacket(DataType data_type, ReadDataCallback callback)
{
    ESP_LOGI(TAG, "readDataPacket called: connected=%d, gattc_if=%d, service_discovered=%d, abf2_handle=0x%04x",
             _connected, _gattc_if, _service_discovered, _char_handle_abf2);
    ESP_LOGI(TAG, "ABF2 characteristic properties: 0x%02x (READ=%d, NOTIFY=%d, INDICATE=%d)",
             _char_properties_abf2,
             (_char_properties_abf2 & ESP_GATT_CHAR_PROP_BIT_READ) ? 1 : 0,
             (_char_properties_abf2 & ESP_GATT_CHAR_PROP_BIT_NOTIFY) ? 1 : 0,
             (_char_properties_abf2 & ESP_GATT_CHAR_PROP_BIT_INDICATE) ? 1 : 0);
    
    if (!_connected || _gattc_if == ESP_GATT_IF_NONE) {
        ESP_LOGW(TAG, "Cannot read: not connected");
        return false;
    }
    
    if (!_service_discovered || _char_handle_abf2 == 0) {
        ESP_LOGW(TAG, "Cannot read: ABF2 characteristic not discovered yet");
        return false;
    }
    
    // Store callback for when read completes
    _read_data_callback = callback;
    
    // Reset read state
    _read_pending = true;
    _read_complete = false;
    _read_success = false;
    _read_length = 0;
    _read_event_count = 0;  // Reset debug counter
    
    // Determine data type name for logging
    const char *type_name = (data_type == DataType::ANIMATION) ? "ANIMATION" : "LIGHTING";
    
    ESP_LOGI(TAG, "Reading DataPacket from ABF2 (0x%04x): type=%s", 
             _char_handle_abf2, type_name);
    ESP_LOGI(TAG, "Read request params: gattc_if=%d, conn_id=%d, char_handle=0x%04x",
             _gattc_if, _conn_id, _char_handle_abf2);
    
    // Initiate read from ABF2 characteristic (data notify)
    esp_err_t ret = esp_ble_gattc_read_char(
        _gattc_if,
        _conn_id,
        _char_handle_abf2,
        ESP_GATT_AUTH_REQ_NONE
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read characteristic: %s (0x%x)", esp_err_to_name(ret), ret);
        _read_pending = false;
        _read_data_callback = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "Read characteristic request sent successfully");
    return true;
}

bool BluetoothService::saveLastConnectedDevice()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("robo_cat_ears", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    // Save address
    err = nvs_set_str(nvs_handle, "last_addr", _connected_address.c_str());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save address to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Save address type
    err = nvs_set_u8(nvs_handle, "last_addr_type", (uint8_t)_connected_address_type);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save address type to NVS: %s", esp_err_to_name(err));
    }

    // Save name
    err = nvs_set_str(nvs_handle, "last_name", _connected_device_name.c_str());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save name to NVS: %s", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Saved last connected device to NVS: %s (%s)", 
                   _connected_device_name.c_str(), _connected_address.c_str());
    return true;
}

bool BluetoothService::loadLastConnectedDevice()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("robo_cat_ears", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "Failed to open NVS for reading (no saved device): %s", esp_err_to_name(err));
        return false;
    }

    // Load address
    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, "last_addr", nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) {
        ESP_LOGD(TAG, "No last connected address in NVS");
        nvs_close(nvs_handle);
        return false;
    }

    char* address = (char*)malloc(required_size);
    err = nvs_get_str(nvs_handle, "last_addr", address, &required_size);
    if (err == ESP_OK) {
        _last_connected_address = std::string(address);
    }
    free(address);

    // Load address type
    uint8_t addr_type_u8 = 0;
    err = nvs_get_u8(nvs_handle, "last_addr_type", &addr_type_u8);
    if (err == ESP_OK) {
        _last_connected_address_type = (esp_ble_addr_type_t)addr_type_u8;
    }

    // Load name
    required_size = 0;
    err = nvs_get_str(nvs_handle, "last_name", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* name = (char*)malloc(required_size);
        err = nvs_get_str(nvs_handle, "last_name", name, &required_size);
        if (err == ESP_OK) {
            _last_connected_name = std::string(name);
        }
        free(name);
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Loaded last connected device from NVS: %s (%s) type=%d", 
                   _last_connected_name.c_str(), _last_connected_address.c_str(), _last_connected_address_type);
    return true;
}

void BluetoothService::attemptAutoReconnect()
{
    if (_last_connected_address.empty()) {
        ESP_LOGD(TAG, "No last connected device to reconnect to");
        return;
    }

    if (_auto_reconnect_attempted) {
        ESP_LOGD(TAG, "Auto-reconnect already attempted");
        return;
    }

    ESP_LOGI(TAG, "Attempting auto-reconnect to %s (%s)", 
                   _last_connected_name.c_str(), _last_connected_address.c_str());

    _auto_reconnect_attempted = true;

    // Wait a bit for BLE stack to be ready
    vTaskDelay(pdMS_TO_TICKS(500));

    // Attempt connection
    connectToDevice(_last_connected_address);
}

void BluetoothService::gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    BluetoothService *service = BluetoothService::getInstance();
    if (service) {
        service->handleGapEvent(event, param);
    }
}

void BluetoothService::handleGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT: {
        if (param->set_ext_scan_params.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Set extended scan params failed, error status = %x", param->set_ext_scan_params.status);
            _scanning = false;
            
            // Notify scanning status
            if (_scanning_status_callback) {
                _scanning_status_callback(false);
            }
        } else {
            ESP_LOGD(TAG, "Extended scan params set, starting scan");
            uint32_t duration = 5000; // Scan for 10 seconds (units of 10ms)
            uint16_t period = 0;       // No periodic scanning
            esp_ble_gap_start_ext_scan(duration, period);
            _scanning = true;
            
            // Notify scanning status
            if (_scanning_status_callback) {
                _scanning_status_callback(true);
            }
        }
        break;
    }
    
    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
        if (param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Extended scan start failed, error status = %x", param->ext_scan_start.status);
            _scanning = false;
            
            // Notify scanning status
            if (_scanning_status_callback) {
                _scanning_status_callback(false);
            }
        } else {
            ESP_LOGD(TAG, "Extended scan started successfully");
        }
        break;

    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT: {
        // Extended advertising report - contains scan results
        esp_ble_gap_ext_adv_report_t *report = &param->ext_adv_report.params;
        
        // Device found
        char addr_str[18];
        snprintf(addr_str, sizeof(addr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 report->addr[0], report->addr[1], report->addr[2],
                 report->addr[3], report->addr[4], report->addr[5]);

        // Extract device name
        std::string device_name = "Unknown";
        uint8_t *adv_data = report->adv_data;
        uint16_t adv_data_len = report->adv_data_len;
        
        if (adv_data_len > 0 && adv_data_len < 64) {
            for (uint16_t i = 0; i < adv_data_len && i < 60; ) {
                uint8_t length = adv_data[i];
                if (length == 0 || i + length >= adv_data_len) break;
                
                uint8_t type = adv_data[i + 1];
                if (type == 0x09 || type == 0x08) { // Complete or shortened local name
                    uint8_t name_len = length - 1;
                    if (name_len > 0 && name_len < 30) {
                        device_name = std::string((char *)&adv_data[i + 2], name_len);
                    }
                    break;
                }
                i += length + 1;
            }
        }

        int rssi = report->rssi;
        esp_ble_addr_type_t addr_type = (esp_ble_addr_type_t)report->addr_type;

        ESP_LOGD(TAG, "Scan found device: %s (%s) addr_type=%d RSSI=%d", 
                      device_name.c_str(), addr_str, addr_type, rssi);

        // Add device to list
        addDevice(device_name, addr_str, addr_type, rssi);
        break;
    }

    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
        if (param->ext_scan_stop.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Extended scan stop failed, error status = %x", param->ext_scan_stop.status);
        } else {
            ESP_LOGD(TAG, "Extended scan stopped successfully");
        }
        _scanning = false;
        
        // Notify scanning status
        if (_scanning_status_callback) {
            _scanning_status_callback(false);
        }
        
        // Notify device list updated
        if (_device_list_updated_callback) {
            _device_list_updated_callback(_discovered_devices);
        }
        break;

    case ESP_GAP_BLE_SCAN_TIMEOUT_EVT:
        ESP_LOGD(TAG, "BLE scan timeout - scan completed");
        _scanning = false;
        
        // Notify scanning status
        if (_scanning_status_callback) {
            _scanning_status_callback(false);
        }
        
        // Notify device list updated
        if (_device_list_updated_callback) {
            _device_list_updated_callback(_discovered_devices);
        }
        break;

    default:
        break;
    }
}

void BluetoothService::addDevice(const std::string &name, const std::string &address, esp_ble_addr_type_t address_type, int rssi)
{
    // Filter: only accept devices whose name starts with "ROBO_CAT_EARS"
    if (name.find("ROBO_CAT_EARS") != 0) {
        return;
    }

    // Check if device already exists (by address)
    auto it = std::find_if(_discovered_devices.begin(), _discovered_devices.end(),
                          [&address](const BleDevice &dev) { return dev.address == address; });

    if (it != _discovered_devices.end()) {
        // Update existing device
        it->name = name;
        it->rssi = rssi;
        it->address_type = address_type;
    } else {
        // Add new device
        BleDevice device;
        device.name = name;
        device.address = address;
        device.address_type = address_type;
        device.rssi = rssi;
        _discovered_devices.push_back(device);
        
        ESP_LOGD(TAG, "Found matching device: %s (%s) RSSI: %d", name.c_str(), address.c_str(), rssi);
        
        // Notify device discovered
        if (_device_discovered_callback) {
            _device_discovered_callback(device);
        }
    }

    // Notify device list updated
    if (_device_list_updated_callback) {
        _device_list_updated_callback(_discovered_devices);
    }
}

} // namespace robo_cat_ears
