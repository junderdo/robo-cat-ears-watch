#include "pick_color_screen.hpp"
#include <cmath>

namespace esp_brookesia {
namespace apps {
namespace screens {

PickColorScreen::PickColorScreen(lv_obj_t *parent)
    : _container(nullptr)
    , _panel(nullptr)
    , _arc(nullptr)
    , _confirm_btn(nullptr)
    , _cancel_btn(nullptr)
    , _current_hue(0)
    , _on_color_picked(nullptr)
    , _on_modal_shown(nullptr)
    , _on_modal_hidden(nullptr)
{
    // Create full-screen container (modal overlay)
    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_container, LV_OPA_90, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_container, LV_OBJ_FLAG_HIDDEN); // Initially hidden

    // Create a centered panel for the color picker and buttons
    _panel = lv_obj_create(_container);
    lv_obj_set_size(_panel, lv_pct(90), lv_pct(80));
    lv_obj_center(_panel);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(_panel, 2, 0);
    lv_obj_set_style_border_color(_panel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(_panel, 10, 0);
    lv_obj_clear_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    lv_obj_t *title = lv_label_create(_panel);
    lv_label_set_text(title, "Pick a Color");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Color wheel arc (full circle, hue 0-359)
    _arc = lv_arc_create(_panel);
    lv_obj_set_size(_arc, 200, 200);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(_arc, 0, 360);
    lv_arc_set_range(_arc, 0, 359);
    lv_arc_set_value(_arc, 0);
    lv_obj_set_style_arc_width(_arc, 30, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arc, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_pad_all(_arc, 12, LV_PART_KNOB);
    // Color the indicator and knob with the initial hue
    lv_color_t init_color = lv_color_hsv_to_rgb(0, 100, 100);
    lv_obj_set_style_arc_color(_arc, init_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_arc, init_color, LV_PART_KNOB);

    lv_obj_add_event_cb(_arc, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (!screen) return;
        lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

        lv_indev_t *indev = lv_indev_get_act();
        if (!indev) return;

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        lv_area_t coords;
        lv_obj_get_coords(arc, &coords);
        int32_t cx = (coords.x1 + coords.x2) / 2;
        int32_t cy = (coords.y1 + coords.y2) / 2;

        float angle = atan2f((float)(point.y - cy), (float)(point.x - cx)) * (180.0f / M_PI);
        if (angle < 0.0f) angle += 360.0f;

        uint16_t hue = (uint16_t)angle;
        screen->_current_hue = hue;
        lv_arc_set_value(arc, hue);

        lv_color_t color = lv_color_hsv_to_rgb(hue, 100, 100);
        lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(arc, color, LV_PART_KNOB);
    }, LV_EVENT_PRESSING, this);

    // Create button container at bottom
    lv_obj_t *btn_container = lv_obj_create(_panel);
    lv_obj_set_size(btn_container, lv_pct(100), 60);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Cancel button
    _cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(_cancel_btn, 100, 50);
    lv_obj_set_style_bg_color(_cancel_btn, lv_color_hex(0x606060), 0);
    
    lv_obj_t *cancel_label = lv_label_create(_cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_add_event_cb(_cancel_btn, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (screen) {
            screen->hide();
        }
    }, LV_EVENT_CLICKED, this);

    // Confirm button
    _confirm_btn = lv_btn_create(btn_container);
    lv_obj_set_size(_confirm_btn, 100, 50);
    lv_obj_set_style_bg_color(_confirm_btn, lv_color_hex(0x00AA00), 0);
    
    lv_obj_t *confirm_label = lv_label_create(_confirm_btn);
    lv_label_set_text(confirm_label, "OK");
    lv_obj_center(confirm_label);

    lv_obj_add_event_cb(_confirm_btn, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (screen && screen->_on_color_picked) {
            lv_color_t color = lv_color_hsv_to_rgb(screen->_current_hue, 100, 100);
            lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
            uint32_t rgb = ((uint32_t)c32.red << 16) | ((uint32_t)c32.green << 8) | c32.blue;
            screen->_on_color_picked(rgb);
        }
        if (screen) {
            screen->hide();
        }
    }, LV_EVENT_CLICKED, this);
}

PickColorScreen::~PickColorScreen()
{
    if (_container) {
        lv_obj_del(_container);
        _container = nullptr;
    }
}

void PickColorScreen::show()
{
    if (_container) {
        lv_obj_clear_flag(_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_container);
    }
    
    // Notify that modal is shown
    if (_on_modal_shown) {
        _on_modal_shown();
    }
}

void PickColorScreen::hide()
{
    if (_container) {
        lv_obj_add_flag(_container, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Notify that modal is hidden
    if (_on_modal_hidden) {
        _on_modal_hidden();
    }
}

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
