/*
 * Description: Custom keyframe animation service for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace robo_cat_ears {

/**
 * @brief Maximum number of keyframes in one animation
 *
 * Must match CUSTOM_ANIMATION_MAX_KEYFRAMES in the peripheral firmware.
 */
constexpr uint8_t CUSTOM_ANIMATION_MAX_KEYFRAMES = 64;

/**
 * @brief Serialized size of a single keyframe
 */
constexpr size_t CUSTOM_ANIMATION_KEYFRAME_SIZE = 12;

/**
 * @brief Size of the per-chunk header: [transfer_id:1][chunk_index:1][chunk_count:1]
 */
constexpr size_t CUSTOM_ANIMATION_CHUNK_HEADER_SIZE = 3;

/**
 * @brief Number of animation slots held in NVS
 */
constexpr uint8_t CUSTOM_ANIMATION_MAX_SLOTS = 8;

/**
 * @brief Easing curve applied to a keyframe transition
 */
enum class EaseType : uint8_t {
    NONE = 0,     /*!< Linear */
    SINE = 1,     /*!< Sinusoidal */
    CUBIC = 2,    /*!< Cubic */
    ELASTIC = 3,  /*!< Elastic (overshoots) */
};

/**
 * @brief Servo indices into AnimationKeyframe::angles
 *
 * Must match the SERVO_* channel constants in the peripheral firmware.
 */
enum ServoIndex : uint8_t {
    SERVO_LEFT_AZIMUTH = 0,
    SERVO_LEFT_LATITUDE = 1,
    SERVO_RIGHT_AZIMUTH = 2,
    SERVO_RIGHT_LATITUDE = 3,
};

/**
 * @brief A single animation keyframe
 *
 * The pose of all four servos and the time, relative to the start of the
 * animation, at which they should reach it.
 *
 * Easing straddles the keyframe boundary: the move from the previous keyframe
 * to this one is driven by the previous keyframe's ease_out (departure) and
 * then this keyframe's ease_in (arrival). The two meet exactly halfway between
 * the two poses, and the ears hold at that halfway pose for whatever time the
 * two durations do not cover.
 */
struct AnimationKeyframe {
    uint16_t time_ms = 0;
    uint8_t angles[4] = {90, 90, 90, 90};
    EaseType ease_in_type = EaseType::NONE;
    EaseType ease_out_type = EaseType::NONE;
    uint16_t ease_in_ms = 0;
    uint16_t ease_out_ms = 0;
};

/**
 * @brief A named custom animation
 */
struct CustomAnimationData {
    std::string name;
    std::vector<AnimationKeyframe> keyframes;

    /**
     * @brief Check that the animation is playable
     *
     * Requires at least one keyframe, no more than the maximum, angles within
     * 0-180, and keyframe times that never move backwards.
     */
    bool isValid() const;

    /**
     * @brief Total duration of the animation in milliseconds
     */
    uint16_t durationMs() const;

    /**
     * @brief Pack the keyframes into the binary wire format
     *
     * Format (big-endian):
     *   [keyframe_count:1]
     *   then keyframe_count repetitions of:
     *   [time_ms:2][left_azi:1][left_lat:1][right_azi:1][right_lat:1]
     *   [ease_in_type:1][ease_out_type:1][ease_in_ms:2][ease_out_ms:2]
     *
     * The name is not transmitted; the peripheral does not retain animations.
     *
     * @return Packed binary string, empty if the animation is not valid
     */
    std::string pack() const;

    /**
     * @brief Unpack the binary wire format into keyframes
     *
     * Leaves `name` untouched.
     *
     * @param packed Binary packed string
     * @param data Output structure
     * @return true if unpacking successful, false otherwise
     */
    static bool unpack(const std::string &packed, CustomAnimationData &data);
};

/**
 * @brief Stores custom animations in NVS and streams them to the ears
 *
 * The watch is the only place a custom animation lives. Playing one sends the
 * whole animation over BLE, in chunks sized to the negotiated MTU; the
 * peripheral plays it and does not retain it.
 */
class CustomAnimationService {
public:
    /**
     * @brief Get singleton instance
     */
    static CustomAnimationService *getInstance();

    /**
     * @brief Initialize the custom animation service
     *
     * @return true if successful, false otherwise
     */
    bool init();

    /**
     * @brief Save an animation to an NVS slot
     *
     * @param slot Slot index, 0 to CUSTOM_ANIMATION_MAX_SLOTS - 1
     * @param animation Animation to store
     * @return true if successful, false otherwise
     */
    bool saveAnimation(uint8_t slot, const CustomAnimationData &animation);

    /**
     * @brief Load an animation from an NVS slot
     *
     * @param slot Slot index, 0 to CUSTOM_ANIMATION_MAX_SLOTS - 1
     * @param animation Structure to populate
     * @return true if the slot held a valid animation, false otherwise
     */
    bool loadAnimation(uint8_t slot, CustomAnimationData &animation);

    /**
     * @brief Send an animation to the connected ears and play it
     *
     * Blocks briefly between chunks to keep the writes, which are
     * unacknowledged, from outrunning the BLE controller.
     *
     * @param animation Animation to play
     * @return true if every chunk was written, false otherwise
     */
    bool playAnimation(const CustomAnimationData &animation);

private:
    CustomAnimationService();
    ~CustomAnimationService();

    static CustomAnimationService *_instance;

    // Lets the peripheral tell a restarted transfer from a continuing one
    uint8_t _next_transfer_id;

    bool _initialized;
};

} // namespace robo_cat_ears
