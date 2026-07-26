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
    // Which region a drag was started in. Latched on press so a drag that
    // wanders across the ring/triangle boundary keeps controlling one thing.
    enum DragMode { DRAG_NONE, DRAG_RING, DRAG_TRIANGLE };

    // Paints [x0,x1) x [y0,y1) of the wheel canvas: hue ring, saturation/
    // lightness triangle, or transparent, depending on where each pixel falls.
    void renderRegion(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    // Repaints just the triangle's bounding box - the only part that depends on hue.
    void renderTriangle();
    // Recomputes _current_color from the hue and the selected triangle point,
    // then repositions and recolors both knobs and the confirm button swatch.
    void updateSelection();
    void handlePress(int32_t canvas_x, int32_t canvas_y, bool is_new_press);

    lv_obj_t *_container;
    lv_obj_t *_panel;
    lv_obj_t *_swatch;
    lv_obj_t *_canvas;
    lv_obj_t *_hue_knob;
    lv_obj_t *_sl_knob;
    lv_obj_t *_confirm_btn;
    lv_obj_t *_confirm_label;
    lv_obj_t *_cancel_btn;
    uint32_t *_canvas_buf;

    uint16_t _current_hue;
    // Selected point inside the saturation/lightness triangle, in canvas coords.
    float _sel_x;
    float _sel_y;
    uint32_t _current_color;
    DragMode _drag_mode;

    std::function<void(uint32_t color)> _on_color_picked;
    std::function<void()> _on_modal_shown;
    std::function<void()> _on_modal_hidden;
};

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
