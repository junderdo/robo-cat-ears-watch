#pragma once

#include <functional>
#include "lvgl.h"

namespace esp_brookesia {
namespace apps {
namespace screens {

class PickColorScreen {
public:
    PickColorScreen(lv_obj_t *parent);
    ~PickColorScreen();

    lv_obj_t *getContainer() const { return _container; }
    void show();
    void hide();
    
    // Callback when a color is picked (confirmed)
    void setOnColorPicked(std::function<void(uint32_t color)> callback) {
        _on_color_picked = callback;
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
    lv_obj_t *_red_slider;
    lv_obj_t *_green_slider;
    lv_obj_t *_blue_slider;
    lv_obj_t *_color_preview;
    lv_obj_t *_confirm_btn;
    lv_obj_t *_cancel_btn;
    
    void updateColorPreview();
    
    std::function<void(uint32_t color)> _on_color_picked;
    std::function<void()> _on_modal_shown;
    std::function<void()> _on_modal_hidden;
};

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
