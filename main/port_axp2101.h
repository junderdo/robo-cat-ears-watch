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

#ifdef __cplusplus
}
#endif

#endif // PORT_AXP2101_H
