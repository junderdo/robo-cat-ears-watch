/*
 * Description: Settings screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "lvgl.h"

namespace esp_brookesia::apps::screens {

/**
 * @brief Settings screen containing configuration options for the robo cat ears
 */
class SettingsScreen {
public:
    /**
     * @brief Constructor - creates the settings screen UI
     *
     * @param parent_screen The parent LVGL screen object
     */
    SettingsScreen(lv_obj_t *parent_screen);

    /**
     * @brief Destructor
     */
    ~SettingsScreen();

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

private:
    lv_obj_t *_container;
    lv_obj_t *_status_label;
};

} // namespace esp_brookesia::apps::screens
