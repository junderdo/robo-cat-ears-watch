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

private:
    lv_obj_t *_container;
    lv_obj_t *_panel;
    lv_obj_t *_modes_list;
    lv_obj_t *_speed_slider;
    lv_obj_t *_cancel_btn;
    lv_obj_t *_ok_btn;
    lv_obj_t *_mode_buttons[5];  // Track mode buttons for state updates
    lv_obj_t *_selected_mode_btn;
    
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
