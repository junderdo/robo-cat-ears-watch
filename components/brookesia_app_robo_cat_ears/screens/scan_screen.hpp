/*
 * Description: Scan screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "lvgl.h"
#include <functional>

namespace esp_brookesia::apps::screens {

/**
 * @brief Scan screen containing device list and connection UI
 */
class ScanScreen {
public:
    /**
     * @brief Constructor - creates the scan screen UI
     *
     * @param parent_screen The parent LVGL screen object
     * @param on_scan_clicked Callback for when scan button is clicked
     * @param on_disconnect_clicked Callback for when disconnect button is clicked
     * @param on_device_clicked Callback for when a device in the list is clicked, receives device address
     */
    ScanScreen(lv_obj_t *parent_screen,
               std::function<void()> on_scan_clicked,
               std::function<void()> on_disconnect_clicked,
               std::function<void(const char*)> on_device_clicked);

    /**
     * @brief Destructor
     */
    ~ScanScreen();

    /**
     * @brief Get the container object for this screen
     *
     * @return The LVGL container object
     */
    lv_obj_t *getContainer() const { return _container; }

    /**
     * @brief Get the status label
     *
     * @return The LVGL label object
     */
    lv_obj_t *getStatusLabel() const { return _status_label; }

    /**
     * @brief Get the device list
     *
     * @return The LVGL list object
     */
    lv_obj_t *getDeviceList() const { return _device_list; }

    /**
     * @brief Get the scan button
     *
     * @return The LVGL button object
     */
    lv_obj_t *getScanButton() const { return _scan_btn; }

    /**
     * @brief Get the disconnect button
     *
     * @return The LVGL button object
     */
    lv_obj_t *getDisconnectButton() const { return _disconnect_btn; }

private:
    lv_obj_t *_container;
    lv_obj_t *_status_label;
    lv_obj_t *_device_list;
    lv_obj_t *_scan_btn;
    lv_obj_t *_disconnect_btn;

    std::function<void()> _on_scan_clicked;
    std::function<void()> _on_disconnect_clicked;
    std::function<void(const char*)> _on_device_clicked;
};

} // namespace esp_brookesia::apps::screens
