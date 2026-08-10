/*
 * Description: Animation store client implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "animation_store_service.hpp"
#include "bluetooth_service.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include <cstring>

static const char *TAG = "AnimationStoreService";

namespace robo_cat_ears {

namespace {

constexpr uint8_t SUB_CAPABILITY = 0x01;
constexpr uint8_t SUB_LIST = 0x02;
constexpr uint8_t SUB_PLAY = 0x05;

constexpr uint8_t STATUS_OK = 0x00;
constexpr uint8_t STATUS_SLOT_OUT_OF_RANGE = 0x03;
constexpr uint8_t STATUS_SLOT_EMPTY = 0x04;

// [type][corr][status_or_sub_opcode][chunk_index][chunk_count]
constexpr size_t FRAME_HEADER_SIZE = 5;

// Fixed part of a LIST entry: [index:1][animation_id:16][name_len:1]
constexpr size_t LIST_ENTRY_FIXED_SIZE = 18;

// A response that never arrives means "unknown", never "failed"
constexpr uint32_t REQUEST_TIMEOUT_MS = 5000;

// How many times a connect-sequence read is tried before we admit defeat
constexpr uint8_t FETCH_ATTEMPT_LIMIT = 2;

} // namespace

AnimationStoreService *AnimationStoreService::_instance = nullptr;

AnimationStoreService *AnimationStoreService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new AnimationStoreService();
    }
    return _instance;
}

AnimationStoreService::AnimationStoreService()
    : _state(AnimationStoreState::NO_CONNECTION)
    , _cached_address("")
    , _watch_stale(false)
    , _last_play_was_stale(false)
    , _subscribe_succeeded(false)
    , _fetch_attempts(0)
    , _pending_sub_opcode(0)
    , _pending_corr(0)
    , _next_corr(0)
    , _rx_length(0)
    , _rx_expected_chunk(0)
    , _rx_status(0)
    , _rx_complete(false)
    , _timeout_timer(nullptr)
    , _changed_callback(nullptr)
    , _session_complete_callback(nullptr)
{
}

void AnimationStoreService::notifyChanged()
{
    if (_changed_callback) {
        _changed_callback();
    }
}

void AnimationStoreService::setState(AnimationStoreState state)
{
    bool was_fetching = _state == AnimationStoreState::FETCHING;
    _state = state;
    notifyChanged();

    // The connect sequence holds the link to itself until it settles, so nothing
    // else may issue a GATT operation before this fires
    if (was_fetching && state != AnimationStoreState::FETCHING && _session_complete_callback) {
        _session_complete_callback();
    }
}

void AnimationStoreService::beginSession(const std::string &device_address)
{
    if (device_address != _cached_address) {
        ESP_LOGI(TAG, "New device %s, discarding any cached slots from %s",
                 device_address.c_str(), _cached_address.c_str());
        _entries.clear();
        _cached_address = device_address;
    }

    stopTimeout();
    _pending_sub_opcode = 0;
    _fetch_attempts = 0;
    _watch_stale = false;
    _last_play_was_stale = false;
    setState(AnimationStoreState::FETCHING);

    BluetoothService *bt = BluetoothService::getInstance();
    bool started = bt && bt->subscribeToIndications(
        [this](const uint8_t *data, size_t length) {
            onIndication(data, length);
        },
        [this](bool success) {
            // BLE task: hop to the LVGL task before touching state or the UI
            _subscribe_succeeded = success;
            lv_async_call([](void *user_data) {
                static_cast<AnimationStoreService *>(user_data)->onSubscribed();
            }, this);
        });

    if (!started) {
        ESP_LOGE(TAG, "Could not subscribe to ABF2");
        setState(AnimationStoreState::FETCH_FAILED);
    }
}

void AnimationStoreService::endSession()
{
    stopTimeout();
    _pending_sub_opcode = 0;

    // The disconnect we asked for; keep saying why rather than blaming the link.
    // Still notify, so the buttons dim on the way out.
    if (_state == AnimationStoreState::VERSION_MISMATCH) {
        notifyChanged();
        return;
    }

    setState(_entries.empty() ? AnimationStoreState::NO_CONNECTION : AnimationStoreState::LINK_LOST);
}

void AnimationStoreService::onSubscribed()
{
    if (!_subscribe_succeeded) {
        ESP_LOGE(TAG, "ABF2 subscription rejected");
        setState(AnimationStoreState::FETCH_FAILED);
        return;
    }

    if (!sendRequest(SUB_CAPABILITY, nullptr, 0)) {
        setState(AnimationStoreState::FETCH_FAILED);
    }
}

bool AnimationStoreService::play(uint8_t slot)
{
    if (_state != AnimationStoreState::READY) {
        return false;
    }

    // Whatever the last tap turned up, this one supersedes it
    if (_last_play_was_stale) {
        _last_play_was_stale = false;
        notifyChanged();
    }

    return sendRequest(SUB_PLAY, &slot, 1);
}

bool AnimationStoreService::sendRequest(uint8_t sub_opcode, const uint8_t *payload, size_t length)
{
    if (_pending_sub_opcode != 0) {
        ESP_LOGW(TAG, "Dropping sub-opcode 0x%02x, 0x%02x still in flight", sub_opcode, _pending_sub_opcode);
        return false;
    }

    BluetoothService *bt = BluetoothService::getInstance();
    if (!bt || !bt->isConnected()) {
        return false;
    }

    _pending_corr = _next_corr++;
    _pending_sub_opcode = sub_opcode;
    _rx_length = 0;
    _rx_expected_chunk = 0;
    _rx_complete = false;

    // Every request this watch sends fits one frame, so chunk_index/count are fixed
    DataPacket packet;
    packet.type = DataType::STORE;
    packet.data.reserve(4 + length);
    packet.data.push_back(static_cast<char>(_pending_corr));
    packet.data.push_back(static_cast<char>(sub_opcode));
    packet.data.push_back(0);
    packet.data.push_back(1);
    if (payload && length > 0) {
        packet.data.append(reinterpret_cast<const char *>(payload), length);
    }

    if (!bt->writeDataPacket(packet, true)) {
        ESP_LOGE(TAG, "Failed to write sub-opcode 0x%02x", sub_opcode);
        _pending_sub_opcode = 0;
        return false;
    }

    startTimeout();
    return true;
}

void AnimationStoreService::onIndication(const uint8_t *data, size_t length)
{
    if (length < FRAME_HEADER_SIZE || data[0] != static_cast<uint8_t>(DataType::STORE)) {
        return;
    }

    if (_pending_sub_opcode == 0 || data[1] != _pending_corr) {
        return;
    }

    uint8_t chunk_index = data[3];
    uint8_t chunk_count = data[4];
    size_t payload_length = length - FRAME_HEADER_SIZE;

    if (chunk_index != _rx_expected_chunk || _rx_length + payload_length > sizeof(_rx_buffer)) {
        _rx_length = 0;
        _rx_expected_chunk = 0;
        return;
    }

    memcpy(_rx_buffer + _rx_length, data + FRAME_HEADER_SIZE, payload_length);
    _rx_length += payload_length;
    _rx_expected_chunk = chunk_index + 1;
    _rx_status = data[2];

    if (_rx_expected_chunk == chunk_count) {
        _rx_complete = true;
        lv_async_call([](void *user_data) {
            static_cast<AnimationStoreService *>(user_data)->handleResponse();
        }, this);
    }
}

void AnimationStoreService::handleResponse()
{
    if (!_rx_complete) {
        return;
    }
    _rx_complete = false;
    stopTimeout();

    uint8_t sub_opcode = _pending_sub_opcode;
    uint8_t status = _rx_status;
    _pending_sub_opcode = 0;

    switch (sub_opcode) {
    case SUB_CAPABILITY:
        onCapabilityResponse(status);
        break;
    case SUB_LIST:
        onListResponse(status);
        break;
    case SUB_PLAY:
        onPlayResponse(status);
        break;
    default:
        break;
    }
}

void AnimationStoreService::onCapabilityResponse(uint8_t status)
{
    if (status != STATUS_OK || _rx_length < 4) {
        ESP_LOGE(TAG, "CAPABILITY failed, status 0x%02x", status);
        setState(AnimationStoreState::FETCH_FAILED);
        return;
    }

    uint8_t version = _rx_buffer[0];
    if (version != ANIMATION_STORE_PROTOCOL_VERSION) {
        _watch_stale = version > ANIMATION_STORE_PROTOCOL_VERSION;
        ESP_LOGE(TAG, "Store protocol v%u, this watch speaks v%u; %s is stale",
                 version, ANIMATION_STORE_PROTOCOL_VERSION, _watch_stale ? "the watch" : "the ears");
        setState(AnimationStoreState::VERSION_MISMATCH);
        BluetoothService *bt = BluetoothService::getInstance();
        if (bt) {
            bt->disconnect();
        }
        return;
    }

    // Anything past the four bytes we know is a later version's addition.
    // A play-only client needs neither figure: it never picks a slot itself,
    // and every request it sends fits one frame.
    ESP_LOGI(TAG, "Store capability: %u slots, %u byte chunks",
             _rx_buffer[1], (static_cast<uint16_t>(_rx_buffer[2]) << 8) | _rx_buffer[3]);

    if (!sendRequest(SUB_LIST, nullptr, 0)) {
        setState(AnimationStoreState::FETCH_FAILED);
    }
}

void AnimationStoreService::onListResponse(uint8_t status)
{
    if (status != STATUS_OK || _rx_length < 1) {
        ESP_LOGE(TAG, "LIST failed, status 0x%02x", status);
        setState(AnimationStoreState::FETCH_FAILED);
        return;
    }

    std::vector<StoredAnimation> entries;
    uint8_t entry_count = _rx_buffer[0];
    size_t offset = 1;

    for (uint8_t i = 0; i < entry_count; i++) {
        if (offset + LIST_ENTRY_FIXED_SIZE > _rx_length) {
            ESP_LOGE(TAG, "LIST truncated at entry %u", i);
            setState(AnimationStoreState::FETCH_FAILED);
            return;
        }

        uint8_t slot = _rx_buffer[offset];
        uint8_t name_len = _rx_buffer[offset + LIST_ENTRY_FIXED_SIZE - 1];
        offset += LIST_ENTRY_FIXED_SIZE;

        if (offset + name_len > _rx_length) {
            ESP_LOGE(TAG, "LIST name truncated at entry %u", i);
            setState(AnimationStoreState::FETCH_FAILED);
            return;
        }

        entries.push_back({slot, std::string(reinterpret_cast<const char *>(&_rx_buffer[offset]), name_len)});
        offset += name_len;
    }

    _entries = std::move(entries);
    ESP_LOGI(TAG, "Store holds %u animations", entry_count);
    setState(AnimationStoreState::READY);
}

void AnimationStoreService::onPlayResponse(uint8_t status)
{
    // No duration on the wire, so an OK is the end of it as far as the UI goes
    if (status == STATUS_OK) {
        return;
    }

    if (status == STATUS_SLOT_EMPTY || status == STATUS_SLOT_OUT_OF_RANGE) {
        ESP_LOGI(TAG, "Slot no longer there, re-reading LIST");
        _last_play_was_stale = true;
        sendRequest(SUB_LIST, nullptr, 0);
        return;
    }

    ESP_LOGE(TAG, "PLAY failed, status 0x%02x", status);
}

void AnimationStoreService::onTimeout()
{
    stopTimeout();

    uint8_t sub_opcode = _pending_sub_opcode;
    _pending_sub_opcode = 0;

    ESP_LOGW(TAG, "Sub-opcode 0x%02x timed out", sub_opcode);

    // A timeout means unknown, never failed. A PLAY may still have played, so
    // ask what is actually there; a read is idempotent, so retry it once before
    // telling the user anything went wrong.
    if (sub_opcode == SUB_PLAY) {
        sendRequest(SUB_LIST, nullptr, 0);
        return;
    }

    if (++_fetch_attempts < FETCH_ATTEMPT_LIMIT && sendRequest(sub_opcode, nullptr, 0)) {
        return;
    }

    setState(AnimationStoreState::FETCH_FAILED);
}

void AnimationStoreService::startTimeout()
{
    if (!_timeout_timer) {
        _timeout_timer = lv_timer_create([](lv_timer_t *t) {
            static_cast<AnimationStoreService *>(lv_timer_get_user_data(t))->onTimeout();
        }, REQUEST_TIMEOUT_MS, this);
    }
    lv_timer_t *timer = static_cast<lv_timer_t *>(_timeout_timer);
    lv_timer_reset(timer);
    lv_timer_resume(timer);
}

void AnimationStoreService::stopTimeout()
{
    if (_timeout_timer) {
        lv_timer_pause(static_cast<lv_timer_t *>(_timeout_timer));
    }
}

} // namespace robo_cat_ears
