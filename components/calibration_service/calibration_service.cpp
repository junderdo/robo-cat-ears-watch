/*
 * Description: Calibration service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "calibration_service.hpp"
#include "bluetooth_service.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cstring>
#include <algorithm>
#include <memory>

static const char *TAG = "CalibrationService";

namespace robo_cat_ears {

// CalibrationData binary pack/unpack implementation
std::string CalibrationData::pack() const
{
    std::string packed;

    // Pack each value as 2 bytes (int16_t)
    packed.push_back((left_azi >> 8) & 0xFF);
    packed.push_back(left_azi & 0xFF);
    packed.push_back((left_lat >> 8) & 0xFF);
    packed.push_back(left_lat & 0xFF);
    packed.push_back((right_azi >> 8) & 0xFF);
    packed.push_back(right_azi & 0xFF);
    packed.push_back((right_lat >> 8) & 0xFF);
    packed.push_back(right_lat & 0xFF);
    
    return packed;
}

bool CalibrationData::unpack(const std::string &packed, CalibrationData &data)
{
    // Minimum size: left_azi + left_lat + right_azi + right_lat = 8 bytes
    if (packed.length() < 8) {
        return false;
    }
    
    size_t offset = 0;
    
    // Unpack left_azi (2 bytes)
    data.left_azi = (packed[offset] << 8) | packed[offset + 1];
    offset += 2;
    
    // Unpack left_lat (2 bytes)
    data.left_lat = (packed[offset] << 8) | packed[offset + 1];
    offset += 2;
    
    // Unpack right_azi (2 bytes)
    data.right_azi = (packed[offset] << 8) | packed[offset + 1];
    offset += 2;
    
    // Unpack right_lat (2 bytes)
    data.right_lat = (packed[offset] << 8) | packed[offset + 1];
    
    return true;
}

CalibrationService *CalibrationService::_instance = nullptr;

CalibrationService *CalibrationService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new CalibrationService();
    }
    return _instance;
}

CalibrationService::CalibrationService()
    : _initialized(false)
{
}

CalibrationService::~CalibrationService()
{
    deinit();
}

bool CalibrationService::init()
{
    if (_initialized) {
        ESP_LOGD(TAG, "Calibration service already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing calibration service");
    
    // Initialize with default values
    _current_data.left_azi = 0;
    _current_data.left_lat = 0;
    _current_data.right_azi = 0;
    _current_data.right_lat = 0;
    _initialized = true;
    ESP_LOGI(TAG, "Calibration service initialized successfully");
    
    return true;
}

void CalibrationService::deinit()
{
    if (!_initialized) {
        return;
    }
    
    ESP_LOGD(TAG, "Deinitializing calibration service");
    
    _current_data.left_azi = 0;
    _current_data.left_lat = 0;
    _current_data.right_azi = 0;
    _current_data.right_lat = 0;
    _initialized = false;
}

bool CalibrationService::writeCalibrationData(const CalibrationData *data)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Calibration service not initialized");
        return false;
    }
    
    if (!data) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return false;
    }
    
    // Pack to binary format
    std::string packed_data = data->pack();
    if (packed_data.empty()) {
        ESP_LOGE(TAG, "Failed to pack calibration data");
        return false;
    }
    
    ESP_LOGI(TAG, "Writing calibration data via DataPacket: left_azi=%d left_lat=%d right_azi=%d right_lat=%d packed_size=%zu bytes",
             data->left_azi, data->left_lat, data->right_azi, data->right_lat, packed_data.size());
    
    // Get Bluetooth service instance
    BluetoothService *bt_service = BluetoothService::getInstance();
    if (!bt_service) {
        ESP_LOGE(TAG, "Failed to get Bluetooth service instance");
        return false;
    }
    
    // Create DataPacket with CALIBRATION type
    DataPacket packet;
    packet.type = DataType::CALIBRATION;
    packet.data = packed_data;
    
    // Write DataPacket to ABF1 characteristic
    if (!bt_service->writeDataPacket(packet)) {
        ESP_LOGE(TAG, "Failed to write calibration DataPacket");
        return false;
    }
    
    // Update cached data
    _current_data = *data;
    
    return true;
}

bool CalibrationService::readCalibrationData(CalibrationData *data, CalibrationService::DataLoadedCallback callback)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Calibration service not initialized");
        return false;
    }
    
    if (!data) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return false;
    }
    
    // Get Bluetooth service instance
    BluetoothService *bt_service = BluetoothService::getInstance();
    if (!bt_service) {
        ESP_LOGE(TAG, "Failed to get Bluetooth service instance");
        return false;
    }
    
    // Check if connected
    if (!bt_service->isConnected()) {
        ESP_LOGE(TAG, "Not connected to device");
        return false;
    }
    
    // Check if ABF2 characteristic is available
    if (bt_service->getCharHandleABF2() == 0) {
        ESP_LOGE(TAG, "ABF2 characteristic not discovered");
        return false;
    }
    
    ESP_LOGI(TAG, "Reading calibration data from device via ABF2 characteristic");
    
    // Capture callback in a shared pointer so it can be safely copied
    auto callback_ptr = std::make_shared<DataLoadedCallback>(callback);
    
    // Initiate read request with minimal callback
    bool request_sent = bt_service->readDataPacket(DataType::CALIBRATION, 
        [callback_ptr](bool success, DataType type, const uint8_t* buffer, size_t length) {
            // Called from BLE task - NO logging, NO allocations allowed here
            // Allocate a copy of the buffer to pass to async call
            if (success && length > 0 && length <= 256) {
                uint8_t* buffer_copy = new uint8_t[length];
                for (size_t i = 0; i < length; i++) {
                    buffer_copy[i] = buffer[i];
                }
                
                // Package the data for async processing
                struct AsyncData {
                    uint8_t* buffer;
                    size_t length;
                    std::shared_ptr<DataLoadedCallback> callback;
                };
                AsyncData* async_data = new AsyncData{buffer_copy, length, callback_ptr};
                
                // Use LVGL async call to defer processing to LVGL task
                lv_async_call([](void* user_data) {
                    AsyncData* async_data = static_cast<AsyncData*>(user_data);
                    
                    if (async_data->length < 2) {
                        ESP_LOGE(TAG, "Read failed or invalid length");
                        delete[] async_data->buffer;
                        delete async_data;
                        return;
                    }
                    
                    DataType type = static_cast<DataType>(async_data->buffer[0]);
                    if (type != DataType::CALIBRATION) {
                        ESP_LOGE(TAG, "Invalid data type in response: got 0x%02x", async_data->buffer[0]);
                        delete[] async_data->buffer;
                        delete async_data;
                        return;
                    }
                    
                    // Create string from data portion (skip type byte)
                    std::string packet_data((const char*)&async_data->buffer[1], async_data->length - 1);
                    
                    // Log the raw bytes for debugging
                    ESP_LOGI(TAG, "Raw packed data (%zu bytes): left_azi=0x%02x left_lat=0x%02x right_azi=0x%02x right_lat=0x%02x",
                             packet_data.length(),
                             packet_data.length() > 0 ? (uint8_t)packet_data[0] : 0,
                             packet_data.length() > 1 ? (uint8_t)packet_data[1] : 0,
                             packet_data.length() > 2 ? (uint8_t)packet_data[2] : 0,
                             packet_data.length() > 3 ? (uint8_t)packet_data[3] : 0);
                    
                    // Unpack into a temporary CalibrationData
                    CalibrationData temp_data;
                    if (CalibrationData::unpack(packet_data, temp_data)) {
                        ESP_LOGI(TAG, "Successfully unpacked calibration data: left_azi=%d, left_lat=%d, right_azi=%d, right_lat=%d",
                                 temp_data.left_azi, temp_data.left_lat, temp_data.right_azi, temp_data.right_lat);
                        // Update cached data in CalibrationService
                        CalibrationService::getInstance()->_current_data = temp_data;
                        ESP_LOGI(TAG, "Successfully read calibration data from device");
                        
                        // Invoke callback if provided
                        if (async_data->callback && *async_data->callback) {
                            (*async_data->callback)(temp_data);
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to unpack calibration data");
                    }
                    
                    delete[] async_data->buffer;
                    delete async_data;
                }, async_data);
            }
        });
    
    if (!request_sent) {
        ESP_LOGE(TAG, "Failed to initiate read request");
        return false;
    }
    
    ESP_LOGI(TAG, "Read request sent, callback will process data when response arrives");
    
    // Copy cached data to output parameter (caller can use this immediately)
    if (data) {
        *data = _current_data;
    }
    
    return true;
}

} // namespace robo_cat_ears
