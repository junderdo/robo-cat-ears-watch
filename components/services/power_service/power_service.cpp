/*
 * Description: Power ladder service implementation for Robo cat ears controller
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "power_service.hpp"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "PowerService";

namespace robo_cat_ears {

namespace {

constexpr uint32_t IDLE_TIMEOUT_MS = 15000;
constexpr uint32_t TICK_PERIOD_MS = 500;

// Long enough that dragging the slider settles first, short enough that the
// value survives a battery pull moments later.
constexpr uint32_t BRIGHTNESS_SAVE_DELAY_MS = 300;

constexpr const char *NVS_NAMESPACE = "power";
constexpr const char *NVS_KEY_SCREEN_BRIGHT = "screen_bright";

} // namespace

PowerService *PowerService::_instance = nullptr;

PowerService *PowerService::getInstance()
{
    if (_instance == nullptr) {
        _instance = new PowerService();
    }
    return _instance;
}

PowerService::PowerService()
    : _rung(Rung::Active)
    , _holds(0)
    , _screen_brightness(SCREEN_BRIGHTNESS_MAX)
    , _tick_timer(nullptr)
    , _brightness_save_timer(nullptr)
    , _rung_changed_callback(nullptr)
{
}

bool PowerService::init()
{
    if (_tick_timer) {
        return true;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS init status: %s (may already be initialized)", esp_err_to_name(err));
    }

    loadScreenBrightnessFromNvs();
    applyScreenBrightness();

    _tick_timer = lv_timer_create([](lv_timer_t *t) {
        static_cast<PowerService *>(lv_timer_get_user_data(t))->tick();
    }, TICK_PERIOD_MS, this);

    ESP_LOGI(TAG, "Power ladder running, screen brightness %u%%", _screen_brightness);

    return _tick_timer != nullptr;
}

void PowerService::holdActive(HoldReason reason)
{
    _holds |= static_cast<uint32_t>(reason);
}

void PowerService::releaseActive(HoldReason reason)
{
    _holds &= ~static_cast<uint32_t>(reason);
}

bool PowerService::setScreenBrightness(uint8_t percent)
{
    if (percent < SCREEN_BRIGHTNESS_MIN) {
        percent = SCREEN_BRIGHTNESS_MIN;
    } else if (percent > SCREEN_BRIGHTNESS_MAX) {
        percent = SCREEN_BRIGHTNESS_MAX;
    }

    _screen_brightness = percent;
    scheduleScreenBrightnessSave();

    return applyScreenBrightness();
}

void PowerService::tick()
{
    lv_disp_t *disp = lv_disp_get_default();
    if (!disp) {
        return;
    }

    uint32_t inactive_time = lv_disp_get_inactive_time(disp);

    if (_rung == Rung::Active) {
        if (_holds == 0 && inactive_time >= IDLE_TIMEOUT_MS) {
            ESP_LOGI(TAG, "Descending to Idle after %lu ms of inactivity", inactive_time);
            enterRung(Rung::Idle);
        }
    } else if (inactive_time < IDLE_TIMEOUT_MS) {
        ESP_LOGI(TAG, "Rising to Active on touch activity");
        enterRung(Rung::Active);
    }
}

void PowerService::enterRung(Rung next)
{
    if (next == _rung) {
        return;
    }

    Rung previous = _rung;
    _rung = next;

    if (next == Rung::Active) {
        bsp_display_sleep(false);
        // bsp_display_sleep(false) ends by forcing the panel to 100%, so the
        // user's level has to be put back after every wake.
        applyScreenBrightness();
    } else {
        bsp_display_sleep(true);
    }

    if (_rung_changed_callback) {
        _rung_changed_callback(previous, next);
    }
}

bool PowerService::applyScreenBrightness()
{
    if (_rung != Rung::Active) {
        return true;
    }

    return bsp_display_brightness_set(_screen_brightness) == ESP_OK;
}

void PowerService::scheduleScreenBrightnessSave()
{
    if (_brightness_save_timer) {
        lv_timer_del(static_cast<lv_timer_t *>(_brightness_save_timer));
        _brightness_save_timer = nullptr;
    }

    _brightness_save_timer = lv_timer_create([](lv_timer_t *t) {
        PowerService *service = static_cast<PowerService *>(lv_timer_get_user_data(t));
        service->_brightness_save_timer = nullptr;
        service->saveScreenBrightnessToNvs();
    }, BRIGHTNESS_SAVE_DELAY_MS, this);
    lv_timer_set_repeat_count(static_cast<lv_timer_t *>(_brightness_save_timer), 1);
}

void PowerService::loadScreenBrightnessFromNvs()
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGD(TAG, "No stored screen brightness in NVS");
        return;
    }

    uint8_t stored = SCREEN_BRIGHTNESS_MAX;
    if (nvs_get_u8(handle, NVS_KEY_SCREEN_BRIGHT, &stored) == ESP_OK) {
        if (stored < SCREEN_BRIGHTNESS_MIN) {
            stored = SCREEN_BRIGHTNESS_MIN;
        } else if (stored > SCREEN_BRIGHTNESS_MAX) {
            stored = SCREEN_BRIGHTNESS_MAX;
        }
        _screen_brightness = stored;
    }

    nvs_close(handle);
}

void PowerService::saveScreenBrightnessToNvs()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_u8(handle, NVS_KEY_SCREEN_BRIGHT, _screen_brightness);

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
}

} // namespace robo_cat_ears
