/*
 * Description: Glow screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "lvgl.h"
#include <functional>
#include <vector>
#include <memory>
#include <string>
#include "modes_screen.hpp"

namespace esp_brookesia::apps::screens {

/**
 * @brief Glow screen containing color management for the robo cat ears
 */
class GlowScreen {
public:
    /**
     * @brief Constructor - creates the glow screen UI
     *
     * @param parent_screen The parent LVGL screen object
     */
    GlowScreen(lv_obj_t *parent_screen);

    /**
     * @brief Destructor
     */
    ~GlowScreen();

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
     * @brief Add a new color to the list
     *
     * @param color RGB color value to add
     */
    void addColor(uint32_t color);

    /**
     * @brief Set the callback for when add color button is clicked
     *
     * @param callback Function to call when add color button is clicked
     */
    void setOnAddColorClicked(std::function<void()> callback) {
        _on_add_color_clicked = callback;
    }

private:

    /**
     * @brief Remove a color from the list
     *
     * @param index Index of the color to remove
     */
    void removeColor(int index);

    /**
     * @brief Update the color list UI
     */
    void updateColorList();
    
    /**
     * @brief Update the modes button label with current mode and speed
     */
    void updateModesButtonLabel();
    
    /**
     * @brief Reorder a color to a new position in the list
     * 
     * @param from_index Current index of the color
     * @param to_index Target index for the color
     */
    void reorderColor(int from_index, int to_index);

    lv_obj_t *_container;
    lv_obj_t *_status_label;
    lv_obj_t *_add_color_btn;
    lv_obj_t *_color_list_container;
    lv_obj_t *_trash_icon;
    lv_obj_t *_modes_btn_label;
    std::vector<uint32_t> _colors;
    std::function<void()> _on_add_color_clicked;
    std::unique_ptr<ModesScreen> _modes_screen;
    std::string _current_mode;
    int _current_speed;
};

} // namespace esp_brookesia::apps::screens
