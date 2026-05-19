/*
 * Description: Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
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

// External reference to global phone instance (declared in main.cpp)
namespace esp_brookesia::systems::phone {
    class Phone;
}
extern esp_brookesia::systems::phone::Phone *g_phone;

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
    _animate_screen(nullptr),
    _glow_screen(nullptr),
    _pick_color_screen(nullptr),
    _current_screen(0),
    _bluetooth_service(nullptr),
    _reconnection_timer(nullptr),
    _last_disconnected_address(""),
    _modal_is_open(false)
{
}

RoboCatEars::~RoboCatEars()
{
    // Bluetooth service is a singleton, managed separately
    
    // Clean up screen objects
    if (_scan_screen) {
        delete _scan_screen;
        _scan_screen = nullptr;
    }
    if (_animate_screen) {
        delete _animate_screen;
        _animate_screen = nullptr;
    }
    if (_glow_screen) {
        delete _glow_screen;
        _glow_screen = nullptr;
    }
    if (_pick_color_screen) {
        delete _pick_color_screen;
        _pick_color_screen = nullptr;
    }
}

bool RoboCatEars::init()
{
    ESP_UTILS_LOGD("Init");

    // Get Bluetooth service instance
    _bluetooth_service = robo_cat_ears::BluetoothService::getInstance();
    if (!_bluetooth_service) {
        ESP_UTILS_LOGE("Failed to get Bluetooth service instance");
        return false;
    }

    // Initialize Bluetooth service
    if (!_bluetooth_service->init()) {
        ESP_UTILS_LOGE("Failed to initialize Bluetooth service");
        return false;
    }

    // Load last connected device from NVS
    _bluetooth_service->loadLastConnectedDevice();

    return true;
}

bool RoboCatEars::deinit()
{
    ESP_UTILS_LOGD("Deinit");

    // Stop auto-reconnection timer
    stopReconnectionTimer();

    // Disconnect from any connected device before cleanup
    if (_bluetooth_service && _bluetooth_service->isConnected()) {
        ESP_UTILS_LOGI("Deinit: Disconnecting from device before cleanup");
        _bluetooth_service->disconnect();
        
        // Give a moment for the close command to be sent to BLE controller
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Wait for disconnect to complete
        int wait_count = 0;
        while (_bluetooth_service->isConnected() && wait_count < 20) {  // Wait up to 1 second
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
            if (wait_count % 4 == 0) {
                ESP_UTILS_LOGD("Deinit: Still waiting for disconnect... (%d/20)", wait_count);
            }
        }
        
        if (_bluetooth_service->isConnected()) {
            ESP_UTILS_LOGW("Deinit: Device still connected after timeout");
        } else {
            ESP_UTILS_LOGI("Deinit: Disconnect completed successfully after %d checks", wait_count);
        }
    }

    // Deinitialize Bluetooth service
    if (_bluetooth_service) {
        _bluetooth_service->deinit();
    }

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
            if (_bluetooth_service) {
                _bluetooth_service->startScan();
            }
        },
        // on_disconnect_clicked
        [this]() {
            ESP_UTILS_LOGI("Disconnect button clicked");
            if (_bluetooth_service) {
                _bluetooth_service->disconnect();
            }
        },
        // on_device_clicked
        [this](const char* address) {
            ESP_UTILS_LOGI("Device clicked: %s", address);
            if (_bluetooth_service) {
                _bluetooth_service->connectToDevice(address);
            }
        }
    );

    _animate_screen = new screens::AnimateScreen(
        screen,
        // on_command_clicked
        [this](const std::string& command) {
            ESP_UTILS_LOGI("Command button clicked: %s", command.c_str());
            if (_bluetooth_service) {
                robo_cat_ears::DataPacket packet;
                packet.type = robo_cat_ears::DataType::ANIMATION;
                packet.data = command;
                _bluetooth_service->writeDataPacket(packet);
            }
        }
    );

    _glow_screen = new screens::GlowScreen(screen);

    // Create pick color screen (modal, not part of swipe navigation)
    _pick_color_screen = new screens::PickColorScreen(screen);

    // Wire up glow screen's add color button to show pick color screen
    _glow_screen->setOnAddColorClicked([this]() {
        ESP_UTILS_LOGI("Add color button clicked, showing color picker");
        _pick_color_screen->show();
    });

    // Wire up pick color screen's callback to add color to glow screen
    _pick_color_screen->setOnColorPicked([this](uint32_t color) {
        ESP_UTILS_LOGI("Color picked: 0x%06X, adding to glow screen", color);
        _glow_screen->addColor(color);
    });

    // Wire up pick color screen's modal visibility callbacks
    _pick_color_screen->setOnModalShown([this]() {
        ESP_UTILS_LOGI("Pick color modal shown, disabling gestures");
        _modal_is_open = true;
    });
    
    _pick_color_screen->setOnModalHidden([this]() {
        ESP_UTILS_LOGI("Pick color modal hidden, enabling gestures");
        _modal_is_open = false;
    });

    // Add swipe gesture detection to the main screen
    lv_obj_add_event_cb(screen, [](lv_event_t *e) {
        RoboCatEars *app = RoboCatEars::requestInstance();
        if (!app) return;
        
        // Don't process gestures if a modal is open
        if (app->_modal_is_open) {
            return;
        }
        
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        
        if (dir == LV_DIR_LEFT) {
            // Swipe left - go to next screen (circular)
            int next_screen = (app->_current_screen + 1) % 3;
            app->switchToScreen(next_screen);
        } else if (dir == LV_DIR_RIGHT) {
            // Swipe right - go to previous screen (circular)
            int prev_screen = (app->_current_screen - 1 + 3) % 3;
            app->switchToScreen(prev_screen);
        }
    }, LV_EVENT_GESTURE, nullptr);

    // Setup Bluetooth service callbacks
    if (_bluetooth_service) {
        // Device list updated callback
        _bluetooth_service->setDeviceListUpdatedCallback([this](const std::vector<robo_cat_ears::BleDevice> &devices) {
            // Defer LVGL operations to avoid blocking BLE task
            lv_async_call([](void* user_data) {
                auto* app = static_cast<RoboCatEars*>(user_data);
                app->updateDeviceList();
            }, this);
        });
        
        // Connection status callback
        _bluetooth_service->setConnectionStatusCallback([this](bool connected, const std::string &device_name, const std::string &address) {
            // Update Bluetooth status bar icon
            updateBluetoothStatusIcon(connected);
            
            // Stop reconnection timer on successful connection
            if (connected) {
                lv_async_call([](void* user_data) {
                    auto* app = static_cast<RoboCatEars*>(user_data);
                    app->stopReconnectionTimer();
                }, this);
            }
            
            // Defer LVGL operations to avoid blocking BLE task
            // updateConnectionStatus() will handle updating the device list internally
            lv_async_call([](void* user_data) {
                auto* app = static_cast<RoboCatEars*>(user_data);
                app->updateConnectionStatus();
            }, this);
            
            // Auto-switch to animate screen on successful connection (deferred to LVGL task)
            if (connected) {
                ESP_UTILS_LOGI("Connection successful, switching to animate screen");
                lv_async_call([](void* user_data) {
                    auto* app = static_cast<RoboCatEars*>(user_data);
                    app->switchToScreen(1);
                }, this);
            }
        });
        
        // Disconnection callback - start auto-reconnection
        _bluetooth_service->setDisconnectionCallback([this](const std::string &device_name, const std::string &address) {
            ESP_UTILS_LOGI("Disconnection detected for %s (%s), starting auto-reconnection", 
                          device_name.c_str(), address.c_str());
            
            // Store the disconnected device address for reconnection
            lv_async_call([](void* user_data) {
                auto* app = static_cast<RoboCatEars*>(user_data);
                if (app->_bluetooth_service) {
                    app->_last_disconnected_address = app->_bluetooth_service->getLastConnectedAddress();
                    app->startReconnectionTimer();
                }
            }, this);
        });
        
        // Scanning status callback
        _bluetooth_service->setScanningStatusCallback([this](bool scanning) {
            // Defer LVGL operations to avoid blocking BLE task
            lv_async_call([](void* user_data) {
                auto* app = static_cast<RoboCatEars*>(user_data);
                app->updateDeviceList();
            }, this);
        });
    }

    // Initialize device list UI with current state
    updateDeviceList();

    // Start on the scan screen
    switchToScreen(0);

    // Attempt auto-reconnect to last device if available
    if (_bluetooth_service && !_bluetooth_service->getLastConnectedAddress().empty() && 
        !_bluetooth_service->isConnected() && !_bluetooth_service->wasAutoReconnectAttempted()) {
        ESP_UTILS_LOGI("Attempting auto-reconnect to last device: %s", _bluetooth_service->getLastConnectedAddress().c_str());
        _bluetooth_service->attemptAutoReconnect();
    } else if (_bluetooth_service) {
        // Start initial scan if not auto-reconnecting
        _bluetooth_service->startScan();
    }

    return true;
}

bool RoboCatEars::back(void)
{
    ESP_UTILS_LOGD("Back");

    // Stop auto-reconnection timer
    stopReconnectionTimer();

    // Disconnect from any connected device before exiting
    if (_bluetooth_service && _bluetooth_service->isConnected()) {
        ESP_UTILS_LOGI("Back: Disconnecting from device before exit");
        _bluetooth_service->disconnect();
        
        // Give a moment for the close command to be sent to BLE controller
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Wait for disconnect to complete
        int wait_count = 0;
        while (_bluetooth_service->isConnected() && wait_count < 20) {  // Wait up to 1 second
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
            if (wait_count % 4 == 0) {
                ESP_UTILS_LOGD("Back: Still waiting for disconnect... (%d/20)", wait_count);
            }
        }
        
        if (_bluetooth_service->isConnected()) {
            ESP_UTILS_LOGW("Back: Device still connected after timeout");
        } else {
            ESP_UTILS_LOGI("Back: Disconnect completed successfully after %d checks", wait_count);
        }
    }

    // Stop scanning before exiting
    if (_bluetooth_service) {
        _bluetooth_service->stopScan();
    }

    // Notify core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

void RoboCatEars::updateConnectionStatus()
{
    if (!_scan_screen || !_animate_screen || !_glow_screen) {
        ESP_UTILS_LOGW("updateConnectionStatus called but UI not initialized");
        return;  // UI not initialized yet
    }

    if (!_bluetooth_service) {
        ESP_UTILS_LOGW("updateConnectionStatus called but Bluetooth service not initialized");
        return;
    }

    bool connected = _bluetooth_service->isConnected();
    std::string device_name = _bluetooth_service->getConnectedDeviceName();
    std::string address = _bluetooth_service->getConnectedAddress();

    ESP_UTILS_LOGD("Updating connection status: connected=%d, address=%s", connected, address.c_str());

    // Update using lv_async_call for thread safety
    lv_async_call([](void *user_data) {
        RoboCatEars *app = (RoboCatEars *)user_data;
        
        if (!app->_bluetooth_service) {
            return;
        }
        
        bool connected = app->_bluetooth_service->isConnected();
        
        if (connected) {
            // Connected state
            std::string device_name = app->_bluetooth_service->getConnectedDeviceName();
            std::string address = app->_bluetooth_service->getConnectedAddress();
            std::string status_text = "Connected\n" + device_name;
            lv_label_set_text(app->_scan_screen->getStatusLabel(), status_text.c_str());
            lv_obj_set_style_text_color(app->_scan_screen->getStatusLabel(), lv_color_hex(0x00FF00), 0);
            
            // Update connected device for highlighting in device list
            app->_scan_screen->setConnectedDevice(address.c_str());
            
            // Update animate screen status label too
            lv_label_set_text(app->_animate_screen->getStatusLabel(), status_text.c_str());
            lv_obj_set_style_text_color(app->_animate_screen->getStatusLabel(), lv_color_hex(0x00FF00), 0);
            
            // Update glow screen status label too
            lv_label_set_text(app->_glow_screen->getStatusLabel(), status_text.c_str());
            lv_obj_set_style_text_color(app->_glow_screen->getStatusLabel(), lv_color_hex(0x00FF00), 0);
            
            // Show disconnect button, hide scan button
            lv_obj_clear_flag(app->_scan_screen->getDisconnectButton(), LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(app->_scan_screen->getScanButton(), LV_OBJ_FLAG_HIDDEN);
            ESP_UTILS_LOGD("UI updated: showing connected status and disconnect button");
            
            // Force refresh the device list after setting connected device
            app->updateDeviceList();
        } else {
            // Disconnected state
            lv_label_set_text(app->_scan_screen->getStatusLabel(), "Not connected");
            lv_obj_set_style_text_color(app->_scan_screen->getStatusLabel(), lv_color_hex(0x808080), 0);
            
            // Clear connected device for device list
            app->_scan_screen->setConnectedDevice(nullptr);
            ESP_UTILS_LOGD("Cleared connected device address");
            
            // Update animate screen status label too
            lv_label_set_text(app->_animate_screen->getStatusLabel(), "Not connected");
            lv_obj_set_style_text_color(app->_animate_screen->getStatusLabel(), lv_color_hex(0x808080), 0);
            
            // Update glow screen status label too
            lv_label_set_text(app->_glow_screen->getStatusLabel(), "Not connected");
            lv_obj_set_style_text_color(app->_glow_screen->getStatusLabel(), lv_color_hex(0x808080), 0);
            
            // Show scan button, hide disconnect button
            lv_obj_clear_flag(app->_scan_screen->getScanButton(), LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(app->_scan_screen->getDisconnectButton(), LV_OBJ_FLAG_HIDDEN);
            ESP_UTILS_LOGD("UI updated: showing disconnected status and scan button");
            
            // Force refresh the device list after clearing connected device
            // This ensures the list is updated with the cleared connection state
            app->updateDeviceList();
        }
    }, this);
}

void RoboCatEars::updateBluetoothStatusIcon(bool connected)
{
    if (!g_phone) {
        ESP_UTILS_LOGW("Cannot update Bluetooth icon: phone instance not available");
        return;
    }

    ESP_UTILS_LOGD("Updating Bluetooth status icon (WiFi icon): %s", connected ? "connected" : "disconnected");

    // Use WiFi icon state: 0 = disconnected (X icon), 3 = connected (full bars)
    // Use lv_async_call for thread safety
    struct IconUpdateData {
        bool connected;
    };
    
    IconUpdateData *data = new IconUpdateData{connected};
    
    lv_async_call([](void *user_data) {
        IconUpdateData *data = static_cast<IconUpdateData*>(user_data);
        
        if (g_phone) {
            // State 0: Disconnected (X icon), State 3: Connected (full bars)
            int state = data->connected ? 3 : 0;
            bool result = g_phone->getDisplay().getStatusBar()->setWifiIconState(state);
            if (result) {
                ESP_UTILS_LOGD("WiFi icon (repurposed for Bluetooth) updated to state %d: %s", 
                               state, data->connected ? "connected" : "disconnected");
            } else {
                ESP_UTILS_LOGE("Failed to update WiFi icon state");
            }
        }
        
        delete data;
    }, data);
}

void RoboCatEars::updateDeviceList()
{
    if (!_scan_screen || !_bluetooth_service) {
        return;
    }

    // This function is called from BLE callback (different task), so we need LVGL lock
    // Use LVGL's async call mechanism to safely update UI from another task
    lv_async_call([](void *user_data) {
        RoboCatEars *app = (RoboCatEars *)user_data;
        if (!app || !app->_scan_screen || !app->_bluetooth_service) {
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

        const auto &discovered_devices = app->_bluetooth_service->getDiscoveredDevices();
        bool scanning = app->_bluetooth_service->isScanning();

        if (discovered_devices.empty()) {
            if (scanning) {
                lv_obj_t *btn = lv_list_add_btn(device_list, LV_SYMBOL_REFRESH, "Scanning for ears...");
                lv_obj_set_style_text_color(btn, lv_color_hex(0x808080), 0);
                lv_obj_add_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
            } else {
                lv_obj_t *btn = lv_list_add_btn(device_list, LV_SYMBOL_WARNING, "No ears found\nPress Scan to search");
                lv_obj_set_style_text_color(btn, lv_color_hex(0x808080), 0);
                lv_obj_add_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
            }
        } else {
            // Sort by RSSI (strongest first)
            std::vector<robo_cat_ears::BleDevice> sorted_devices = discovered_devices;
            std::sort(sorted_devices.begin(), sorted_devices.end(),
                     [](const robo_cat_ears::BleDevice &a, const robo_cat_ears::BleDevice &b) { return a.rssi > b.rssi; });

            // Add devices to the list
            for (const auto &device : sorted_devices) {
                // Check if this device is connected
                const char *connected_addr = app->_scan_screen->getConnectedDevice();
                bool is_connected = (connected_addr && strlen(connected_addr) > 0 && 
                                   device.address == connected_addr);
                
                char label[128];
                if (is_connected) {
                    snprintf(label, sizeof(label), "%s\n%s (RSSI: %d)\nCONNECTED",
                            device.name.c_str(), device.address.c_str(), device.rssi);
                } else {
                    snprintf(label, sizeof(label), "%s\n%s (RSSI: %d)",
                            device.name.c_str(), device.address.c_str(), device.rssi);
                }
                
                const char *icon = is_connected ? LV_SYMBOL_OK : LV_SYMBOL_BLUETOOTH;
                lv_obj_t *btn = lv_list_add_btn(device_list, icon, label);
                
                // Make list items larger (more height if connected for extra text)
                lv_obj_set_height(btn, is_connected ? 100 : 80);
                lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);
                
                // Highlight connected device with different background color
                if (is_connected) {
                    lv_obj_set_style_bg_color(btn, lv_color_hex(0x005500), 0);  // Dark green background
                }
                
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
                lv_obj_add_flag(bar_container, LV_OBJ_FLAG_GESTURE_BUBBLE);
                
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
                    lv_obj_add_flag(bar, LV_OBJ_FLAG_GESTURE_BUBBLE);
                }

                // Store device address in user data for click handler
                std::string *addr = new std::string(device.address);
                lv_obj_set_user_data(btn, addr);
                
                // Allow gestures to bubble up from list items for swipe navigation
                lv_obj_add_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
                
                // Add click event handler
                lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                    // Ignore click if a gesture (swipe) was detected
                    lv_dir_t gesture_dir = lv_indev_get_gesture_dir(lv_indev_get_act());
                    if (gesture_dir != LV_DIR_NONE) {
                        return; // Swipe detected, don't process as click
                    }
                    
                    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
                    std::string *addr = (std::string *)lv_obj_get_user_data(btn);
                    if (addr) {
                        RoboCatEars *app = RoboCatEars::requestInstance();
                        if (app && app->_bluetooth_service) {
                            ESP_UTILS_LOGI("Connecting to device: %s", addr->c_str());
                            
                            // Stop scanning if active
                            if (app->_bluetooth_service->isScanning()) {
                                ESP_UTILS_LOGI("Stopping scan before connection");
                                app->_bluetooth_service->stopScan();
                                vTaskDelay(pdMS_TO_TICKS(150));
                            }
                            
                            // Disconnect from current device if connected
                            if (app->_bluetooth_service->isConnected()) {
                                ESP_UTILS_LOGI("Disconnecting from current device before connecting to new one");
                                app->_bluetooth_service->disconnect();
                                
                                // Wait for disconnect to complete before connecting
                                vTaskDelay(pdMS_TO_TICKS(200));
                            }
                            
                            app->_bluetooth_service->connectToDevice(*addr);
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
    
    if (!_scan_screen || !_animate_screen || !_glow_screen) {
        ESP_UTILS_LOGE("Screens not initialized");
        return;
    }
    
    // Hide all screens first
    lv_obj_add_flag(_scan_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_animate_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_glow_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
    
    // Show the selected screen
    if (screen_index == 0) {
        lv_obj_clear_flag(_scan_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        _current_screen = 0;
    } else if (screen_index == 1) {
        lv_obj_clear_flag(_animate_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        _current_screen = 1;
        updateConnectionStatus();
    } else if (screen_index == 2) {
        lv_obj_clear_flag(_glow_screen->getContainer(), LV_OBJ_FLAG_HIDDEN);
        _current_screen = 2;
        updateConnectionStatus();
    }
}

void RoboCatEars::startReconnectionTimer()
{
    // Stop existing timer if any
    stopReconnectionTimer();
    
    ESP_UTILS_LOGI("Starting auto-reconnection timer (will retry every 5 seconds)");
    
    // Create timer that attempts reconnection every 5 seconds
    _reconnection_timer = lv_timer_create([](lv_timer_t *t) {
        RoboCatEars *app = (RoboCatEars *)t->user_data;
        if (!app || !app->_bluetooth_service) {
            return;
        }
        
        // Check if backlight is on by checking display inactive time
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) {
            uint32_t inactive_time = lv_disp_get_inactive_time(disp);
            const uint32_t BACKLIGHT_TIMEOUT_MS = 15000;
            
            // Don't attempt reconnection if backlight is off (device is idle)
            if (inactive_time >= BACKLIGHT_TIMEOUT_MS) {
                ESP_UTILS_LOGD("Backlight is off (inactive %lu ms), skipping reconnection attempt", inactive_time);
                return;
            }
        }
        
        // Don't attempt if already connected or already scanning
        if (app->_bluetooth_service->isConnected()) {
            ESP_UTILS_LOGI("Already connected, stopping reconnection timer");
            app->stopReconnectionTimer();
            return;
        }
        
        if (app->_bluetooth_service->isScanning()) {
            ESP_UTILS_LOGD("Still scanning, skipping reconnection attempt");
            return;
        }
        
        // Try to reconnect to the last disconnected device
        if (!app->_last_disconnected_address.empty()) {
            ESP_UTILS_LOGI("Auto-reconnection: attempting to reconnect to %s", 
                          app->_last_disconnected_address.c_str());
            app->_bluetooth_service->connectToDevice(app->_last_disconnected_address);
        } else {
            ESP_UTILS_LOGW("Auto-reconnection: no last disconnected address available");
            app->stopReconnectionTimer();
        }
    }, 5000, this);  // Retry every 5 seconds
}

void RoboCatEars::stopReconnectionTimer()
{
    if (_reconnection_timer) {
        ESP_UTILS_LOGI("Stopping auto-reconnection timer");
        lv_timer_del(_reconnection_timer);
        _reconnection_timer = nullptr;
    }
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, RoboCatEars, APP_NAME, []()
{
    return std::shared_ptr<RoboCatEars>(RoboCatEars::requestInstance(true, false), [](RoboCatEars * p) {});
})

} // namespace esp_brookesia::apps
