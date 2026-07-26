/*
 * Description: Animation mode service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "animation_mode_service.hpp"
#include "bluetooth_service.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cstring>
#include <memory>

static const char *TAG = "AnimationModeService";

namespace robo_cat_ears {

// AnimationModeData binary pack/unpack implementation
std::string AnimationModeData::pack() const
{
    std::string packed;

    // Pack each value as 2 bytes (int16_t)
    packed.push_back((mode_id >> 8) & 0xFF);
    packed.push_back(mode_id & 0xFF);
    packed.push_back((frequency >> 8) & 0xFF);
    packed.push_back(frequency & 0xFF);

    return packed;
}

bool AnimationModeData::unpack(const std::string &packed, AnimationModeData &data)
{
    // Minimum size: mode_id + frequency = 4 bytes
    if (packed.length() < 4) {
        return false;
    }

    size_t offset = 0;

    // Unpack mode_id (2 bytes)
    data.mode_id = (packed[offset] << 8) | packed[offset + 1];
    offset += 2;

    // Unpack frequency (2 bytes)
    data.frequency = (packed[offset] << 8) | packed[offset + 1];

    return true;
}

AnimationModeService *AnimationModeService::_instance = nullptr;

AnimationModeService *AnimationModeService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new AnimationModeService();
    }
    return _instance;
}

AnimationModeService::AnimationModeService()
    : _initialized(false)
{
}

AnimationModeService::~AnimationModeService()
{
    deinit();
}

bool AnimationModeService::init()
{
    if (_initialized) {
        ESP_LOGD(TAG, "Animation mode service already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing animation mode service");

    // Initialize with default values
    _current_data.mode_id = 0;
    _current_data.frequency = 0;
    _initialized = true;
    ESP_LOGI(TAG, "Animation mode service initialized successfully");

    return true;
}

void AnimationModeService::deinit()
{
    if (!_initialized) {
        return;
    }

    ESP_LOGD(TAG, "Deinitializing animation mode service");

    _current_data.mode_id = 0;
    _current_data.frequency = 0;
    _initialized = false;
}

bool AnimationModeService::writeAnimationModeData(const AnimationModeData *data)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Animation mode service not initialized");
        return false;
    }

    if (!data) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return false;
    }

    // Pack to binary format
    std::string packed_data = data->pack();
    if (packed_data.empty()) {
        ESP_LOGE(TAG, "Failed to pack animation mode data");
        return false;
    }

    ESP_LOGI(TAG, "Writing animation mode data via DataPacket: mode_id=%d frequency=%d packed_size=%zu bytes",
             data->mode_id, data->frequency, packed_data.size());

    // Get Bluetooth service instance
    BluetoothService *bt_service = BluetoothService::getInstance();
    if (!bt_service) {
        ESP_LOGE(TAG, "Failed to get Bluetooth service instance");
        return false;
    }

    // Create DataPacket with ANIMATION_MODE type
    DataPacket packet;
    packet.type = DataType::ANIMATION_MODE;
    packet.data = packed_data;

    // Write DataPacket to ABF1 characteristic
    if (!bt_service->writeDataPacket(packet)) {
        ESP_LOGE(TAG, "Failed to write animation mode DataPacket");
        return false;
    }

    // Update cached data
    _current_data = *data;

    return true;
}

bool AnimationModeService::readAnimationModeData(AnimationModeData *data, AnimationModeService::DataLoadedCallback callback)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Animation mode service not initialized");
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

    ESP_LOGI(TAG, "Reading animation mode data from device via ABF2 characteristic");

    // Capture callback in a shared pointer so it can be safely copied
    auto callback_ptr = std::make_shared<DataLoadedCallback>(callback);

    // Initiate read request with minimal callback
    bool request_sent = bt_service->readDataPacket(DataType::ANIMATION,
        [callback_ptr](bool success, DataType type, const uint8_t* buffer, size_t length) {
            // Log raw data for debugging
            ESP_LOGI(TAG, "Raw data received (length=%zu):", length);
            for (size_t i = 0; i < length; i++) {
                ESP_LOGI(TAG, "Byte[%zu]: 0x%02x", i, buffer[i]);
            }

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
                    if (type != DataType::ANIMATION) {
                        ESP_LOGE(TAG, "Invalid data type in response: got 0x%02x", async_data->buffer[0]);
                        delete[] async_data->buffer;
                        delete async_data;
                        return;
                    }

                    // Create string from data portion (skip type byte)
                    std::string packet_data((const char*)&async_data->buffer[1], async_data->length - 1);

                    // Log the raw bytes for debugging
                    ESP_LOGI(TAG, "Raw packed data (%zu bytes): mode_id=0x%02x%02x frequency=0x%02x%02x",
                             packet_data.length(),
                             packet_data.length() > 0 ? (uint8_t)packet_data[0] : 0,
                             packet_data.length() > 1 ? (uint8_t)packet_data[1] : 0,
                             packet_data.length() > 2 ? (uint8_t)packet_data[2] : 0,
                             packet_data.length() > 3 ? (uint8_t)packet_data[3] : 0);

                    // Unpack into a temporary AnimationModeData
                    AnimationModeData temp_data;
                    if (AnimationModeData::unpack(packet_data, temp_data)) {
                        ESP_LOGI(TAG, "Successfully unpacked animation mode data: mode_id=%d, frequency=%d",
                                 temp_data.mode_id, temp_data.frequency);
                        // Update cached data in AnimationModeService
                        AnimationModeService::getInstance()->_current_data = temp_data;
                        ESP_LOGI(TAG, "Successfully read animation mode data from device");

                        // Invoke callback if provided
                        if (async_data->callback && *async_data->callback) {
                            (*async_data->callback)(temp_data);
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to unpack animation mode data");
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
