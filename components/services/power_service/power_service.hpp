/*
 * Description: Power ladder service for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <functional>

namespace robo_cat_ears {

/**
 * @brief A rung of the power ladder
 *
 * The watch occupies exactly one rung at a time. Rungs descend in order by
 * inactivity and rise straight back to Active on user input. Active and Dimmed
 * are panel-on; Idle and Suspended are panel-off. Suspended is declared but
 * never entered yet.
 */
enum class Rung : uint8_t {
    Active,
    Dimmed,
    Idle,
    Suspended,
};

/**
 * @brief Whether the panel is physically dark on this rung
 *
 * Panel-off is a property of a rung, not a rung of its own: Idle and Suspended
 * are dark, Active and Dimmed are lit.
 */
constexpr bool isPanelOff(Rung rung)
{
    return rung == Rung::Idle || rung == Rung::Suspended;
}

/**
 * @brief Why something is holding the ladder at Active
 *
 * A bitmask rather than a counter so that repeating a hold is idempotent, and
 * so a stuck ladder can be printed as one number.
 */
enum class HoldReason : uint32_t {
    None     = 0,
    Charging = 1u << 0,
};

/**
 * @brief Owns the power ladder, the panel and the screen brightness
 *
 * The rung state machine, its timeouts, panel sleep-in/sleep-out and the
 * user's screen brightness (including its NVS persistence) all live here.
 * Rung transitions are announced through a single callback slot; subscribers
 * do everything the ladder deliberately knows nothing about, such as BLE
 * connection parameters.
 */
class PowerService {
public:
    using RungChangedCallback = std::function<void(Rung from, Rung to)>;

    /// Lowest screen brightness the user can select. Not zero: a black panel
    /// cannot be found by sight, so the slider that dimmed it cannot be found
    /// either. 10% is the lowest step that still reads in a dark room.
    static constexpr uint8_t SCREEN_BRIGHTNESS_MIN = 10;
    static constexpr uint8_t SCREEN_BRIGHTNESS_MAX = 100;

    /**
     * @brief Get singleton instance
     */
    static PowerService *getInstance();

    /**
     * @brief Restore the stored screen brightness and start the ladder
     *
     * Must be called with the LVGL lock held; it creates the ladder's timer.
     *
     * @return true if the ladder is running
     */
    bool init();

    /**
     * @brief The rung the watch is on right now
     */
    Rung rung() const { return _rung; }

    /**
     * @brief Prevent the ladder descending below Active
     *
     * Holds block descent only; they never force an ascent. Taking a hold that
     * is already held does nothing.
     */
    void holdActive(HoldReason reason);

    /**
     * @brief Release a hold taken by holdActive()
     */
    void releaseActive(HoldReason reason);

    /**
     * @brief The user's chosen screen brightness, 0-100
     *
     * The value the user picked, unaffected by auto-dim: dimming changes what
     * the panel shows, never what the user chose.
     */
    uint8_t screenBrightness() const { return _screen_brightness; }

    /**
     * @brief Set the screen brightness the watch uses while Active
     *
     * Applied immediately and persisted once the value stops changing, so a
     * dragged slider does not write NVS on every step. Values are clamped to
     * [SCREEN_BRIGHTNESS_MIN, SCREEN_BRIGHTNESS_MAX].
     *
     * @param percent Requested brightness, 0-100
     * @return true if the panel accepted the new value
     */
    bool setScreenBrightness(uint8_t percent);

    /**
     * @brief Subscribe to rung transitions
     *
     * Single slot, matching the other services: setting a callback replaces
     * any previous one.
     */
    void setRungChangedCallback(RungChangedCallback callback) { _rung_changed_callback = callback; }

private:
    PowerService();

    void tick();
    void enterRung(Rung next);
    bool applyScreenBrightness();
    void scheduleScreenBrightnessSave();
    void loadScreenBrightnessFromNvs();
    void saveScreenBrightnessToNvs();

    static PowerService *_instance;

    Rung _rung;
    uint32_t _holds;
    uint8_t _screen_brightness;
    void *_tick_timer;
    void *_brightness_save_timer;
    RungChangedCallback _rung_changed_callback;
};

} // namespace robo_cat_ears
