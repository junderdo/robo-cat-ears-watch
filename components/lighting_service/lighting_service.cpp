/*
 * Description: Lighting service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lighting_service.hpp"
#include "bluetooth_service.hpp"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>
#include <algorithm>

static const char *TAG = "LightingService";

namespace robo_cat_ears {

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
    
    // Convert to JSON
    std::string json = lightingDataToJson(data);
    if (json.empty()) {
        ESP_LOGE(TAG, "Failed to convert lighting data to JSON");
        return false;
    }
    
    ESP_LOGI(TAG, "Writing lighting data to ABF2: %s", json.c_str());
    
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
    
    // Get ABF2 characteristic handle
    uint16_t abf2_handle = bt_service->getCharHandleABF2();
    if (abf2_handle == 0) {
        ESP_LOGE(TAG, "ABF2 characteristic not discovered");
        return false;
    }
    
    // Write to ABF2 characteristic
    if (!bt_service->writeCharacteristic(abf2_handle, json)) {
        ESP_LOGE(TAG, "Failed to write to ABF2 characteristic");
        return false;
    }
    
    // Update cached data
    _current_data = *data;
    
    return true;
}

bool LightingService::readLightingData(LightingData *data)
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
    
    // TODO: Implement read from ABF2 characteristic
    // This will need to be added to bluetooth_service
    // For now, return the cached data
    ESP_LOGW(TAG, "Read from characteristic not yet implemented, returning cached data");
    *data = _current_data;
    
    return true;
}

std::string LightingService::lightingDataToJson(const LightingData *data)
{
    if (!data) {
        return "";
    }
    
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return "";
    }
    
    // Add mode
    cJSON_AddStringToObject(root, "mode", modeToString(data->mode));
    
    // Add speed
    cJSON_AddNumberToObject(root, "speed", data->speed);
    
    // Add colors array
    cJSON *colors_array = cJSON_CreateArray();
    if (!colors_array) {
        ESP_LOGE(TAG, "Failed to create colors array");
        cJSON_Delete(root);
        return "";
    }
    
    for (const auto &color : data->colors) {
        cJSON *color_obj = cJSON_CreateObject();
        if (!color_obj) {
            continue;
        }
        cJSON_AddNumberToObject(color_obj, "r", color.r);
        cJSON_AddNumberToObject(color_obj, "g", color.g);
        cJSON_AddNumberToObject(color_obj, "b", color.b);
        cJSON_AddItemToArray(colors_array, color_obj);
    }
    
    cJSON_AddItemToObject(root, "colors", colors_array);
    
    // Convert to string
    char *json_str = cJSON_PrintUnformatted(root);
    std::string result;
    if (json_str) {
        result = std::string(json_str);
        free(json_str);
    }
    
    cJSON_Delete(root);
    
    return result;
}

bool LightingService::jsonToLightingData(const std::string &json, LightingData *data)
{
    if (!data) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return false;
    }
    
    if (json.empty()) {
        ESP_LOGE(TAG, "Empty JSON string");
        return false;
    }
    
    cJSON *root = cJSON_Parse(json.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", json.c_str());
        return false;
    }
    
    // Parse mode
    cJSON *mode_item = cJSON_GetObjectItem(root, "mode");
    if (mode_item && cJSON_IsString(mode_item)) {
        data->mode = stringToMode(mode_item->valuestring);
    } else {
        ESP_LOGW(TAG, "Mode not found in JSON, using default");
        data->mode = LightingMode::SOLID;
    }
    
    // Parse speed
    cJSON *speed_item = cJSON_GetObjectItem(root, "speed");
    if (speed_item && cJSON_IsNumber(speed_item)) {
        data->speed = (uint8_t)std::clamp(speed_item->valueint, 1, 100);
    } else {
        ESP_LOGW(TAG, "Speed not found in JSON, using default");
        data->speed = 50;
    }
    
    // Parse colors
    data->colors.clear();
    cJSON *colors_array = cJSON_GetObjectItem(root, "colors");
    if (colors_array && cJSON_IsArray(colors_array)) {
        int count = cJSON_GetArraySize(colors_array);
        int max_colors = std::min(count, 32);  // Limit to 32 colors
        
        for (int i = 0; i < max_colors; i++) {
            cJSON *color_obj = cJSON_GetArrayItem(colors_array, i);
            if (!color_obj || !cJSON_IsObject(color_obj)) {
                continue;
            }
            
            cJSON *r_item = cJSON_GetObjectItem(color_obj, "r");
            cJSON *g_item = cJSON_GetObjectItem(color_obj, "g");
            cJSON *b_item = cJSON_GetObjectItem(color_obj, "b");
            
            if (r_item && g_item && b_item && 
                cJSON_IsNumber(r_item) && cJSON_IsNumber(g_item) && cJSON_IsNumber(b_item)) {
                RGBColor color(
                    (uint8_t)std::clamp(r_item->valueint, 0, 255),
                    (uint8_t)std::clamp(g_item->valueint, 0, 255),
                    (uint8_t)std::clamp(b_item->valueint, 0, 255)
                );
                data->colors.push_back(color);
            }
        }
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Parsed lighting data: mode=%s, speed=%d, colors=%zu",
             modeToString(data->mode), data->speed, data->colors.size());
    
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
