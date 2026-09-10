/*
 * Description: System Info app implementation - AXP2101 PMU data and watch settings
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:SystemInfo"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_system_info.hpp"
#include "system/status.h"
#include "power_service.hpp"

#define APP_NAME "SYSTEM_INFO"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_system_info_112_112);

namespace esp_brookesia::apps {

SystemInfo *SystemInfo::_instance = nullptr;

SystemInfo *SystemInfo::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new SystemInfo(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

SystemInfo::SystemInfo(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, &esp_brookesia_app_icon_launcher_system_info_112_112, true, use_status_bar, use_navigation_bar),
    _info_label(nullptr),
    _screen_brightness_label(nullptr),
    _screen_brightness_slider(nullptr),
    _system_status(nullptr)
{
}

SystemInfo::~SystemInfo()
{
    if (_system_status) {
        delete _system_status;
        _system_status = nullptr;
    }
}

bool SystemInfo::run(void)
{
    ESP_UTILS_LOGD("Run");

    // Create SystemStatus instance for this app
    _system_status = new (std::nothrow) SystemStatus();
    if (!_system_status) {
        ESP_LOGE(ESP_UTILS_LOG_TAG, "Failed to allocate SystemStatus");
        return false;
    }
    
    // Initialize SystemStatus (reuses the I2C bus already initialized)
    if (_system_status->init() != ESP_OK) {
        ESP_LOGW(ESP_UTILS_LOG_TAG, "Failed to initialize SystemStatus - will show placeholder data");
    }

    // Get the active screen
    lv_obj_t *screen = lv_scr_act();

    // Set black background
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Create a label to display system information
    _info_label = lv_label_create(screen);
    lv_obj_set_style_text_color(_info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_info_label, &lv_font_montserrat_20, 0);
    lv_obj_set_width(_info_label, lv_pct(90));
    lv_label_set_long_mode(_info_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(_info_label, LV_ALIGN_TOP_MID, 0, 20);

    createSettings(screen, 220);

    // Initial update of system information
    updateSystemInfo();

    return true;
}

bool SystemInfo::back(void)
{
    ESP_UTILS_LOGD("Back");

    // Notify core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

void SystemInfo::createSettings(lv_obj_t *screen, int16_t y_offset)
{
    robo_cat_ears::PowerService *power = robo_cat_ears::PowerService::getInstance();

    lv_obj_t *heading = lv_label_create(screen);
    lv_label_set_text(heading, "Settings");
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, y_offset);

    _screen_brightness_label = lv_label_create(screen);
    lv_obj_set_style_text_color(_screen_brightness_label, lv_color_hex(0xC0C0C0), 0);
    lv_obj_set_style_text_font(_screen_brightness_label, &lv_font_montserrat_20, 0);
    lv_obj_align(_screen_brightness_label, LV_ALIGN_TOP_MID, 0, y_offset + 40);

    _screen_brightness_slider = lv_slider_create(screen);
    lv_obj_set_size(_screen_brightness_slider, lv_pct(70), 24);
    lv_obj_align(_screen_brightness_slider, LV_ALIGN_TOP_MID, 0, y_offset + 80);
    lv_slider_set_range(_screen_brightness_slider,
                        robo_cat_ears::PowerService::SCREEN_BRIGHTNESS_MIN,
                        robo_cat_ears::PowerService::SCREEN_BRIGHTNESS_MAX);
    lv_slider_set_value(_screen_brightness_slider, power->screenBrightness(), LV_ANIM_OFF);

    lv_obj_add_event_cb(_screen_brightness_slider, [](lv_event_t *e) {
        SystemInfo *app = (SystemInfo *)lv_event_get_user_data(e);
        if (!app) {
            return;
        }

        int32_t percent = lv_slider_get_value(app->_screen_brightness_slider);
        robo_cat_ears::PowerService::getInstance()->setScreenBrightness((uint8_t)percent);
        app->updateScreenBrightnessLabel();
    }, LV_EVENT_VALUE_CHANGED, this);

    updateScreenBrightnessLabel();
}

void SystemInfo::updateScreenBrightnessLabel()
{
    if (!_screen_brightness_label) {
        return;
    }

    char text[48];
    snprintf(text, sizeof(text), "Screen Brightness  %u%%",
             (unsigned)robo_cat_ears::PowerService::getInstance()->screenBrightness());
    lv_label_set_text(_screen_brightness_label, text);
}

void SystemInfo::updateSystemInfo()
{
    if (!_info_label || !_system_status) {
        return;  // UI or SystemStatus not initialized yet
    }

    // Get PMU data from SystemStatus
    int battery_voltage = _system_status->getBatteryVoltage();
    int vbus_voltage = _system_status->getVbusVoltage();
    int system_voltage = _system_status->getSystemVoltage();
    float temperature = _system_status->getTemperature();

    // Format the information text
    char watch_info_text[512];
    snprintf(watch_info_text, sizeof(watch_info_text),
             "Watch Sys Info\n\n"
             "Battery: %d mV\n"
             "VBUS: %d mV\n"
             "System: %d mV\n"
             "Temperature: %.1f°C",
             battery_voltage,
             vbus_voltage,
             system_voltage,
             temperature);


    // Get data from the ears system status if available
    // TODO: get ears system status from the RoboCatEars app or shared service if available
    int ears_battery_voltage = 0;
    int ears_vbus_voltage = 0;
    int ears_system_voltage = 0;
    float ears_temperature = 0.0f;
    char ears_info_text[512];
    snprintf(ears_info_text, sizeof(ears_info_text),
             "Ears Sys Info\n\n"
             "Battery: %d mV\n"
             "VBUS: %d mV\n"
             "System: %d mV\n"
             "Temperature: %.1f°C",
             ears_battery_voltage,
             ears_vbus_voltage,
             ears_system_voltage,
             ears_temperature);

    // Store the text in user_data temporarily before async call
    char *text_copy = strdup(watch_info_text);
    lv_obj_set_user_data(_info_label, text_copy);

    // Update using lv_async_call for thread safety (in case called from timer)
    lv_async_call([](void *user_data) {
        auto *app = (SystemInfo *)user_data;
        if (app && app->_info_label) {
            char *text = (char *)lv_obj_get_user_data(app->_info_label);
            if (text) {
                lv_label_set_text(app->_info_label, text);
                free(text);
                lv_obj_set_user_data(app->_info_label, nullptr);
            }
        }
    }, this);
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, SystemInfo, APP_NAME, []()
{
    return std::shared_ptr<SystemInfo>(SystemInfo::requestInstance(), [](SystemInfo * p) {});
})

} // namespace esp_brookesia::apps
