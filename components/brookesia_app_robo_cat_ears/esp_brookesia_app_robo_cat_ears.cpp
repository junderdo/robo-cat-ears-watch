/*
 * Description: Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:RoboCatEars"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_robo_cat_ears.hpp"

#include <cstring>
#include <algorithm>

#define APP_NAME "ROBO_CAT_EARS"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_robo_cat_ears_112_112);

namespace esp_brookesia::apps {

RoboCatEars *RoboCatEars::_instance = nullptr;

RoboCatEars *RoboCatEars::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new RoboCatEars(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

RoboCatEars::RoboCatEars(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, &esp_brookesia_app_icon_launcher_robo_cat_ears_112_112, true, use_status_bar, use_navigation_bar),
    _scan_screen(nullptr),
    _control_screen(nullptr),
    _current_screen(0),
    _ble_initialized(false),
    _scanning(false),
    _connected(false),
    _connected_address(""),
    _connected_device_name(""),
    _connected_address_type(BLE_ADDR_TYPE_PUBLIC),
    _conn_id(0),
    _gattc_if(ESP_GATT_IF_NONE),
    _char_handle(0),
    _service_discovered(false),
    _last_connected_address(""),
    _last_connected_address_type(BLE_ADDR_TYPE_PUBLIC),
    _last_connected_name(""),
    _auto_reconnect_attempted(false)
{
}

RoboCatEars::~RoboCatEars()
{
    deinitBLE();
    
    // Clean up screen objects
    if (_scan_screen) {
        delete _scan_screen;
        _scan_screen = nullptr;
    }
    if (_control_screen) {
        delete _control_screen;
        _control_screen = nullptr;
    }
}

bool RoboCatEars::init()
{
    ESP_UTILS_LOGD("Init");

    // Reset auto-reconnect flag when reinitializing
    _auto_reconnect_attempted = false;

    // Initialize BLE
    if (!initBLE()) {
        ESP_UTILS_LOGE("Failed to initialize BLE");
        return false;
    }

    // Load last connected device from NVS
    loadLastConnectedDevice();

    return true;
}

bool RoboCatEars::deinit()
{
    ESP_UTILS_LOGD("Deinit");

    // Disconnect from any connected device before cleanup
    if (_connected) {
        ESP_UTILS_LOGI("Deinit: Disconnecting from device before cleanup");
        disconnect();
        
        // Give a moment for the close command to be sent to BLE controller
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Wait for disconnect to complete and be acknowledged by peripheral
        // This ensures the peripheral receives the disconnect packet
        int wait_count = 0;
        while (_connected && wait_count < 20) {  // Wait up to 1 second
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
            if (wait_count % 4 == 0) {
                ESP_UTILS_LOGD("Deinit: Still waiting for disconnect... (%d/20)", wait_count);
            }
        }
        
        if (_connected) {
            ESP_UTILS_LOGW("Deinit: Device still connected after timeout, forcing cleanup");
            _connected = false;
        } else {
            ESP_UTILS_LOGI("Deinit: Disconnect completed successfully after %d checks", wait_count);
        }
    }

    deinitBLE();

    return true;
}

bool RoboCatEars::run(void)
{
    ESP_UTILS_LOGD("Run");

    // Get the active screen
    lv_obj_t *screen = lv_scr_act();

    // Set black background
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Create both screen objects with callbacks
    _scan_screen = new screens::ScanScreen(
        screen,
        // on_scan_clicked
        [this]() {
            ESP_UTILS_LOGI("Scan button clicked");
            startScan();
        },
        // on_disconnect_clicked
        [this]() {
            ESP_UTILS_LOGI("Disconnect button clicked");
            disconnect();
        },
        // on_device_clicked
        [this](const char* address) {
            ESP_UTILS_LOGI("Device clicked: %s", address);
            connectToDevice(address);
        }
    );

    _control_screen = new screens::ControlScreen(
        screen,
        // on_command_clicked
        [this](const std::string& command) {
            ESP_UTILS_LOGI("Command button clicked: %s", command.c_str());
            writeCharacteristic(command);
        }
    );

    // Add swipe gesture detection to the main screen
    lv_obj_add_event_cb(screen, [](lv_event_t *e) {
        RoboCatEars *app = RoboCatEars::requestInstance();
        if (!app) return;
        
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        
        if (dir == LV_DIR_LEFT && app->_current_screen == 0) {
            // Swipe left from scan screen to control screen
            app->switchToScreen(1);
        } else if (dir == LV_DIR_RIGHT && app->_current_screen == 1) {
            // Swipe right from control screen to scan screen
            app->switchToScreen(0);
        }
    }, LV_EVENT_GESTURE, nullptr);

    // Start on the scan screen
    switchToScreen(0);

    // Attempt auto-reconnect to last device if available
    if (!_last_connected_address.empty() && !_connected) {
        ESP_UTILS_LOGI("Attempting auto-reconnect to last device: %s", _last_connected_address.c_str());
        attemptAutoReconnect();
    } else {
        // Start initial scan if not auto-reconnecting
        startScan();
    }

    return true;
}

bool RoboCatEars::back(void)
{
    ESP_UTILS_LOGD("Back");

    // Disconnect from any connected device before exiting
    if (_connected) {
        ESP_UTILS_LOGI("Back: Disconnecting from device before exit");
        disconnect();
        
        // Give a moment for the close command to be sent to BLE controller
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Wait for disconnect to complete and be acknowledged by peripheral
        // This ensures the peripheral receives the disconnect packet
        int wait_count = 0;
        while (_connected && wait_count < 20) {  // Wait up to 1 second
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
            if (wait_count % 4 == 0) {
                ESP_UTILS_LOGD("Back: Still waiting for disconnect... (%d/20)", wait_count);
            }
        }
        
        if (_connected) {
            ESP_UTILS_LOGW("Back: Device still connected after timeout, forcing cleanup");
            _connected = false;
        } else {
            ESP_UTILS_LOGI("Back: Disconnect completed successfully after %d checks", wait_count);
        }
    }

    // Stop scanning before exiting
    stopScan();

    // Notify core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

bool RoboCatEars::initBLE()
{
    if (_ble_initialized) {
        ESP_UTILS_LOGD("BLE already initialized");
        return true;
    }

    ESP_UTILS_LOGD("Initializing BLE");

    // Initialize NVS (if not already initialized, we just log any errors but don't fail)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_UTILS_LOGW("NVS needs erase, erasing...");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_UTILS_LOGW("NVS erase failed: %s (continuing anyway)", esp_err_to_name(ret));
        } else {
            ret = nvs_flash_init();
        }
    }
    // Log if there was an error, but don't fail - NVS might already be initialized
    if (ret != ESP_OK) {
        ESP_UTILS_LOGD("NVS init status: %s (may already be initialized)", esp_err_to_name(ret));
    }

    // Release classic BT memory (we only need BLE) - ignore if already released
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGW("BT classic mem release failed: %s", esp_err_to_name(ret));
    }

    // Check if controller is already initialized
    ret = esp_bt_controller_get_status();
    if (ret == ESP_BT_CONTROLLER_STATUS_IDLE) {
        // Initialize BT controller
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret) {
            ESP_UTILS_LOGE("Initialize controller failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Enable BT controller if not enabled
    ret = esp_bt_controller_get_status();
    if (ret != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret) {
            ESP_UTILS_LOGE("Enable controller failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Initialize Bluedroid if not already initialized
    ret = esp_bluedroid_get_status();
    if (ret == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        ret = esp_bluedroid_init();
        if (ret) {
            ESP_UTILS_LOGE("Init bluedroid failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Enable Bluedroid if not enabled
    ret = esp_bluedroid_get_status();
    if (ret != ESP_BLUEDROID_STATUS_ENABLED) {
        ret = esp_bluedroid_enable();
        if (ret) {
            ESP_UTILS_LOGE("Enable bluedroid failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // Register GAP callback
    ret = esp_ble_gap_register_callback(gapEventHandler);
    if (ret && ret != ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGE("GAP register error: %s", esp_err_to_name(ret));
        return false;
    }

    // Register GATT client for connections
    ret = esp_ble_gattc_register_callback([](esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
        RoboCatEars *app = RoboCatEars::requestInstance();
        if (!app) return;

        switch (event) {
        case ESP_GATTC_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                app->_gattc_if = gattc_if;
                ESP_UTILS_LOGI("GATT client registered, gattc_if: %d", gattc_if);
            } else {
                ESP_UTILS_LOGE("GATT client registration failed, status: %d", param->reg.status);
            }
            break;
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) {
                app->_connected = true;
                app->_conn_id = param->open.conn_id;
                ESP_UTILS_LOGI("Connected to device, conn_id: %d", app->_conn_id);
                app->updateConnectionStatus();
                
                // Save to NVS for auto-reconnect
                app->saveLastConnectedDevice();
                
                // Start service discovery
                ESP_UTILS_LOGI("Starting service search");
                esp_ble_gattc_search_service(gattc_if, param->open.conn_id, nullptr);
            } else {
                ESP_UTILS_LOGE("Connection failed, status: %d", param->open.status);
                app->_connected = false;
                app->updateConnectionStatus();
            }
            break;
        case ESP_GATTC_CLOSE_EVT:
            ESP_UTILS_LOGI("ESP_GATTC_CLOSE_EVT: Disconnected from device (conn_id=%d, reason=%d)", 
                          app->_conn_id, param->close.reason);
            app->_connected = false;
            app->_conn_id = 0;
            app->_connected_address = "";
            app->_connected_device_name = "";
            app->_connected_address_type = BLE_ADDR_TYPE_PUBLIC;
            app->_char_handle = 0;
            app->_service_discovered = false;
            app->updateConnectionStatus();
            break;
        case ESP_GATTC_SEARCH_CMPL_EVT:
            if (param->search_cmpl.status == ESP_GATT_OK) {
                ESP_UTILS_LOGI("Service search complete");
                
                // Target characteristic: ABF1 (writable command characteristic)
                const uint16_t target_uuid16 = 0xABF1;
                uint8_t target_uuid128[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 
                                              0x00, 0x10, 0x00, 0x00, 0xF1, 0xAB, 0x00, 0x00};
                
                // Get all characteristics
                uint16_t count = 0;
                esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if, 
                                                                         param->search_cmpl.conn_id,
                                                                         ESP_GATT_DB_CHARACTERISTIC,
                                                                         0, 0xFFFF,
                                                                         ESP_GATT_ILLEGAL_HANDLE,
                                                                         &count);
                
                if (status == ESP_GATT_OK && count > 0) {
                    ESP_UTILS_LOGI("Found %d characteristics", count);
                    
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
                            // Display all characteristics
                            for (int i = 0; i < count; i++) {
                                if (char_elems[i].uuid.len == ESP_UUID_LEN_16) {
                                    ESP_UTILS_LOGI("Char %d: UUID16=0x%04X, handle=0x%04x, properties=0x%02x",
                                                   i, char_elems[i].uuid.uuid.uuid16, 
                                                   char_elems[i].char_handle,
                                                   char_elems[i].properties);
                                    
                                    // Check if this is our target characteristic (16-bit UUID)
                                    if (char_elems[i].uuid.uuid.uuid16 == target_uuid16) {
                                        app->_char_handle = char_elems[i].char_handle;
                                        app->_service_discovered = true;
                                        ESP_UTILS_LOGI(">>> Found target characteristic UUID16 ABF1, handle: 0x%04x <<<", app->_char_handle);
                                    }
                                } else if (char_elems[i].uuid.len == ESP_UUID_LEN_128) {
                                    uint8_t *u = char_elems[i].uuid.uuid.uuid128;
                                    ESP_UTILS_LOGI("Char %d: UUID128=%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X, handle=0x%04x",
                                                   i,
                                                   u[15], u[14], u[13], u[12],
                                                   u[11], u[10], u[9], u[8],
                                                   u[7], u[6], u[5], u[4],
                                                   u[3], u[2], u[1], u[0],
                                                   char_elems[i].char_handle);
                                    
                                    // Check if this is our target characteristic (128-bit UUID)
                                    if (memcmp(char_elems[i].uuid.uuid.uuid128, target_uuid128, 16) == 0) {
                                        app->_char_handle = char_elems[i].char_handle;
                                        app->_service_discovered = true;
                                        ESP_UTILS_LOGI(">>> Found target characteristic UUID128 ABF1, handle: 0x%04x <<<", app->_char_handle);
                                    }
                                }
                            }
                        }
                        free(char_elems);
                    }
                }
                
                if (!app->_service_discovered) {
                    ESP_UTILS_LOGW("Target characteristic ABF1 not found");
                }
            } else {
                ESP_UTILS_LOGE("Service search failed, status: %d", param->search_cmpl.status);
            }
            break;
        default:
            break;
        }
    });

    if (ret && ret != ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGE("GATT client register error: %s", esp_err_to_name(ret));
    }

    // Register GATT client app
    ret = esp_ble_gattc_app_register(0);
    if (ret) {
        ESP_UTILS_LOGE("GATT client app register failed: %s", esp_err_to_name(ret));
    }

    _ble_initialized = true;
    ESP_UTILS_LOGI("BLE initialized successfully");

    return true;
}

void RoboCatEars::deinitBLE()
{
    if (!_ble_initialized) {
        return;
    }

    ESP_UTILS_LOGD("Deinitializing BLE");

    // Stop scanning first
    stopScan();

    // If still connected, force disconnect was already done in deinit()/back()
    // but double-check and clean up any lingering state
    if (_connected) {
        ESP_UTILS_LOGW("deinitBLE: Still showing connected, forcing final cleanup");
        _connected = false;
        _conn_id = 0;
        _connected_address = "";
        _connected_device_name = "";
        _connected_address_type = BLE_ADDR_TYPE_PUBLIC;
        _char_handle = 0;
        _service_discovered = false;
    }

    // Unregister the GATT client app to clean up state
    if (_gattc_if != ESP_GATT_IF_NONE) {
        ESP_UTILS_LOGI("Unregistering GATT client app (gattc_if=%d)", _gattc_if);
        esp_err_t ret = esp_ble_gattc_app_unregister(_gattc_if);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGW("Failed to unregister GATT client: %s", esp_err_to_name(ret));
        } else {
            ESP_UTILS_LOGI("GATT client unregistered successfully");
            // Give the BLE stack time to fully clean up after unregister
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        _gattc_if = ESP_GATT_IF_NONE;
    }

    // Note: We don't fully deinitialize the BLE stack (Bluedroid/controller)
    // as it might be shared by other parts of the system
    
    _ble_initialized = false;
}

bool RoboCatEars::startScan()
{
    if (!_ble_initialized) {
        ESP_UTILS_LOGE("BLE not initialized");
        return false;
    }

    if (_scanning) {
        ESP_UTILS_LOGD("Already scanning, stopping previous scan");
        stopScan();
    }

    ESP_UTILS_LOGI("Starting BLE scan (clearing %d previous devices)", _discovered_devices.size());

    // Clear previous devices and update UI to show scanning status
    _discovered_devices.clear();
    updateDeviceList();

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
        ESP_UTILS_LOGE("Set extended scan params error: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

void RoboCatEars::stopScan()
{
    if (_scanning) {
        ESP_UTILS_LOGD("Stopping BLE scan");
        esp_ble_gap_stop_ext_scan();
        _scanning = false;
    }
}

bool RoboCatEars::connectToDevice(const std::string &address)
{
    if (!_ble_initialized) {
        ESP_UTILS_LOGE("BLE not initialized");
        return false;
    }

    // Check if we're still connected - shouldn't happen if disconnect was called properly
    if (_connected) {
        ESP_UTILS_LOGW("Still connected to previous device, forcing disconnect");
        disconnect();
        // Wait for disconnect to complete
        vTaskDelay(pdMS_TO_TICKS(300));
        
        // If still connected after waiting, something went wrong
        if (_connected) {
            ESP_UTILS_LOGE("Failed to disconnect from previous device");
            return false;
        }
    }

    // Stop scanning before connecting
    if (_scanning) {
        ESP_UTILS_LOGI("Stopping scan before connection attempt");
        stopScan();
        
        // Wait a bit for scan to fully stop to avoid "Command Disallowed" error
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_UTILS_LOGI("Connecting to device: %s", address.c_str());

    // Find the device in the discovered list to get its address type and name
    esp_ble_addr_type_t addr_type = BLE_ADDR_TYPE_PUBLIC;  // Default
    std::string device_name = "Unknown";
    auto it = std::find_if(_discovered_devices.begin(), _discovered_devices.end(),
                          [&address](const BleDevice &dev) { return dev.address == address; });
    if (it != _discovered_devices.end()) {
        addr_type = it->address_type;
        device_name = it->name;
        ESP_UTILS_LOGI("Found device in list, using address type %d for device %s", addr_type, address.c_str());
    } else {
        ESP_UTILS_LOGW("Device not in discovered list, using default address type PUBLIC");
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
        ESP_UTILS_LOGE("Invalid MAC address format");
        return false;
    }

    // Check if GATT client is registered
    if (_gattc_if == ESP_GATT_IF_NONE) {
        ESP_UTILS_LOGE("GATT client not registered");
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
        .is_aux = true,  // Use auxiliary connections for BLE 5.0 (we're using extended scanning)
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,  // Use public address for our device
        .phy_mask = ESP_BLE_GAP_PHY_1M_PREF_MASK,
        .phy_1m_conn_params = &conn_params_1m,
        .phy_2m_conn_params = nullptr,
        .phy_coded_conn_params = nullptr,
    };
    memcpy(conn_params.remote_bda, remote_addr, sizeof(esp_bd_addr_t));

    ESP_UTILS_LOGI("Attempting GATT connection: remote_addr_type=%d, own_addr_type=%d, is_aux=%d",
                   conn_params.remote_addr_type, conn_params.own_addr_type, conn_params.is_aux);

    // Initiate connection using enhanced API
    esp_err_t ret = esp_ble_gattc_enh_open(_gattc_if, &conn_params);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("Failed to open GATT connection: %s", esp_err_to_name(ret));
        return false;
    }

    _connected_address = address;
    _connected_device_name = device_name;
    ESP_UTILS_LOGI("Connection initiated to %s (%s)", device_name.c_str(), address.c_str());

    return true;
}

void RoboCatEars::disconnect()
{
    if (_connected && _gattc_if != ESP_GATT_IF_NONE) {
        ESP_UTILS_LOGI("Disconnecting from device, conn_id=%d, gattc_if=%d", _conn_id, _gattc_if);
        // Call close - the ESP_GATTC_CLOSE_EVT will clean up state and update UI
        esp_err_t ret = esp_ble_gattc_close(_gattc_if, _conn_id);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Failed to close GATT connection: %s", esp_err_to_name(ret));
            // Clean up state anyway if close fails
            _connected = false;
            _conn_id = 0;
            _connected_address = "";
            _connected_device_name = "";
            updateConnectionStatus();
        }
    } else {
        ESP_UTILS_LOGW("disconnect() called but not connected (connected=%d, gattc_if=%d, conn_id=%d)", 
                       _connected, _gattc_if, _conn_id);
    }
}

void RoboCatEars::updateConnectionStatus()
{
    if (!_scan_screen || !_control_screen) {
        ESP_UTILS_LOGW("updateConnectionStatus called but UI not initialized");
        return;  // UI not initialized yet
    }

    ESP_UTILS_LOGD("Updating connection status: connected=%d, address=%s", _connected, _connected_address.c_str());

    // Update using lv_async_call for thread safety
    lv_async_call([](void *user_data) {
        RoboCatEars *app = (RoboCatEars *)user_data;
        
        if (app->_connected) {
            // Connected state
            std::string status_text = "Connected\n" + app->_connected_device_name;
            lv_label_set_text(app->_scan_screen->getStatusLabel(), status_text.c_str());
            lv_obj_set_style_text_color(app->_scan_screen->getStatusLabel(), lv_color_hex(0x00FF00), 0);
            
            // Update control screen status label too
            lv_label_set_text(app->_control_screen->getStatusLabel(), status_text.c_str());
            lv_obj_set_style_text_color(app->_control_screen->getStatusLabel(), lv_color_hex(0x00FF00), 0);
            
            // Show disconnect button, hide scan button
            lv_obj_clear_flag(app->_scan_screen->getDisconnectButton(), LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(app->_scan_screen->getScanButton(), LV_OBJ_FLAG_HIDDEN);
            ESP_UTILS_LOGD("UI updated: showing connected status and disconnect button");
        } else {
            // Disconnected state
            lv_label_set_text(app->_scan_screen->getStatusLabel(), "Not connected");
            lv_obj_set_style_text_color(app->_scan_screen->getStatusLabel(), lv_color_hex(0x808080), 0);
            
            // Update control screen status label too
            lv_label_set_text(app->_control_screen->getStatusLabel(), "Not connected");
            lv_obj_set_style_text_color(app->_control_screen->getStatusLabel(), lv_color_hex(0x808080), 0);
            
            // Show scan button, hide disconnect button
            lv_obj_clear_flag(app->_scan_screen->getScanButton(), LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(app->_scan_screen->getDisconnectButton(), LV_OBJ_FLAG_HIDDEN);
            ESP_UTILS_LOGD("UI updated: showing disconnected status and scan button");
        }
    }, this);
}

void RoboCatEars::gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (_instance) {
        _instance->handleGapEvent(event, param);
    }
}

void RoboCatEars::handleGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT: {
        if (param->set_ext_scan_params.status != ESP_BT_STATUS_SUCCESS) {
            ESP_UTILS_LOGE("Set extended scan params failed, error status = %x", param->set_ext_scan_params.status);
            _scanning = false;
        } else {
            ESP_UTILS_LOGD("Extended scan params set, starting scan");
            uint32_t duration = 1000; // Scan for 10 seconds (units of 10ms)
            uint16_t period = 0;       // No periodic scanning
            esp_ble_gap_start_ext_scan(duration, period);
            _scanning = true;
        }
        break;
    }
    
    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
        if (param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
            ESP_UTILS_LOGE("Extended scan start failed, error status = %x", param->ext_scan_start.status);
            _scanning = false;
        } else {
            ESP_UTILS_LOGD("Extended scan started successfully");
        }
        break;

    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT: {
        // Extended advertising report - contains scan results
        esp_ble_gap_ext_adv_report_t *report = &param->ext_adv_report.params;
        
        // Device found - extract minimal info here to avoid stack overflow
        char addr_str[18];
        snprintf(addr_str, sizeof(addr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 report->addr[0], report->addr[1], report->addr[2],
                 report->addr[3], report->addr[4], report->addr[5]);

        // Extract device name (keep it simple to avoid stack issues)
        std::string device_name = "Unknown";
        uint8_t *adv_data = report->adv_data;
        uint16_t adv_data_len = report->adv_data_len;
        
        // Limit search to avoid stack overflow
        if (adv_data_len > 0 && adv_data_len < 64) {
            for (uint16_t i = 0; i < adv_data_len && i < 60; ) {
                uint8_t length = adv_data[i];
                if (length == 0 || i + length >= adv_data_len) break;
                
                uint8_t type = adv_data[i + 1];
                if (type == 0x09 || type == 0x08) { // Complete or shortened local name
                    uint8_t name_len = length - 1;
                    if (name_len > 0 && name_len < 30) {  // Limit name length
                        device_name = std::string((char *)&adv_data[i + 2], name_len);
                    }
                    break;
                }
                i += length + 1;
            }
        }

        int rssi = report->rssi;
        esp_ble_addr_type_t addr_type = (esp_ble_addr_type_t)report->addr_type;

        ESP_UTILS_LOGD("Scan found device: %s (%s) addr_type=%d RSSI=%d", 
                      device_name.c_str(), addr_str, addr_type, rssi);

        // Add device to list
        addDevice(device_name, addr_str, addr_type, rssi);
        break;
    }

    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
        if (param->ext_scan_stop.status != ESP_BT_STATUS_SUCCESS) {
            ESP_UTILS_LOGE("Extended scan stop failed, error status = %x", param->ext_scan_stop.status);
        } else {
            ESP_UTILS_LOGD("Extended scan stopped successfully");
        }
        _scanning = false;
        // Update UI to show scan is complete
        updateDeviceList();
        break;

    case ESP_GAP_BLE_SCAN_TIMEOUT_EVT:
        ESP_UTILS_LOGD("BLE scan timeout - scan completed");
        _scanning = false;
        // Update UI to show scan is complete
        updateDeviceList();
        break;

    default:
        break;
    }
}

void RoboCatEars::addDevice(const std::string &name, const std::string &address, esp_ble_addr_type_t address_type, int rssi)
{
    // Filter: only accept devices whose name starts with "ROBO_CAT_EARS"
    if (name.find("ROBO_CAT_EARS") != 0) {
        // Device name doesn't match pattern, skip it
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
        
        ESP_UTILS_LOGD("Found matching device: %s (%s) RSSI: %d", name.c_str(), address.c_str(), rssi);
    }

    // Update the UI
    updateDeviceList();
}

void RoboCatEars::updateDeviceList()
{
    if (!_scan_screen) {
        return;
    }

    // This function is called from BLE callback (different task), so we need LVGL lock
    // Use LVGL's async call mechanism to safely update UI from another task
    lv_async_call([](void *user_data) {
        RoboCatEars *app = (RoboCatEars *)user_data;
        if (!app || !app->_scan_screen) {
            return;
        }

        lv_obj_t *device_list = app->_scan_screen->getDeviceList();
        if (!device_list) {
            return;
        }

        // Free allocated memory for user_data before clearing
        uint32_t child_count = lv_obj_get_child_cnt(device_list);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(device_list, i);
            std::string *addr = (std::string *)lv_obj_get_user_data(child);
            if (addr) {
                delete addr;
                lv_obj_set_user_data(child, nullptr);
            }
        }

        // Clear the list
        lv_obj_clean(device_list);

        if (app->_discovered_devices.empty()) {
            if (app->_scanning) {
                lv_obj_t *btn = lv_list_add_btn(device_list, LV_SYMBOL_REFRESH, "Scanning for ears...");
                lv_obj_set_style_text_color(btn, lv_color_hex(0x808080), 0);
            } else {
                lv_obj_t *btn = lv_list_add_btn(device_list, LV_SYMBOL_WARNING, "No ears found\nPress Scan to search");
                lv_obj_set_style_text_color(btn, lv_color_hex(0x808080), 0);
            }
        } else {
            // Sort by RSSI (strongest first)
            std::vector<BleDevice> sorted_devices = app->_discovered_devices;
            std::sort(sorted_devices.begin(), sorted_devices.end(),
                     [](const BleDevice &a, const BleDevice &b) { return a.rssi > b.rssi; });

            // Add devices to the list
            for (const auto &device : sorted_devices) {
                char label[128];
                snprintf(label, sizeof(label), "%s\n%s (RSSI: %d)",
                        device.name.c_str(), device.address.c_str(), device.rssi);
                
                lv_obj_t *btn = lv_list_add_btn(device_list, LV_SYMBOL_BLUETOOTH, label);
                
                // Make list items larger
                lv_obj_set_height(btn, 80);
                lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);
                
                // Color code by signal strength: green (good) / orange (medium) / red (poor)
                lv_color_t signal_color;
                int num_bars;
                if (device.rssi > -60) {
                    lv_obj_set_style_text_color(btn, lv_color_hex(0x00FF00), 0); // Green - good signal
                    signal_color = lv_color_hex(0x00FF00);
                    num_bars = 3;
                } else if (device.rssi > -80) {
                    lv_obj_set_style_text_color(btn, lv_color_hex(0xFF8800), 0); // Orange - medium signal
                    signal_color = lv_color_hex(0xFF8800);
                    num_bars = 2;
                } else {
                    lv_obj_set_style_text_color(btn, lv_color_hex(0xFF0000), 0); // Red - poor signal
                    signal_color = lv_color_hex(0xFF0000);
                    num_bars = 1;
                }

                // Add signal strength bars on the right side
                lv_obj_t *bar_container = lv_obj_create(btn);
                lv_obj_set_size(bar_container, 40, 30);
                lv_obj_align(bar_container, LV_ALIGN_RIGHT_MID, -10, 0);
                lv_obj_set_style_bg_opa(bar_container, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(bar_container, 0, 0);
                lv_obj_set_style_pad_all(bar_container, 0, 0);
                lv_obj_clear_flag(bar_container, LV_OBJ_FLAG_CLICKABLE);
                
                // Bar dimensions
                const int bar_width = 6;
                const int bar_gap = 4;
                const int bar_heights[] = {8, 16, 24}; // Short, medium, full
                
                // Add bars based on signal strength
                for (int i = 0; i < num_bars; i++) {
                    lv_obj_t *bar = lv_obj_create(bar_container);
                    lv_obj_set_size(bar, bar_width, bar_heights[i]);
                    lv_obj_set_pos(bar, i * (bar_width + bar_gap), 30 - bar_heights[i]);
                    lv_obj_set_style_bg_color(bar, signal_color, 0);
                    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
                    lv_obj_set_style_border_width(bar, 0, 0);
                    lv_obj_set_style_radius(bar, 1, 0);
                    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
                }

                // Store device address in user data for click handler
                std::string *addr = new std::string(device.address);
                lv_obj_set_user_data(btn, addr);
                
                // Add click event handler
                lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
                    std::string *addr = (std::string *)lv_obj_get_user_data(btn);
                    if (addr) {
                        RoboCatEars *app = RoboCatEars::requestInstance();
                        if (app) {
                            ESP_UTILS_LOGI("Connecting to device: %s", addr->c_str());
                            
                            // Stop scanning if active
                            if (app->_scanning) {
                                ESP_UTILS_LOGI("Stopping scan before connection");
                                app->stopScan();
                                vTaskDelay(pdMS_TO_TICKS(150));
                            }
                            
                            // Disconnect from current device if connected
                            if (app->_connected) {
                                ESP_UTILS_LOGI("Disconnecting from current device before connecting to new one");
                                app->disconnect();
                                
                                // Wait for disconnect to complete before connecting
                                // The disconnect is asynchronous, so we need to wait
                                vTaskDelay(pdMS_TO_TICKS(200));
                            }
                            
                            app->connectToDevice(*addr);
                        }
                    }
                }, LV_EVENT_CLICKED, nullptr);
            }
        }
    }, this);
}

void RoboCatEars::switchToScreen(int screen_index)
{
    ESP_UTILS_LOGD("Switching to screen %d", screen_index);
    
    if (!_scan_screen || !_control_screen) {
        ESP_UTILS_LOGE("Screens not initialized");
        return;
    }
    
    if (screen_index == 0) {
        // Show scan screen, hide control screen
        lv_obj_clear_flag(_scan_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_control_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        _current_screen = 0;
    } else if (screen_index == 1) {
        // Show control screen, hide scan screen
        lv_obj_add_flag(_scan_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_control_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        _current_screen = 1;
        
        // Update the control screen status label to match current connection state
        updateConnectionStatus();
    }
}

bool RoboCatEars::writeCharacteristic(const std::string &data)
{
    if (!_connected || _gattc_if == ESP_GATT_IF_NONE) {
        ESP_UTILS_LOGW("Cannot write: not connected");
        return false;
    }
    
    if (!_service_discovered || _char_handle == 0) {
        ESP_UTILS_LOGW("Cannot write: characteristic not discovered yet");
        return false;
    }
    
    ESP_UTILS_LOGI("Writing to characteristic: %s", data.c_str());
    
    esp_err_t ret = esp_ble_gattc_write_char(
        _gattc_if,
        _conn_id,
        _char_handle,
        data.length(),
        (uint8_t *)data.c_str(),
        ESP_GATT_WRITE_TYPE_NO_RSP,
        ESP_GATT_AUTH_REQ_NONE
    );
    
    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("Failed to write characteristic: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool RoboCatEars::saveLastConnectedDevice()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("robo_cat_ears", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_UTILS_LOGE("Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    // Save address
    err = nvs_set_str(nvs_handle, "last_addr", _connected_address.c_str());
    if (err != ESP_OK) {
        ESP_UTILS_LOGE("Failed to save address: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Save address type
    err = nvs_set_u8(nvs_handle, "last_addr_type", (uint8_t)_connected_address_type);
    if (err != ESP_OK) {
        ESP_UTILS_LOGE("Failed to save address type: %s", esp_err_to_name(err));
    }

    // Save name
    err = nvs_set_str(nvs_handle, "last_name", _connected_device_name.c_str());
    if (err != ESP_OK) {
        ESP_UTILS_LOGE("Failed to save name: %s", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_UTILS_LOGI("Saved last connected device to NVS: %s (%s)", 
                   _connected_device_name.c_str(), _connected_address.c_str());
    return true;
}

bool RoboCatEars::loadLastConnectedDevice()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("robo_cat_ears", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_UTILS_LOGD("No saved device found or NVS not initialized: %s", esp_err_to_name(err));
        return false;
    }

    // Load address
    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, "last_addr", nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) {
        ESP_UTILS_LOGD("No saved address found");
        nvs_close(nvs_handle);
        return false;
    }

    char* address = (char*)malloc(required_size);
    if (!address) {
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_get_str(nvs_handle, "last_addr", address, &required_size);
    if (err == ESP_OK) {
        _last_connected_address = std::string(address);
    }
    free(address);

    // Load address type
    uint8_t addr_type = BLE_ADDR_TYPE_PUBLIC;
    err = nvs_get_u8(nvs_handle, "last_addr_type", &addr_type);
    if (err == ESP_OK) {
        _last_connected_address_type = (esp_ble_addr_type_t)addr_type;
    }

    // Load name
    required_size = 0;
    err = nvs_get_str(nvs_handle, "last_name", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* name = (char*)malloc(required_size);
        if (name) {
            err = nvs_get_str(nvs_handle, "last_name", name, &required_size);
            if (err == ESP_OK) {
                _last_connected_name = std::string(name);
            }
            free(name);
        }
    }

    nvs_close(nvs_handle);

    if (!_last_connected_address.empty()) {
        ESP_UTILS_LOGI("Loaded last connected device from NVS: %s (%s)", 
                       _last_connected_name.c_str(), _last_connected_address.c_str());
        return true;
    }

    return false;
}

void RoboCatEars::attemptAutoReconnect()
{
    if (_auto_reconnect_attempted || _last_connected_address.empty()) {
        return;
    }

    _auto_reconnect_attempted = true;

    ESP_UTILS_LOGI("Attempting auto-reconnect to: %s (%s)",
                   _last_connected_name.c_str(), _last_connected_address.c_str());

    // Add the device to discovered list temporarily so connectToDevice can find it
    BleDevice device;
    device.name = _last_connected_name.empty() ? "Saved Device" : _last_connected_name;
    device.address = _last_connected_address;
    device.address_type = _last_connected_address_type;
    device.rssi = -50; // Placeholder RSSI for saved device
    _discovered_devices.push_back(device);
    
    // Update UI to show the saved device
    updateDeviceList();

    // Attempt connection
    connectToDevice(_last_connected_address);
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, RoboCatEars, APP_NAME, []()
{
    return std::shared_ptr<RoboCatEars>(RoboCatEars::requestInstance(), [](RoboCatEars * p) {});
})

} // namespace esp_brookesia::apps
