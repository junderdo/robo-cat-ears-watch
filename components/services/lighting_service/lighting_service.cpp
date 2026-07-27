/*
 * Description: Lighting service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lighting_service.hpp"
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

static const char *TAG = "LightingService";

namespace robo_cat_ears {

// LightingData binary pack/unpack implementation
std::string LightingData::pack() const
{
    std::string packed;
    packed.reserve(3 + (colors.size() * 3));  // mode + speed + num_colors + RGB data
    
    // Pack mode (1 byte)
    packed.push_back(static_cast<uint8_t>(mode));
    
    // Pack speed (1 byte)
    packed.push_back(speed);
    
    // Pack number of colors (1 byte, clamped to max 32)
    uint8_t num_colors = static_cast<uint8_t>(std::min(colors.size(), size_t(32)));
    packed.push_back(num_colors);
    
    // Pack each color as RGB (3 bytes each)
    for (size_t i = 0; i < num_colors; i++) {
        packed.push_back(colors[i].r);
        packed.push_back(colors[i].g);
        packed.push_back(colors[i].b);
    }
    
    return packed;
}

bool LightingData::unpack(const std::string &packed, LightingData &data)
{
    // Minimum size: mode + speed + num_colors = 3 bytes
    if (packed.length() < 3) {
        return false;
    }
    
    size_t offset = 0;
    
    // Unpack mode (1 byte)
    data.mode = static_cast<LightingMode>(packed[offset++]);
    
    // Validate mode
    if (data.mode < LightingMode::SOLID || data.mode > LightingMode::RAIN) {
        return false;
    }
    
    // Unpack speed (1 byte)
    data.speed = packed[offset++];
    if (data.speed < 1 || data.speed > 100) {
        data.speed = std::clamp(data.speed, uint8_t(1), uint8_t(100));
    }
    
    // Unpack number of colors (1 byte)
    uint8_t num_colors = packed[offset++];
    if (num_colors > 32) {
        return false;  // Invalid color count
    }
    
    // Verify we have enough data for all colors
    size_t expected_size = 3 + (num_colors * 3);
    if (packed.length() != expected_size) {
        return false;
    }
    
    // Unpack colors (3 bytes each: RGB)
    data.colors.clear();
    data.colors.reserve(num_colors);
    
    for (uint8_t i = 0; i < num_colors; i++) {
        uint8_t r = packed[offset++];
        uint8_t g = packed[offset++];
        uint8_t b = packed[offset++];
        data.colors.emplace_back(r, g, b);
    }
    
    return true;
}

LightingService *LightingService::_instance = nullptr;

LightingService *LightingService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new LightingService();
    }
    return _instance;
}

LightingService::LightingService()
    : _initialized(false)
{
}

LightingService::~LightingService()
{
    deinit();
}

bool LightingService::init()
{
    if (_initialized) {
        ESP_LOGD(TAG, "Lighting service already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing lighting service");
    
    // Initialize with default values
    _current_data.mode = LightingMode::SOLID;
    _current_data.speed = 50;
    _current_data.colors.clear();
    
    _initialized = true;
    ESP_LOGI(TAG, "Lighting service initialized successfully");
    
    return true;
}

void LightingService::deinit()
{
    if (!_initialized) {
        return;
    }
    
    ESP_LOGD(TAG, "Deinitializing lighting service");
    
    _current_data.colors.clear();
    _initialized = false;
}

bool LightingService::writeLightingData(const LightingData *data)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Lighting service not initialized");
        return false;
    }
    
    if (!data) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return false;
    }
    
    // Validate data
    if (data->colors.size() > 32) {
        ESP_LOGE(TAG, "Too many colors (max 32, got %zu)", data->colors.size());
        return false;
    }
    
    // Pack to binary format
    std::string packed_data = data->pack();
    if (packed_data.empty()) {
        ESP_LOGE(TAG, "Failed to pack lighting data");
        return false;
    }
    
    ESP_LOGI(TAG, "Writing lighting data via DataPacket: mode=%d, speed=%d, colors=%zu, packed_size=%zu bytes",
             static_cast<int>(data->mode), data->speed, data->colors.size(), packed_data.size());
    
    // Get Bluetooth service instance
    BluetoothService *bt_service = BluetoothService::getInstance();
    if (!bt_service) {
        ESP_LOGE(TAG, "Failed to get Bluetooth service instance");
        return false;
    }
    
    // Create DataPacket with LIGHTING type
    DataPacket packet;
    packet.type = DataType::LIGHTING;
    packet.data = packed_data;
    
    // Write DataPacket to ABF1 characteristic
    if (!bt_service->writeDataPacket(packet)) {
        ESP_LOGE(TAG, "Failed to write lighting DataPacket");
        return false;
    }
    
    // Update cached data
    _current_data = *data;
    
    return true;
}

bool LightingService::readLightingData(LightingData *data, DataLoadedCallback callback)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Lighting service not initialized");
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
    
    ESP_LOGI(TAG, "Reading lighting data from device via ABF2 characteristic");
    
    // Capture callback in a shared pointer so it can be safely copied
    auto callback_ptr = std::make_shared<DataLoadedCallback>(callback);
    
    // Initiate read request with minimal callback
    bool request_sent = bt_service->readDataPacket(DataType::LIGHTING, 
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
                    if (type != DataType::LIGHTING) {
                        ESP_LOGE(TAG, "Invalid data type in response: got 0x%02x", async_data->buffer[0]);
                        delete[] async_data->buffer;
                        delete async_data;
                        return;
                    }
                    
                    // Create string from data portion (skip type byte)
                    std::string packet_data((const char*)&async_data->buffer[1], async_data->length - 1);
                    
                    // Log the raw bytes for debugging
                    ESP_LOGI(TAG, "Raw packed data (%zu bytes): mode=0x%02x speed=0x%02x num_colors=0x%02x",
                             packet_data.length(),
                             packet_data.length() > 0 ? (uint8_t)packet_data[0] : 0,
                             packet_data.length() > 1 ? (uint8_t)packet_data[1] : 0,
                             packet_data.length() > 2 ? (uint8_t)packet_data[2] : 0);
                    
                    // Unpack into a temporary LightingData
                    LightingData temp_data;
                    if (LightingData::unpack(packet_data, temp_data)) {
                        ESP_LOGI(TAG, "Successfully unpacked lighting data: mode=%d, speed=%d, colors=%zu",
                                 static_cast<int>(temp_data.mode), temp_data.speed, temp_data.colors.size());
                        // Update cached data in LightingService
                        LightingService::getInstance()->_current_data = temp_data;
                        ESP_LOGI(TAG, "Successfully read lighting data from device");
                        
                        // Invoke callback if provided
                        if (async_data->callback && *async_data->callback) {
                            (*async_data->callback)(temp_data);
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to unpack lighting data");
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

const char* LightingService::modeToString(LightingMode mode)
{
    switch (mode) {
        case LightingMode::SOLID: return "Solid";
        case LightingMode::BREATHING: return "Breathing";
        case LightingMode::MARQUEE: return "Marquee";
        case LightingMode::CHASING: return "Chasing";
        case LightingMode::RAIN: return "Rain";
        default: return "Solid";
    }
}

LightingMode LightingService::stringToMode(const std::string &mode_str)
{
    if (mode_str == "Solid") return LightingMode::SOLID;
    if (mode_str == "Breathing") return LightingMode::BREATHING;
    if (mode_str == "Marquee") return LightingMode::MARQUEE;
    if (mode_str == "Chasing") return LightingMode::CHASING;
    if (mode_str == "Rain") return LightingMode::RAIN;
    return LightingMode::SOLID;  // Default
}

} // namespace robo_cat_ears
