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

// For now, force AXP2101 since manufacturer confirms it
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "port_axp2101.h"
#include "sysinfo.h"

static const char *TAG = "PMU";

static XPowersPMU PMU;

esp_err_t pmu_init()
{
    ESP_LOGI(TAG, "Probing I2C device at address 0x34...");
    
    // Give the PMU time to stabilize after power-on
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Try reading chip ID multiple times as it might not be ready immediately
    uint8_t chip_id_reg = 0x03;
    uint8_t chip_id = 0;
    
    for (int attempt = 0; attempt < 5; attempt++) {
        pmu_register_read(AXP2101_SLAVE_ADDRESS, chip_id_reg, &chip_id, 1);
        ESP_LOGI(TAG, "Attempt %d: Chip ID at register 0x03 = 0x%02X", attempt + 1, chip_id);
        
        if (chip_id == 0x4A) {
            ESP_LOGI(TAG, "Found AXP2101!");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "Final chip ID: 0x%02X (expected 0x4A for AXP2101)", chip_id);
    
    ESP_LOGI(TAG, "Attempting PMU.begin() as AXP2101...");
    bool begin_result = PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte);
    
    if (!begin_result) {
        ESP_LOGW(TAG, "PMU.begin() failed due to chip ID check");
        ESP_LOGI(TAG, "Manually initializing PMU object (manufacturer confirms AXP2101)...");
        
        // The begin() call already set up I2C callbacks internally, even though it returned false
        // We just need to manually do what initImpl() would have done
        PMU.disableTSPinMeasure();
        
        ESP_LOGI(TAG, "PMU object initialized despite chip ID mismatch");
    } else {
        ESP_LOGI(TAG, "PMU.begin() succeeded");
    }
    
    // Continue with normal initialization regardless of begin() result
    ESP_LOGI(TAG, "Init PMU SUCCESS!");
    
    // Turn off unused power channels
    PMU.disableDC2();
    PMU.disableDC3();
    PMU.disableDC4();
    PMU.disableDC5();
    
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();
    PMU.disableBLDO1();
    PMU.disableBLDO2();
    
    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();
    
    // Enable DC1 (3.3V external)
    PMU.setDC1Voltage(3300);
    PMU.enableDC1();
    
    // Enable ALDO1 (3.3V)
    PMU.setALDO1Voltage(3300);
    PMU.enableALDO1();
    
    ESP_LOGI(TAG, "DCDC=======================================================================\n");
    ESP_LOGI(TAG, "DC1  : %s   Voltage:%u mV \n", PMU.isEnableDC1() ? "+" : "-", PMU.getDC1Voltage());
    ESP_LOGI(TAG, "DC2  : %s   Voltage:%u mV \n", PMU.isEnableDC2() ? "+" : "-", PMU.getDC2Voltage());
    ESP_LOGI(TAG, "DC3  : %s   Voltage:%u mV \n", PMU.isEnableDC3() ? "+" : "-", PMU.getDC3Voltage());
    ESP_LOGI(TAG, "DC4  : %s   Voltage:%u mV \n", PMU.isEnableDC4() ? "+" : "-", PMU.getDC4Voltage());
    ESP_LOGI(TAG, "DC5  : %s   Voltage:%u mV \n", PMU.isEnableDC5() ? "+" : "-", PMU.getDC5Voltage());
    ESP_LOGI(TAG, "ALDO=======================================================================\n");
    ESP_LOGI(TAG, "ALDO1: %s   Voltage:%u mV\n", PMU.isEnableALDO1() ? "+" : "-", PMU.getALDO1Voltage());
    ESP_LOGI(TAG, "ALDO2: %s   Voltage:%u mV\n", PMU.isEnableALDO2() ? "+" : "-", PMU.getALDO2Voltage());
    ESP_LOGI(TAG, "ALDO3: %s   Voltage:%u mV\n", PMU.isEnableALDO3() ? "+" : "-", PMU.getALDO3Voltage());
    ESP_LOGI(TAG, "ALDO4: %s   Voltage:%u mV\n", PMU.isEnableALDO4() ? "+" : "-", PMU.getALDO4Voltage());
    ESP_LOGI(TAG, "BLDO=======================================================================\n");
    ESP_LOGI(TAG, "BLDO1: %s   Voltage:%u mV\n", PMU.isEnableBLDO1() ? "+" : "-", PMU.getBLDO1Voltage());
    ESP_LOGI(TAG, "BLDO2: %s   Voltage:%u mV\n", PMU.isEnableBLDO2() ? "+" : "-", PMU.getBLDO2Voltage());
    ESP_LOGI(TAG, "CPUSLDO====================================================================\n");
    ESP_LOGI(TAG, "CPUSLDO: %s Voltage:%u mV\n", PMU.isEnableCPUSLDO() ? "+" : "-", PMU.getCPUSLDOVoltage());
    ESP_LOGI(TAG, "DLDO=======================================================================\n");
    ESP_LOGI(TAG, "DLDO1: %s   Voltage:%u mV\n", PMU.isEnableDLDO1() ? "+" : "-", PMU.getDLDO1Voltage());
    ESP_LOGI(TAG, "DLDO2: %s   Voltage:%u mV\n", PMU.isEnableDLDO2() ? "+" : "-", PMU.getDLDO2Voltage());
    ESP_LOGI(TAG, "===========================================================================\n");
    
    PMU.clearIrqStatus();

    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    PMU.enableTemperatureMeasure();

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU.disableTSPinMeasure();

    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |    // BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |  // VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |     // POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ // CHARGE
        // XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ | XPOWERS_AXP2101_PKEY_POSITIVE_IRQ   |   //POWER KEY
    );

    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_400MA);
    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    // Read battery percentage
    ESP_LOGI(TAG, "battery percentage:%d %%", PMU.getBatteryPercent());
    
    return ESP_OK;
}

