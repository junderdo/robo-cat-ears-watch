
/*
 * Description: System status and I2C implementation for hardware monitoring
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "system/status.h"
#include "bsp/esp-bsp.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

#define TAG "sysinfo"
#define AXP2101_SLAVE_ADDRESS 0x34

// PMU interrupt and I2C config
#define PMU_INPUT_PIN (gpio_num_t) 10  // Adjust if needed
#define PMU_INPUT_PIN_SEL (1ULL << PMU_INPUT_PIN)

#define I2C_MASTER_NUM 0
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_SDA_IO BSP_I2C_SDA  // GPIO 15
#define I2C_MASTER_SCL_IO BSP_I2C_SCL  // GPIO 14
#define I2C_MASTER_TIMEOUT_MS 1000

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t pmu_dev_handle = NULL;
static QueueHandle_t gpio_evt_queue = NULL;

esp_err_t i2c_init() {
    // Get the I2C bus handle from BSP (already initialized for touch controller)
    i2c_bus_handle = bsp_i2c_get_handle();
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get BSP I2C handle");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Got BSP I2C handle: %p", i2c_bus_handle);

    // Add PMU as a device on the existing I2C bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x34,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0
        }
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &pmu_dev_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PMU device added to I2C bus, handle: %p", pmu_dev_handle);
        
        // Scan I2C bus to see what devices are present
        ESP_LOGI(TAG, "Scanning I2C bus for devices...");
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            i2c_device_config_t scan_config = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = I2C_MASTER_FREQ_HZ,
            };
            i2c_master_dev_handle_t scan_handle = NULL;
            if (i2c_master_bus_add_device(i2c_bus_handle, &scan_config, &scan_handle) == ESP_OK) {
                uint8_t test_data = 0;
                if (i2c_master_receive(scan_handle, &test_data, 1, I2C_MASTER_TIMEOUT_MS) == ESP_OK) {
                    ESP_LOGI(TAG, "  Found device at address 0x%02X", addr);
                }
                i2c_master_bus_rm_device(scan_handle);
            }
        }
        
        // Read multiple registers from 0x34 to try to identify the chip
        ESP_LOGI(TAG, "Reading registers from device at 0x34:");
        uint8_t reg_data[16];
        for (uint8_t reg = 0x00; reg < 0x10; reg++) {
            if (i2c_master_transmit_receive(pmu_dev_handle, &reg, 1, &reg_data[reg], 1, I2C_MASTER_TIMEOUT_MS) == ESP_OK) {
                ESP_LOGI(TAG, "  Reg 0x%02X = 0x%02X", reg, reg_data[reg]);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to add PMU device: 0x%x (%s)", ret, esp_err_to_name(ret));
    }
    return ret;
}

// PMU read function using new API
int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len) {
    if (pmu_dev_handle == NULL) {
        ESP_LOGE(TAG, "PMU device handle is NULL!");
        return -1;
    }
    esp_err_t ret = i2c_master_transmit_receive(pmu_dev_handle, &regAddr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PMU READ FAILED! addr=0x%02x reg=0x%02x Error: 0x%x (%s)", devAddr, regAddr, ret, esp_err_to_name(ret));
        return -1;
    }
    return 0;
}

// PMU write function using new API
int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len) {
    if (pmu_dev_handle == NULL) {
        ESP_LOGE(TAG, "PMU device handle is NULL!");
        return -1;
    }
    uint8_t *buffer = (uint8_t *)malloc(len + 1);
    if (!buffer) return -1;
    buffer[0] = regAddr;
    memcpy(&buffer[1], data, len);

    esp_err_t ret = i2c_master_transmit(pmu_dev_handle, buffer, len + 1, I2C_MASTER_TIMEOUT_MS);
    free(buffer);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PMU WRITE FAILED! addr=0x%02x reg=0x%02x Error: 0x%x (%s)", devAddr, regAddr, ret, esp_err_to_name(ret));
        return -1;
    }
    return 0;
}

// PMU event task
void pmu_hander_task(void *args) {
    while (1) {
        // PMU handler can be extended if needed
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ==================== SystemStatus Class Implementation ====================

SystemStatus::SystemStatus() : pmu_instance(nullptr), initialized(false) {
}

SystemStatus::~SystemStatus() {
    if (pmu_instance) {
        delete static_cast<XPowersPMU*>(pmu_instance);
        pmu_instance = nullptr;
    }
}

esp_err_t SystemStatus::init() {
    if (initialized) {
        ESP_LOGW(TAG, "SystemStatus already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SystemStatus (PMU in read-only mode for battery monitoring)...");
    
    // Initialize I2C if not already done
    if (i2c_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C");
        return ESP_FAIL;
    }
    
    // Give the PMU time to stabilize after power-on
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create PMU instance
    pmu_instance = new (std::nothrow) XPowersPMU();
    if (!pmu_instance) {
        ESP_LOGE(TAG, "Failed to allocate PMU instance");
        return ESP_FAIL;
    }
    
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    
    // Initialize PMU object for I2C communication (read-only)
    ESP_LOGI(TAG, "Calling PMU.begin() for I2C setup...");
    bool begin_result = pmu->begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte);
    
    if (!begin_result) {
        ESP_LOGW(TAG, "PMU.begin() returned false - continuing anyway");
        // Even if begin() fails, the I2C callbacks are set up, so we can still read values
    } else {
        ESP_LOGI(TAG, "PMU.begin() succeeded");
    }
    
    // Enable battery monitoring measurements (safe - these don't change power channels)
    pmu->enableVbusVoltageMeasure();
    pmu->enableBattVoltageMeasure();
    pmu->enableSystemVoltageMeasure();
    pmu->enableTemperatureMeasure();
    
    // Disable TS pin to avoid charging issues (safe - doesn't affect display power)
    pmu->disableTSPinMeasure();
    
    initialized = true;
    ESP_LOGI(TAG, "SystemStatus init complete - read-only battery monitoring enabled");
    
    // Log current battery status (if available)
    if (pmu->isBatteryConnect()) {
        ESP_LOGI(TAG, "Battery connected: %d%%, %d mV", 
                 pmu->getBatteryPercent(), pmu->getBattVoltage());
    } else {
        ESP_LOGI(TAG, "Battery not connected");
    }
    
    return ESP_OK;
}

int SystemStatus::getBatteryPercent() {
    if (!initialized || !pmu_instance) {
        return -1;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    if (!pmu->isBatteryConnect()) {
        return -1;
    }
    return pmu->getBatteryPercent();
}

int SystemStatus::getBatteryVoltage() {
    if (!initialized || !pmu_instance) {
        return 0;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    return pmu->getBattVoltage();
}

int SystemStatus::getVbusVoltage() {
    if (!initialized || !pmu_instance) {
        return -1;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    return pmu->getVbusVoltage();
}

int SystemStatus::getSystemVoltage() {
    if (!initialized || !pmu_instance) {
        return -1;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    return pmu->getSystemVoltage();
}

float SystemStatus::getTemperature() {
    if (!initialized || !pmu_instance) {
        return -999.0f;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    return pmu->getTemperature();
}

bool SystemStatus::isCharging() {
    if (!initialized || !pmu_instance) {
        return false;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    return pmu->isCharging();
}

bool SystemStatus::isBatteryConnected() {
    if (!initialized || !pmu_instance) {
        return false;
    }
    XPowersPMU* pmu = static_cast<XPowersPMU*>(pmu_instance);
    return pmu->isBatteryConnect();
}