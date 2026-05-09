/*
 * Description: AXP2101 Power Management Unit (PMU) interface
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PORT_AXP2101_H
#define PORT_AXP2101_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the AXP2101 PMU
 * 
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t pmu_init(void);

/**
 * @brief Handle PMU interrupt and log status information
 */
void pmu_isr_handler(void);

/**
 * @brief Get the current battery percentage
 * 
 * @return int Battery percentage (0-100), or -1 if battery not connected
 */
int pmu_get_battery_percent(void);

/**
 * @brief Get the battery voltage
 * 
 * @return int Battery voltage in millivolts, or 0 if not available
 */
int pmu_get_battery_voltage(void);

/**
 * @brief Get the VBUS voltage (USB power input)
 * 
 * @return int VBUS voltage in millivolts, or -1 if not available
 */
int pmu_get_vbus_voltage(void);

/**
 * @brief Get the system voltage
 * 
 * @return int System voltage in millivolts, or -1 if not available
 */
int pmu_get_system_voltage(void);

/**
 * @brief Get the PMU temperature
 * 
 * @return float Temperature in Celsius, or -999.0 if not available
 */
float pmu_get_temperature(void);

/**
 * @brief Check if the battery is currently charging
 * 
 * @return bool true if charging, false otherwise
 */
bool pmu_is_charging(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_AXP2101_H
