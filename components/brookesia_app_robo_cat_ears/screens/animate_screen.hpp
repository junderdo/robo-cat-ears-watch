/*
 * Description: Animate screen for Robo cat ears controller app
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
 * @brief Animate screen containing command buttons for the robo cat ears
 */
class AnimateScreen {
public:
    /**
     * @brief Constructor - creates the animate screen UI
     *
     * @param parent_screen The parent LVGL screen object
     * @param on_command_clicked Callback for when a command button is clicked, receives command string
     */
    AnimateScreen(lv_obj_t *parent_screen,
                  std::function<void(const std::string&)> on_command_clicked);

    /**
     * @brief Destructor
     */
    ~AnimateScreen();

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
     * @brief Load animation mode data from the device and update the toggle state
     */
    void loadAnimationModeData();

    /**
     * @brief Rebuild the grid from the ears' reported store
     *
     * The grid is the eight built-ins followed by whatever slots the ears say
     * they hold, in slot order. Empty slots are not drawn at all.
     */
    void refreshAnimations();

private:
    void createAnimationButton(int position, const char *name, const lv_font_t *font, int encoded_action);

    /**
     * @brief The sentence describing the store's state, or nullptr when the grid speaks for itself
     */
    const char *storeMessage() const;

    lv_obj_t *_container;
    lv_obj_t *_status_label;
    lv_obj_t *_auto_animate_switch;
    lv_obj_t *_scroll_container;

    int _btn_width;
    int _btn_height;

    bool _loading_from_device;

    std::function<void(const std::string&)> _on_command_clicked;
};

} // namespace esp_brookesia::apps::screens
