
/*
 * Description: AXP2101 System info
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
#include "sysinfo.h"
#include "port_axp2101.h"
#include "bsp/esp-bsp.h"

#define TAG "sysinfo"

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

// Function declarations
extern esp_err_t pmu_init();
extern void pmu_isr_handler();

// ISR for GPIO
static void IRAM_ATTR pmu_irq_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

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
        pmu_isr_handler();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}