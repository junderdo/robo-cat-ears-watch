/*
 * Description: Control screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "lvgl.h"
#include <functional>
#include <string>

namespace esp_brookesia::apps::screens {

/**
 * @brief Control screen containing command buttons for the robo cat ears
 */
class ControlScreen {
public:
    /**
     * @brief Constructor - creates the control screen UI
     *
     * @param parent_screen The parent LVGL screen object
     * @param on_command_clicked Callback for when a command button is clicked, receives command string
     */
    ControlScreen(lv_obj_t *parent_screen,
                  std::function<void(const std::string&)> on_command_clicked);

    /**
     * @brief Destructor
     */
    ~ControlScreen();

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

    std::function<void(const std::string&)> _on_command_clicked;
};

} // namespace esp_brookesia::apps::screens
