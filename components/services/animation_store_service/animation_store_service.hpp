/*
 * Description: Animation store client for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace robo_cat_ears {

/**
 * @brief The only store protocol version this watch understands
 *
 * Outside this the watch disconnects rather than degrading, because degrading
 * would mean guessing what changed in a version it has never seen.
 */
constexpr uint8_t ANIMATION_STORE_PROTOCOL_VERSION = 1;

/**
 * @brief One occupied slot in the ears' store
 *
 * The record on the wire also carries the web app's animation id; the watch
 * plays by slot index and has no use for it.
 */
struct StoredAnimation {
    uint8_t slot;
    std::string name;
};

enum class AnimationStoreState {
    NO_CONNECTION,     /*!< Never fetched, or fetched from a device we then left */
    FETCHING,          /*!< Somewhere in subscribe -> CAPABILITY -> LIST */
    READY,             /*!< Entries are current for the connected device */
    FETCH_FAILED,      /*!< The connect sequence did not complete */
    VERSION_MISMATCH,  /*!< Refused the link; see isWatchStale() */
    LINK_LOST,         /*!< Entries are stale but retained so the UI can dim them */
};

/**
 * @brief Reads the ears' custom animation store and plays from it
 *
 * A pure client of the 0x06 surface: it runs the mandatory connect sequence,
 * caches what it read for the life of the connection, and plays by slot. It
 * never stores, deletes or renames, and it persists nothing.
 *
 * The cache is tagged with the device address and dropped on sight when a new
 * connection reports a different one — playing device A's slot indices against
 * device B is the worst state this client could reach.
 */
class AnimationStoreService {
public:
    using ChangedCallback = std::function<void()>;

    static AnimationStoreService *getInstance();

    /**
     * @brief Run the connect sequence: subscribe to ABF2, CAPABILITY, then LIST
     *
     * Called once per connection, not on every screen entry.
     *
     * @param device_address Address of the newly connected device
     */
    void beginSession(const std::string &device_address);

    /**
     * @brief Note that the link dropped
     *
     * Entries are kept, not cleared, so the screen can dim its buttons.
     */
    void endSession();

    /**
     * @brief Play a stored slot on the ears
     *
     * An OK means the ears accepted the command, not that they finished moving,
     * so nothing here reports playback progress.
     *
     * @param slot Slot index from a StoredAnimation
     * @return true if the request was written, false if busy or not ready
     */
    bool play(uint8_t slot);

    AnimationStoreState getState() const { return _state; }
    const std::vector<StoredAnimation> &getEntries() const { return _entries; }

    /**
     * @brief Which side is behind, valid while getState() is VERSION_MISMATCH
     */
    bool isWatchStale() const { return _watch_stale; }

    /**
     * @brief Whether the last play hit a slot the ears no longer hold
     *
     * Set when a PLAY comes back SLOT_EMPTY or SLOT_OUT_OF_RANGE, which means
     * the cached list was stale rather than that anything failed. Cleared by
     * the next successful play or a new session.
     */
    bool wasLastPlayStale() const { return _last_play_was_stale; }

    /**
     * @brief Set the callback fired on the LVGL task whenever the state or entries change
     */
    void setChangedCallback(ChangedCallback callback) { _changed_callback = callback; }

private:
    AnimationStoreService();

    bool sendRequest(uint8_t sub_opcode, const uint8_t *payload, size_t length);
    void onIndication(const uint8_t *data, size_t length);
    void onSubscribed();
    void handleResponse();
    void onCapabilityResponse(uint8_t status);
    void onListResponse(uint8_t status);
    void onPlayResponse(uint8_t status);
    void onTimeout();
    void startTimeout();
    void stopTimeout();
    void notifyChanged();
    void setState(AnimationStoreState state);

    static AnimationStoreService *_instance;

    AnimationStoreState _state;
    std::string _cached_address;
    std::vector<StoredAnimation> _entries;
    uint8_t _slot_count;
    uint16_t _max_chunk_bytes;
    bool _watch_stale;
    bool _last_play_was_stale;
    bool _subscribe_succeeded;

    uint8_t _pending_sub_opcode;  // 0 when nothing is in flight
    uint8_t _pending_corr;
    uint8_t _next_corr;

    // Written by the BLE task, read by the LVGL task once _rx_complete is set.
    // A LIST response is the only chunked one, at 801 bytes worst case.
    uint8_t _rx_buffer[1024];
    size_t _rx_length;
    uint8_t _rx_expected_chunk;
    uint8_t _rx_status;
    volatile bool _rx_complete;

    void *_timeout_timer;
    ChangedCallback _changed_callback;
};

} // namespace robo_cat_ears