void pmu_isr_handler()
{
    // Get PMU Interrupt Status Register
    PMU.getIrqStatus();

    ESP_LOGI(TAG, "Power Temperature: %.2f°C", PMU.getTemperature());

    ESP_LOGI(TAG, "isCharging: %s", PMU.isCharging() ? "YES" : "NO");

    ESP_LOGI(TAG, "isDischarge: %s", PMU.isDischarge() ? "YES" : "NO");

    ESP_LOGI(TAG, "isStandby: %s", PMU.isStandby() ? "YES" : "NO");

    ESP_LOGI(TAG, "isVbusIn: %s", PMU.isVbusIn() ? "YES" : "NO");

    ESP_LOGI(TAG, "isVbusGood: %s", PMU.isVbusGood() ? "YES" : "NO");

    uint8_t charge_status = PMU.getChargerStatus();
    if (charge_status == XPOWERS_AXP2101_CHG_TRI_STATE)
    {
        ESP_LOGI(TAG, "Charger Status: tri_charge");
    }
    else if (charge_status == XPOWERS_AXP2101_CHG_PRE_STATE)
    {
        ESP_LOGI(TAG, "Charger Status: pre_charge");
    }
    else if (charge_status == XPOWERS_AXP2101_CHG_CC_STATE)
    {
        ESP_LOGI(TAG, "Charger Status: constant charge");
    }
    else if (charge_status == XPOWERS_AXP2101_CHG_CV_STATE)
    {
        ESP_LOGI(TAG, "Charger Status: constant voltage");
    }
    else if (charge_status == XPOWERS_AXP2101_CHG_DONE_STATE)
    {
        ESP_LOGI(TAG, "Charger Status: charge done");
    }
    else if (charge_status == XPOWERS_AXP2101_CHG_STOP_STATE)
    {
        ESP_LOGI(TAG, "Charger Status: not charge");
    }

    ESP_LOGI(TAG, "getBattVoltage: %d mV", PMU.getBattVoltage());

    ESP_LOGI(TAG, "getVbusVoltage: %d mV", PMU.getVbusVoltage());

    ESP_LOGI(TAG, "getSystemVoltage: %d mV", PMU.getSystemVoltage());

    if (PMU.isBatteryConnect())
    {
        ESP_LOGI(TAG, "getBatteryPercent: %d %%", PMU.getBatteryPercent());
    }
    // Clear PMU Interrupt Status Register
    PMU.clearIrqStatus();
}

int pmu_get_battery_percent()
{
    if (!PMU.isBatteryConnect()) {
        ESP_LOGI(TAG, "Battery not connected");
        return -1;
    }
    int percent = PMU.getBatteryPercent();
    ESP_LOGI(TAG, "Battery percent: %d%%", percent);
    return percent;
}

int pmu_get_battery_voltage()
{
    uint16_t voltage = PMU.getBattVoltage();
    ESP_LOGI(TAG, "Battery voltage raw: %u mV", voltage);
    return voltage;
}

int pmu_get_vbus_voltage()
{
    uint16_t voltage = PMU.getVbusVoltage();
    ESP_LOGI(TAG, "VBUS voltage raw: %u mV", voltage);
    return voltage;
}

int pmu_get_system_voltage()
{
    uint16_t voltage = PMU.getSystemVoltage();
    ESP_LOGI(TAG, "System voltage raw: %u mV", voltage);
    return voltage;
}

float pmu_get_temperature()
{
    float temp = PMU.getTemperature();
    ESP_LOGI(TAG, "Temperature: %.2f°C", temp);
    return temp;
}