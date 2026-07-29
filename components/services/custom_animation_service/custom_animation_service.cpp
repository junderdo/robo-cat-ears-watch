/*
 * Description: Custom keyframe animation service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "custom_animation_service.hpp"
#include "bluetooth_service.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cstdint>

static const char *TAG = "CustomAnimationService";

namespace robo_cat_ears {

namespace {

constexpr const char *NVS_NAMESPACE = "custom_anims";
constexpr uint8_t MAX_SERVO_ANGLE = 180;
constexpr size_t MAX_NAME_LENGTH = 31;

// Unacknowledged writes can outrun the BLE controller if sent back to back
constexpr uint32_t CHUNK_GAP_MS = 20;

/**
 * @brief Build the NVS key for a slot, e.g. "anim3"
 */
std::string slotKey(uint8_t slot)
{
    return "anim" + std::to_string(slot);
}

void appendUint16(std::string &out, uint16_t value)
{
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>(value & 0xFF));
}

uint16_t readUint16(const std::string &in, size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint8_t>(in[offset]) << 8) | static_cast<uint8_t>(in[offset + 1]));
}

} // namespace

bool CustomAnimationData::isValid() const
{
    if (keyframes.empty() || keyframes.size() > CUSTOM_ANIMATION_MAX_KEYFRAMES) {
        return false;
    }

    uint16_t previous_time_ms = 0;
    for (size_t i = 0; i < keyframes.size(); i++) {
        const AnimationKeyframe &kf = keyframes[i];

        for (uint8_t ch = 0; ch < 4; ch++) {
            if (kf.angles[ch] > MAX_SERVO_ANGLE) {
                return false;
            }
        }

        if (kf.ease_in_type > EaseType::ELASTIC || kf.ease_out_type > EaseType::ELASTIC) {
            return false;
        }

        if (i > 0 && kf.time_ms < previous_time_ms) {
            return false;
        }
        previous_time_ms = kf.time_ms;
    }

    return true;
}

uint16_t CustomAnimationData::durationMs() const
{
    if (keyframes.empty()) {
        return 0;
    }
    return keyframes.back().time_ms - keyframes.front().time_ms;
}

std::string CustomAnimationData::pack() const
{
    if (!isValid()) {
        return std::string();
    }

    std::string packed;
    packed.reserve(1 + keyframes.size() * CUSTOM_ANIMATION_KEYFRAME_SIZE);

    packed.push_back(static_cast<char>(keyframes.size()));

    for (const AnimationKeyframe &kf : keyframes) {
        appendUint16(packed, kf.time_ms);
        packed.push_back(static_cast<char>(kf.angles[0]));
        packed.push_back(static_cast<char>(kf.angles[1]));
        packed.push_back(static_cast<char>(kf.angles[2]));
        packed.push_back(static_cast<char>(kf.angles[3]));
        packed.push_back(static_cast<char>(kf.ease_in_type));
        packed.push_back(static_cast<char>(kf.ease_out_type));
        appendUint16(packed, kf.ease_in_ms);
        appendUint16(packed, kf.ease_out_ms);
    }

    return packed;
}

bool CustomAnimationData::unpack(const std::string &packed, CustomAnimationData &data)
{
    if (packed.empty()) {
        return false;
    }

    uint8_t count = static_cast<uint8_t>(packed[0]);
    if (count == 0 || count > CUSTOM_ANIMATION_MAX_KEYFRAMES) {
        return false;
    }

    if (packed.length() < 1 + count * CUSTOM_ANIMATION_KEYFRAME_SIZE) {
        return false;
    }

    data.keyframes.clear();
    data.keyframes.reserve(count);

    size_t offset = 1;
    for (uint8_t i = 0; i < count; i++) {
        AnimationKeyframe kf;

        kf.time_ms = readUint16(packed, offset);
        offset += 2;
        kf.angles[0] = static_cast<uint8_t>(packed[offset++]);
        kf.angles[1] = static_cast<uint8_t>(packed[offset++]);
        kf.angles[2] = static_cast<uint8_t>(packed[offset++]);
        kf.angles[3] = static_cast<uint8_t>(packed[offset++]);
        kf.ease_in_type = static_cast<EaseType>(packed[offset++]);
        kf.ease_out_type = static_cast<EaseType>(packed[offset++]);
        kf.ease_in_ms = readUint16(packed, offset);
        offset += 2;
        kf.ease_out_ms = readUint16(packed, offset);
        offset += 2;

        data.keyframes.push_back(kf);
    }

    return data.isValid();
}

CustomAnimationService *CustomAnimationService::_instance = nullptr;

CustomAnimationService *CustomAnimationService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new CustomAnimationService();
    }
    return _instance;
}

CustomAnimationService::CustomAnimationService()
    : _next_transfer_id(0)
    , _initialized(false)
{
}

CustomAnimationService::~CustomAnimationService()
{
}

