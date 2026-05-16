/*
 * Description: Modes screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <functional>
#include "lvgl.h"

namespace esp_brookesia {
namespace apps {
namespace screens {

class ModesScreen {
public:
    ModesScreen(lv_obj_t *parent);
    ~ModesScreen();

    lv_obj_t *getContainer() const { return _container; }
    void show();
    void hide();
    
    // Callback when a mode is selected
    void setOnModeSelected(std::function<void(const char *mode)> callback) {
        _on_mode_selected = callback;
    }
    
    // Callback when OK button is clicked
    void setOnConfirmed(std::function<void()> callback) {
        _on_confirmed = callback;
    }
    
    // Callback when speed changes
    void setOnSpeedChanged(std::function<void(int speed)> callback) {
        _on_speed_changed = callback;
    }

    // Callbacks for modal visibility changes
    void setOnModalShown(std::function<void()> callback) {
        _on_modal_shown = callback;
    }
    
    void setOnModalHidden(std::function<void()> callback) {
        _on_modal_hidden = callback;
    }

    /**
     * @brief Set the mode programmatically
     * 
     * @param mode Mode name (e.g., "Solid", "Breathing", etc.)
     */
    void setMode(const char *mode);

    /**
     * @brief Set the speed programmatically
     * 
     * @param speed Speed value (1-100)
     */
    void setSpeed(int speed);

private:
    lv_obj_t *_container;
    lv_obj_t *_panel;
    lv_obj_t *_modes_list;
    lv_obj_t *_speed_slider;
    lv_obj_t *_cancel_btn;
    lv_obj_t *_ok_btn;
    lv_obj_t *_mode_buttons[5];  // Track mode buttons for state updates
    lv_obj_t *_selected_mode_btn;
    lv_timer_t *_speed_debounce_timer;  // Timer for debouncing speed changes
    
    // Store initial state for cancel functionality
    char _initial_mode[32];
    int _initial_speed;
    
    void updateModeButtonStates(lv_obj_t *selected_btn);
    
    std::function<void(const char *mode)> _on_mode_selected;
    std::function<void()> _on_confirmed;
    std::function<void(int speed)> _on_speed_changed;
    std::function<void()> _on_modal_shown;
    std::function<void()> _on_modal_hidden;
};

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
