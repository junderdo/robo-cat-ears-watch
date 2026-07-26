/*
 * Description: Animation mode service for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <cstdint>
#include <functional>

namespace robo_cat_ears {

/**
 * @brief Animation mode data structure
 */
struct AnimationModeData {
    int16_t mode_id;
    int16_t frequency;

    AnimationModeData() : mode_id(0), frequency(0) {}

    /**
     * @brief Pack animation mode data into binary format for efficient BLE transmission
     *
     * Format: [mode_id:2bytes][frequency:2bytes]
     * Max size: 2 + 2 = 4 bytes
     *
     * @return Packed binary string
     */
    std::string pack() const;

    /**
     * @brief Unpack binary data into AnimationModeData structure
     *
     * @param packed Binary packed string
     * @param data Output AnimationModeData structure
     * @return true if unpacking successful, false otherwise
     */
    static bool unpack(const std::string &packed, AnimationModeData &data);
};

/**
 * @brief Animation mode service class
 *
 * Manages animation mode data and communicates with the Bluetooth characteristic ABF1 using binary packed format
 */
class AnimationModeService {
public:
    using DataLoadedCallback = std::function<void(const AnimationModeData &data)>;

    /**
     * @brief Get singleton instance
     */
    static AnimationModeService *getInstance();

    /**
     * @brief Initialize the animation mode service
     *
     * @return true if successful, false otherwise
     */
    bool init();

    /**
     * @brief Deinitialize the animation mode service
     */
    void deinit();

    /**
     * @brief Write animation mode data to the ABF1 characteristic using binary packed format
     *
     * @param data Pointer to animation mode data structure
     * @return true if successful, false otherwise
     */
    bool writeAnimationModeData(const AnimationModeData *data);

    /**
     * @brief Read animation mode data from the ABF2 characteristic (async)
     *
     * @param data Pointer to animation mode data structure to populate with cached data immediately
     * @param callback Optional callback invoked when actual data is loaded from device
     * @return true if read request sent successfully, false otherwise
     */
    bool readAnimationModeData(AnimationModeData *data, DataLoadedCallback callback = nullptr);

    /**
     * @brief Get the current animation mode data (cached)
     *
     * @return Reference to current animation mode data
     */
    const AnimationModeData& getCurrentAnimationModeData() const { return _current_data; }

private:
    AnimationModeService();
    ~AnimationModeService();

    // Singleton instance
    static AnimationModeService *_instance;

    // Current animation mode data (cached)
    AnimationModeData _current_data;

    // Initialization state
    bool _initialized;
};

} // namespace robo_cat_ears