bool CustomAnimationService::init()
{
    if (_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing custom animation service");
    _initialized = true;

    return true;
}

bool CustomAnimationService::saveAnimation(uint8_t slot, const CustomAnimationData &animation)
{
    if (slot >= CUSTOM_ANIMATION_MAX_SLOTS) {
        ESP_LOGE(TAG, "Slot %d out of range (0-%d)", slot, CUSTOM_ANIMATION_MAX_SLOTS - 1);
        return false;
    }

    if (!animation.isValid()) {
        ESP_LOGE(TAG, "Refusing to save invalid animation '%s'", animation.name.c_str());
        return false;
    }

    // Stored record: [name_len:1][name][wire format body]
    std::string name = animation.name.substr(0, MAX_NAME_LENGTH);
    std::string record;
    record.push_back(static_cast<char>(name.length()));
    record.append(name);
    record.append(animation.pack());

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(nvs_handle, slotKey(slot).c_str(), record.data(), record.length());
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save animation to slot %d: %s", slot, esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Saved animation '%s' to slot %d (%d keyframes, %zu bytes)",
             name.c_str(), slot, (int)animation.keyframes.size(), record.length());
    return true;
}

bool CustomAnimationService::loadAnimation(uint8_t slot, CustomAnimationData &animation)
{
    if (slot >= CUSTOM_ANIMATION_MAX_SLOTS) {
        ESP_LOGE(TAG, "Slot %d out of range (0-%d)", slot, CUSTOM_ANIMATION_MAX_SLOTS - 1);
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No stored animations yet: %s", esp_err_to_name(err));
        return false;
    }

    size_t record_size = 0;
    err = nvs_get_blob(nvs_handle, slotKey(slot).c_str(), nullptr, &record_size);
    if (err != ESP_OK || record_size < 2) {
        ESP_LOGD(TAG, "Slot %d is empty", slot);
        nvs_close(nvs_handle);
        return false;
    }

    std::string record(record_size, '\0');
    err = nvs_get_blob(nvs_handle, slotKey(slot).c_str(), &record[0], &record_size);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot %d: %s", slot, esp_err_to_name(err));
        return false;
    }

    size_t name_len = static_cast<uint8_t>(record[0]);
    if (record.length() < 1 + name_len) {
        ESP_LOGE(TAG, "Slot %d holds a malformed record", slot);
        return false;
    }

    animation.name = record.substr(1, name_len);
    if (!CustomAnimationData::unpack(record.substr(1 + name_len), animation)) {
        ESP_LOGE(TAG, "Slot %d holds an unreadable animation", slot);
        return false;
    }

    ESP_LOGI(TAG, "Loaded animation '%s' from slot %d (%d keyframes)",
             animation.name.c_str(), slot, (int)animation.keyframes.size());
    return true;
}

bool CustomAnimationService::playAnimation(const CustomAnimationData &animation)
{
    if (!_initialized) {
        ESP_LOGE(TAG, "Custom animation service not initialized");
        return false;
    }

    std::string body = animation.pack();
    if (body.empty()) {
        ESP_LOGE(TAG, "Cannot play invalid animation '%s'", animation.name.c_str());
        return false;
    }

    BluetoothService *bt_service = BluetoothService::getInstance();
    if (!bt_service || !bt_service->isConnected() || bt_service->getCharHandleABF1() == 0) {
        ESP_LOGE(TAG, "Not connected to device");
        return false;
    }

    // One packet is [type:1][chunk header][payload] and must fit a single write
    uint16_t max_write = bt_service->getMaxWriteLength();
    if (max_write <= 1 + CUSTOM_ANIMATION_CHUNK_HEADER_SIZE) {
        ESP_LOGE(TAG, "Negotiated MTU leaves no room for animation data");
        return false;
    }
    size_t max_payload = max_write - 1 - CUSTOM_ANIMATION_CHUNK_HEADER_SIZE;

    size_t chunk_count = (body.length() + max_payload - 1) / max_payload;
    if (chunk_count > UINT8_MAX) {
        ESP_LOGE(TAG, "Animation needs %zu chunks, more than the protocol allows", chunk_count);
        return false;
    }

    uint8_t transfer_id = _next_transfer_id++;

    ESP_LOGI(TAG, "Playing '%s': %d keyframes, %d ms, %zu bytes in %zu chunk(s)",
             animation.name.c_str(), (int)animation.keyframes.size(), animation.durationMs(),
             body.length(), chunk_count);

    for (size_t index = 0; index < chunk_count; index++) {
        size_t offset = index * max_payload;
        size_t length = std::min(max_payload, body.length() - offset);

        DataPacket packet;
        packet.type = DataType::CUSTOM_ANIMATION;
        packet.data.reserve(CUSTOM_ANIMATION_CHUNK_HEADER_SIZE + length);
        packet.data.push_back(static_cast<char>(transfer_id));
        packet.data.push_back(static_cast<char>(index));
        packet.data.push_back(static_cast<char>(chunk_count));
        packet.data.append(body, offset, length);

        if (!bt_service->writeDataPacket(packet)) {
            ESP_LOGE(TAG, "Failed to write chunk %zu of %zu", index + 1, chunk_count);
            return false;
        }

        if (index + 1 < chunk_count) {
            vTaskDelay(pdMS_TO_TICKS(CHUNK_GAP_MS));
        }
    }

    return true;
}

} // namespace robo_cat_ears
