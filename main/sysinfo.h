/*
 * Description: System information and I2C interface for hardware monitoring
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SYSINFO_H
#define SYSINFO_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize I2C master bus for PMU communication
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t i2c_init(void);

/**
 * @brief Read register from PMU via I2C
 * 
 * @param devAddr Device address (unused, configured in i2c_init)
 * @param regAddr Register address to read from
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return int 0 on success, -1 on error
 */
int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);

/**
 * @brief Write data to PMU register via I2C
 * 
 * @param devAddr Device address (unused, configured in i2c_init)
 * @param regAddr Register address to write to
 * @param data Data to write
 * @param len Number of bytes to write
 * @return int 0 on success, -1 on error
 */
int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);

/**
 * @brief FreeRTOS task that periodically polls PMU status
 * 
 * @param args Task arguments (unused)
 */
void pmu_hander_task(void *args);

#ifdef __cplusplus
}
#endif

#endif // SYSINFO_H
