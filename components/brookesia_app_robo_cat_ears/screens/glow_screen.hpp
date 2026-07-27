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

    /**
     * @brief Load lighting data from the lighting service
     * 
     * Attempts to read lighting configuration from the ABF2 characteristic
     * and updates the UI accordingly (colors, mode, speed)
     */
    void loadLightingData();

    /**
     * @brief Set the current mode
     * 
     * @param mode Mode name string
     */
    void setMode(const std::string &mode);

    /**
     * @brief Set the current speed
     * 
     * @param speed Speed value (1-100)
     */
    void setSpeed(int speed);

    /**
     * @brief Clear all colors from the list
     */
    void clearColors();

    /**
     * @brief Set the overall LED brightness
     *
     * Colors shown in the UI are always the user's chosen values; brightness is
     * applied only to the copies sent over BLE.
     *
     * @param brightness Brightness value (0-100)
     */
    void setBrightness(int brightness);

    /**
     * @brief Get the current brightness
     *
     * @return Brightness value (0-100)
     */
    int getBrightness() const { return _brightness; }

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

    /**
     * @brief Save current lighting data to the device
     * 
     * Collects the current mode, speed, and colors and writes them
     * to the ABF2 characteristic via the lighting service
     */
    void saveLightingDataToDevice();

    /**
     * @brief Update the brightness label to show the current percentage
     */
    void updateBrightnessLabel();

    /**
     * @brief Persist the user's colors and brightness to NVS
     *
     * The BLE protocol carries no brightness field and the colors written to the
     * peripheral are already dimmed, so the peripheral cannot round-trip the
     * user's originals. The watch is the source of truth for both.
     */
    void saveStateToNvs();

    /**
     * @brief Restore colors and brightness from NVS
     *
     * @return true if a stored color list was found
     */
    bool loadStateFromNvs();

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
    int _last_reorder_from_index;  // Track last reorder indices to prevent rapid re-triggering
    int _last_reorder_to_index;
    bool _loading_from_device;  // Flag to prevent saving during initial load
    lv_obj_t *_brightness_slider;
    lv_obj_t *_brightness_label;
    lv_timer_t *_brightness_debounce_timer;
    int _brightness;  // 0-100, applied only to colors sent over BLE
    bool _loaded_from_nvs;  // Colors came from NVS, so don't overwrite them from the device
};

} // namespace esp_brookesia::apps::screens
