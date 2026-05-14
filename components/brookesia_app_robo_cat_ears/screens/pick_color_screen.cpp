#include "pick_color_screen.hpp"

namespace esp_brookesia {
namespace apps {
namespace screens {

PickColorScreen::PickColorScreen(lv_obj_t *parent)
    : _container(nullptr)
    , _panel(nullptr)
    , _red_slider(nullptr)
    , _green_slider(nullptr)
    , _blue_slider(nullptr)
    , _color_preview(nullptr)
    , _confirm_btn(nullptr)
    , _cancel_btn(nullptr)
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

    // Color preview box
    _color_preview = lv_obj_create(_panel);
    lv_obj_set_size(_color_preview, 80, 80);
    lv_obj_align(_color_preview, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_bg_color(_color_preview, lv_color_hex(0x3380B3), 0);
    lv_obj_set_style_border_width(_color_preview, 2, 0);
    lv_obj_set_style_border_color(_color_preview, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(_color_preview, 10, 0);

    // Red slider
    lv_obj_t *red_label = lv_label_create(_panel);
    lv_label_set_text(red_label, "R");
    lv_obj_align(red_label, LV_ALIGN_TOP_LEFT, 20, 110);
    lv_obj_set_style_text_color(red_label, lv_color_hex(0xFF0000), 0);
    
    _red_slider = lv_slider_create(_panel);
    lv_obj_set_size(_red_slider, lv_pct(70), 32);
    lv_obj_align(_red_slider, LV_ALIGN_TOP_LEFT, 50, 110);
    lv_slider_set_range(_red_slider, 0, 255);
    lv_slider_set_value(_red_slider, 51, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_red_slider, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    lv_obj_add_event_cb(_red_slider, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (screen) {
            screen->updateColorPreview();
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // Green slider
    lv_obj_t *green_label = lv_label_create(_panel);
    lv_label_set_text(green_label, "G");
    lv_obj_align(green_label, LV_ALIGN_TOP_LEFT, 20, 170);
    lv_obj_set_style_text_color(green_label, lv_color_hex(0x00FF00), 0);
    
    _green_slider = lv_slider_create(_panel);
    lv_obj_set_size(_green_slider, lv_pct(70), 32);
    lv_obj_align(_green_slider, LV_ALIGN_TOP_LEFT, 50, 170);
    lv_slider_set_range(_green_slider, 0, 255);
    lv_slider_set_value(_green_slider, 128, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_green_slider, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_add_event_cb(_green_slider, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (screen) {
            screen->updateColorPreview();
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // Blue slider
    lv_obj_t *blue_label = lv_label_create(_panel);
    lv_label_set_text(blue_label, "B");
    lv_obj_align(blue_label, LV_ALIGN_TOP_LEFT, 20, 230);
    lv_obj_set_style_text_color(blue_label, lv_color_hex(0x0000FF), 0);
    
    _blue_slider = lv_slider_create(_panel);
    lv_obj_set_size(_blue_slider, lv_pct(70), 32);
    lv_obj_align(_blue_slider, LV_ALIGN_TOP_LEFT, 50, 230);
    lv_slider_set_range(_blue_slider, 0, 255);
    lv_slider_set_value(_blue_slider, 179, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_blue_slider, lv_color_hex(0x0000FF), LV_PART_INDICATOR);
    lv_obj_add_event_cb(_blue_slider, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (screen) {
            screen->updateColorPreview();
        }
    }, LV_EVENT_VALUE_CHANGED, this);

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
            uint8_t r = lv_slider_get_value(screen->_red_slider);
            uint8_t g = lv_slider_get_value(screen->_green_slider);
            uint8_t b = lv_slider_get_value(screen->_blue_slider);
            uint32_t rgb = (r << 16) | (g << 8) | b;
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

void PickColorScreen::updateColorPreview()
{
    if (!_color_preview || !_red_slider || !_green_slider || !_blue_slider) {
        return;
    }
    
    uint8_t r = lv_slider_get_value(_red_slider);
    uint8_t g = lv_slider_get_value(_green_slider);
    uint8_t b = lv_slider_get_value(_blue_slider);
    uint32_t rgb = (r << 16) | (g << 8) | b;
    
    lv_obj_set_style_bg_color(_color_preview, lv_color_hex(rgb), 0);
}

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
