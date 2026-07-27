/*
 * Description: Main boot and initialization function for robo cat ears watch
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"
#include "esp_lib_utils.h"
#include "./dark/stylesheet.hpp"
#include "esp_brookesia_app_system_info.hpp"
#include "esp_wifi.h"

// C includes
#include "system/status.h"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

#define LVGL_PORT_INIT_CONFIG() \
    {                               \
        .task_priority = 4,       \
        .task_stack = 10 * 1024,       \
        .task_affinity = -1,      \
        .task_max_sleep_ms = 500, \
        .timer_period_ms = 5,     \
    }

constexpr bool EXAMPLE_SHOW_MEM_INFO = false;
constexpr uint32_t BACKLIGHT_TIMEOUT_MS = 15000; // 15 seconds
// Set to true to enable serial logging, false to disable
constexpr bool DEBUG_LOG_ENABLED = true;

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Main"

<<<<<<< HEAD
#if !DEBUG_LOG_ENABLED
#   define ESP_UTILS_LOGI(...)   ((void)0)
#   define ESP_UTILS_LOGW(...)   ((void)0)
#   define ESP_UTILS_LOGE(...)   ((void)0)
#   define ESP_UTILS_LOGD(...)   ((void)0)
#endif
=======
// #if !DEBUG_LOG_ENABLED
// #   define ESP_UTILS_LOGI(...)   ((void)0)
// #   define ESP_UTILS_LOGW(...)   ((void)0)
// #   define ESP_UTILS_LOGE(...)   ((void)0)
// #   define ESP_UTILS_LOGD(...)   ((void)0)
// #endif
>>>>>>> main

// Global system status instance
static SystemStatus *g_system_status = nullptr;
Phone *g_phone = nullptr;  // Global phone instance for Bluetooth status updates

extern "C" void app_main(void)
{
    ESP_UTILS_LOGI("Display ESP-Brookesia");

    /* Initialize display BSP - this initializes I2C bus for touch */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = LVGL_PORT_INIT_CONFIG(),
    };
    ESP_UTILS_CHECK_NULL_EXIT(bsp_display_start_with_config(&cfg), "Start display failed");

    /* Turn on backlight */
    ESP_UTILS_CHECK_ERROR_EXIT(bsp_display_backlight_on(), "Turn on display backlight failed");

    /* Initialize SystemStatus AFTER display to avoid I2C conflicts - non-fatal */
    g_system_status = new (std::nothrow) SystemStatus();
    if (g_system_status && g_system_status->init() == ESP_OK) {
        ESP_UTILS_LOGI("SystemStatus initialized for battery monitoring");
    } else {
        ESP_UTILS_LOGW("SystemStatus not available - battery monitoring disabled");
        if (g_system_status) {
            delete g_system_status;
            g_system_status = nullptr;
        }
    }

    // turn off wifi radio to save power
    ESP_UTILS_CHECK_FALSE_EXIT(esp_wifi_stop(), "Stop WiFi failed");
    ESP_UTILS_CHECK_FALSE_EXIT(esp_wifi_deinit(), "Deinit WiFi failed");

    /* Configure GUI lock */
    LvLock::registerCallbacks([](int timeout_ms) {
        if (timeout_ms < 0) {
            timeout_ms = 0;
        } else if (timeout_ms == 0) {
            timeout_ms = 1;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(bsp_display_lock(timeout_ms), false, "Lock failed");

        return true;
    }, []() {
        bsp_display_unlock();

        return true;
    });

    /* Create a phone object */
    Phone *phone = new (std::nothrow) Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");
    g_phone = phone;  // Store for global access

    /* Try using a stylesheet that corresponds to the resolution */
    if ((BSP_LCD_H_RES == 410) && (BSP_LCD_V_RES == 502)) {
        Stylesheet *stylesheet = new (std::nothrow) Stylesheet(STYLESHEET_410_502_DARK);
        ESP_UTILS_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");

        ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
        delete stylesheet;
    }

    {
        // When operating on non-GUI tasks, should acquire a lock before operating on LVGL
        LvLockGuard gui_guard;

        /* Begin the phone */
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");
        // assert(phone->getDisplay().showContainerBorder() && "Show container border failed");

        /* Init and install apps from registry */
        std::vector<systems::base::Manager::RegistryAppInfo> inited_apps;
        ESP_UTILS_CHECK_FALSE_EXIT(phone->initAppFromRegistry(inited_apps), "Init app registry failed");
        
        /* Define app order - Robo Cat Ears appears first */
        std::vector<std::string> app_order = {"ROBO_CAT_EARS", "SYSTEM_INFO"};
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installAppFromRegistry(inited_apps, &app_order), "Install app registry failed");

        /* Set initial WiFi icon state to disconnected (will be used for Bluetooth status) */
        ESP_UTILS_CHECK_FALSE_EXIT(
            phone->getDisplay().getStatusBar()->setWifiIconState(0),
            "Set initial WiFi icon state failed"
        );

        /* Create a task to handle battery status updates */
        lv_timer_create([](lv_timer_t *t) {
            Phone *phone = (Phone *)t->user_data;

            ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");

            /* Update battery percent every 2 seconds */
            if (g_system_status) {
                int battery_percent = g_system_status->getBatteryPercent();
                bool is_charging = g_system_status->isCharging();
                ESP_UTILS_LOGI("Battery: %d%%, Charging: %s", battery_percent, is_charging ? "YES" : "NO");
                if (battery_percent >= 0) {
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        phone->getDisplay().getStatusBar()->setBatteryPercent(is_charging, battery_percent),
                        "Refresh status bar failed"
                    );
                }
            }
            
            /* Update SystemInfo app if it's running */
            esp_brookesia::apps::SystemInfo *system_info_app = esp_brookesia::apps::SystemInfo::requestInstance();
            if (system_info_app) {
                system_info_app->updateSystemInfo();
            }
        }, 2000, phone);

        /* Create a task to handle backlight timeout */
        lv_timer_create([](lv_timer_t *t) {
            static bool backlight_on = true;
            lv_disp_t *disp = lv_disp_get_default();
            
            if (!disp) {
                return;
            }

            uint32_t inactive_time = lv_disp_get_inactive_time(disp);

            /* Turn off backlight after timeout */
            if (backlight_on && inactive_time >= BACKLIGHT_TIMEOUT_MS) {
                ESP_UTILS_LOGI("Turning off backlight due to inactivity (%lu ms)", inactive_time);
                bsp_display_backlight_off();
                backlight_on = false;
            }
            /* Turn on backlight when touch detected */
            else if (!backlight_on && inactive_time < BACKLIGHT_TIMEOUT_MS) {
                ESP_UTILS_LOGI("Turning on backlight due to touch activity");
                bsp_display_backlight_on();
                backlight_on = true;
            }
        }, 500, nullptr);  // Check every 500ms
    }

    if constexpr (EXAMPLE_SHOW_MEM_INFO) {
        esp_utils::thread_config_guard thread_config({
            .name = "mem_info",
            .stack_size = 4096,
        });
        boost::thread([ = ]() {
            char buffer[128];    /* Make sure buffer is enough for `sprintf` */
            size_t internal_free = 0;
            size_t internal_total = 0;
            size_t external_free = 0;
            size_t external_total = 0;

            while (1) {
                internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
                external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
                sprintf(buffer,
                        "\t           Biggest /     Free /    Total\n"
                        "\t  SRAM : [%8d / %8d / %8d]\n"
                        "\t PSRAM : [%8d / %8d / %8d]",
                        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), internal_free, internal_total,
                        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), external_free, external_total);
                ESP_UTILS_LOGI("\n%s", buffer);

                {
                    LvLockGuard gui_guard;
                    ESP_UTILS_CHECK_FALSE_EXIT(
                        phone->getDisplay().getRecentsScreen()->setMemoryLabel(
                            internal_free / 1024, internal_total / 1024, external_free / 1024, external_total / 1024
                        ), "Set memory label failed"
                    );
                }

                boost::this_thread::sleep_for(boost::chrono::seconds(5));
            }
        }).detach();
    }
}