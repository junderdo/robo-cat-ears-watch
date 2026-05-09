/*
 * Description: AXP2101 Power Management Unit (PMU) interface
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <cstring>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "port_axp2101.h"
#include "sysinfo.h"

static const char *TAG = "PMU";

static XPowersPMU PMU;

esp_err_t pmu_init()
{
    // SAFE MODE: Initialize PMU for READ-ONLY battery monitoring
    // DO NOT call any disable/enable/set functions - bootloader configured power correctly
    ESP_LOGI(TAG, "Initializing PMU in read-only mode for battery monitoring...");
    
    // Give the PMU time to stabilize after power-on
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize PMU object for I2C communication (read-only)
    ESP_LOGI(TAG, "Calling PMU.begin() for I2C setup...");
    bool begin_result = PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte);
    
    if (!begin_result) {
        ESP_LOGW(TAG, "PMU.begin() returned false - continuing anyway");
        // Even if begin() fails, the I2C callbacks are set up, so we can still read values
    } else {
        ESP_LOGI(TAG, "PMU.begin() succeeded");
    }
    
    // Enable battery monitoring measurements (safe - these don't change power channels)
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    PMU.enableTemperatureMeasure();
    
    // Disable TS pin to avoid charging issues (safe - doesn't affect display power)
    PMU.disableTSPinMeasure();
    
    ESP_LOGI(TAG, "PMU init complete - read-only battery monitoring enabled");
    
    // Log current battery status (if available)
    if (PMU.isBatteryConnect()) {
        ESP_LOGI(TAG, "Battery connected: %d%%, %d mV", 
                 PMU.getBatteryPercent(), PMU.getBattVoltage());
    } else {
        ESP_LOGI(TAG, "Battery not connected");
    }
    
    return ESP_OK;
}

void pmu_isr_handler()
{
    ESP_LOGI(TAG, "PMU ISR handler called");
    // ISR functionality can be added here if needed
}

int pmu_get_battery_percent()
{
    if (!PMU.isBatteryConnect()) {
        return -1;
    }
    return PMU.getBatteryPercent();
}

int pmu_get_battery_voltage()
{
    return PMU.getBattVoltage();
}

int pmu_get_vbus_voltage()
{
    return PMU.getVbusVoltage();
}

int pmu_get_system_voltage()
{
    return PMU.getSystemVoltage();
}

float pmu_get_temperature()
{
    return PMU.getTemperature();
}

bool pmu_is_charging()
{
    return PMU.isCharging();
}
