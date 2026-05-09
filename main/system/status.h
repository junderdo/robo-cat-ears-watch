/*
 * Description: System status and I2C interface for hardware monitoring
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

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

/**
 * @brief SystemStatus class - provides interface for system information
 * 
 * This class manages the AXP2101 PMU (Power Management Unit) and provides
 * methods to query battery status, charging state, voltages, and temperature.
 */
class SystemStatus {
public:
    /**
     * @brief Construct a new SystemStatus object
     */
    SystemStatus();

    /**
     * @brief Destroy the SystemStatus object
     */
    ~SystemStatus();

    /**
     * @brief Initialize the PMU and I2C communication
     * 
     * @return esp_err_t ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t init();

    /**
     * @brief Get the current battery percentage
     * 
     * @return int Battery percentage (0-100), or -1 if battery not connected
     */
    int getBatteryPercent();

    /**
     * @brief Get the battery voltage
     * 
     * @return int Battery voltage in millivolts, or 0 if not available
     */
    int getBatteryVoltage();

    /**
     * @brief Get the VBUS voltage (USB power input)
     * 
     * @return int VBUS voltage in millivolts, or -1 if not available
     */
    int getVbusVoltage();

    /**
     * @brief Get the system voltage
     * 
     * @return int System voltage in millivolts, or -1 if not available
     */
    int getSystemVoltage();

    /**
     * @brief Get the PMU temperature
     * 
     * @return float Temperature in Celsius, or -999.0 if not available
     */
    float getTemperature();

    /**
     * @brief Check if the battery is currently charging
     * 
     * @return bool true if charging, false otherwise
     */
    bool isCharging();

    /**
     * @brief Check if battery is connected
     * 
     * @return bool true if battery is connected, false otherwise
     */
    bool isBatteryConnected();

private:
    void *pmu_instance;  // Opaque pointer to XPowersPMU instance
    bool initialized;
};

#endif

#endif // SYSTEM_STATUS_H
