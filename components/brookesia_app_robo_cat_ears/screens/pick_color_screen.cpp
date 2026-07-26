#include "pick_color_screen.hpp"
#include "esp_heap_caps.h"
#include <cmath>

namespace esp_brookesia {
namespace apps {
namespace screens {

namespace {

// Wheel geometry, all in canvas-local coordinates. Everything is derived from
// WHEEL_SIZE so the assembly scales from one number.
constexpr int32_t WHEEL_SIZE = 280;
constexpr float CENTER      = WHEEL_SIZE / 2.0f;
constexpr float R_OUT       = WHEEL_SIZE / 2.0f;        // outer edge of the hue ring
constexpr float RING_W      = WHEEL_SIZE * 0.108f;      // hue ring band thickness
constexpr float R_IN        = R_OUT - RING_W;           // inner edge of the hue ring
constexpr float R_KNOB      = (R_OUT + R_IN) / 2.0f;
constexpr float TRI_R       = WHEEL_SIZE * 0.35f;       // circumradius of the S/L triangle

// Equilateral triangle inscribed in TRI_R. It stays put as the hue changes -
// only its colors are recomputed - so the geometry below is fixed.
constexpr float COS30 = 0.8660254f;
constexpr float AX = CENTER,                 AY = CENTER - TRI_R;          // pure hue
constexpr float BX = CENTER + TRI_R * COS30, BY = CENTER + TRI_R * 0.5f;   // black
constexpr float CX = CENTER - TRI_R * COS30, CY = CENTER + TRI_R * 0.5f;   // white

// Vertex-to-opposite-edge distance. For an equilateral triangle a barycentric
// weight is exactly (distance from that edge) / TRI_H, which gives edge
// antialiasing for free.
constexpr float TRI_H = 1.5f * TRI_R;

constexpr float BARY_DENOM = (BY - CY) * (AX - CX) + (CX - BX) * (AY - CY);

// Groove between the triangle and the selected-color swatch behind it, so the
// triangle's edge stays readable when the two colors are close. Drawn in the
// panel's own background color.
constexpr float SWATCH_GAP = 3.0f;
constexpr uint32_t GAP_COLOR = 0x202020;

// Triangle bounding box. The margin covers the groove, which at a 60 degree
// vertex juts out twice its width, plus a pixel for the antialiased edge.
constexpr float TRI_MARGIN = SWATCH_GAP * 2.0f + 3.0f;
constexpr int32_t TRI_X0 = (int32_t)(CX - TRI_MARGIN);
constexpr int32_t TRI_X1 = (int32_t)(BX + TRI_MARGIN + 1.0f);
constexpr int32_t TRI_Y0 = (int32_t)(AY - TRI_MARGIN);
constexpr int32_t TRI_Y1 = (int32_t)(BY + TRI_MARGIN + 1.0f);

// Matched to the ring thickness so the knob never overhangs the canvas edge.
constexpr int32_t KNOB_SIZE = (int32_t)RING_W;

// Biased upward: the empty space above the wheel is larger than the gap to the
// buttons below it, so centering crowds the buttons. Paired with WHEEL_SIZE -
// growing the wheel by N while moving this down by N/2 keeps the top edge clear
// of the title. The swatch shares it so the two stay concentric.
constexpr int32_t WHEEL_Y_OFFSET = -30;

inline uint8_t clamp255(float v)
{
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return (uint8_t)v;
}

// Barycentric weights of (px, py) against the fixed triangle.
inline void barycentric(float px, float py, float &wa, float &wb, float &wc)
{
    wa = ((BY - CY) * (px - CX) + (CX - BX) * (py - CY)) / BARY_DENOM;
    wb = ((CY - AY) * (px - CX) + (AX - CX) * (py - CY)) / BARY_DENOM;
    wc = 1.0f - wa - wb;
}

// Drop negative weights and renormalize, which projects a point outside the
// triangle onto its nearest edge or vertex.
inline void clampBarycentric(float &wa, float &wb, float &wc)
{
    if (wa < 0.0f) wa = 0.0f;
    if (wb < 0.0f) wb = 0.0f;
    if (wc < 0.0f) wc = 0.0f;
    float sum = wa + wb + wc;
    if (sum <= 0.0f) {
        wa = 1.0f;
        wb = wc = 0.0f;
        return;
    }
    wa /= sum;
    wb /= sum;
    wc /= sum;
}

// The triangle blends pure hue (A) against black (B) and white (C), so the
// black vertex contributes nothing and drops out of the sum.
inline uint32_t blendTriangle(lv_color_t hue_rgb, float wa, float wc)
{
    uint8_t r = clamp255(wa * hue_rgb.red   + wc * 255.0f);
    uint8_t g = clamp255(wa * hue_rgb.green + wc * 255.0f);
    uint8_t b = clamp255(wa * hue_rgb.blue  + wc * 255.0f);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Linear blend from `a` to `b`, per channel.
inline uint32_t mixRgb(uint32_t a, uint32_t b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    const float ia = 1.0f - t;
    uint8_t r = clamp255(((a >> 16) & 0xFF) * ia + ((b >> 16) & 0xFF) * t);
    uint8_t g = clamp255(((a >> 8) & 0xFF) * ia + ((b >> 8) & 0xFF) * t);
    uint8_t bl = clamp255((a & 0xFF) * ia + (b & 0xFF) * t);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

// Label color for text sitting on top of `rgb`, by Rec.601 luma. The confirm
// button is filled with the picked color, which ranges from white to black.
inline lv_color_t labelColorFor(uint32_t rgb)
{
    const float luma = 0.299f * ((rgb >> 16) & 0xFF) +
                       0.587f * ((rgb >> 8) & 0xFF) +
                       0.114f * (rgb & 0xFF);
    return lv_color_hex(luma > 150.0f ? 0x000000 : 0xFFFFFF);
}

inline uint16_t angleToHue(float dx, float dy)
{
    float deg = atan2f(dy, dx) * (180.0f / (float)M_PI);
    if (deg < 0.0f) deg += 360.0f;
    return (uint16_t)deg % 360;
}

} // namespace

PickColorScreen::PickColorScreen(lv_obj_t *parent)
    : _container(nullptr)
    , _panel(nullptr)
    , _swatch(nullptr)
    , _canvas(nullptr)
    , _hue_knob(nullptr)
    , _sl_knob(nullptr)
    , _confirm_btn(nullptr)
    , _confirm_label(nullptr)
    , _cancel_btn(nullptr)
    , _canvas_buf(nullptr)
    , _current_hue(0)
    , _sel_x(AX)
    , _sel_y(AY)
    , _current_color(0xFF0000)
    , _drag_mode(DRAG_NONE)
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

    // The hue ring never changes and the triangle only changes with hue, so
    // both live in one canvas painted per-pixel rather than as LVGL widgets.
    // Flash is the scarce resource here (~890KB free in a 4MB app partition),
    // so the wheel is generated into PSRAM at construction instead of baked in.
    _canvas_buf = (uint32_t *)heap_caps_malloc(WHEEL_SIZE * WHEEL_SIZE * sizeof(uint32_t),
                                               MALLOC_CAP_SPIRAM);
    if (_canvas_buf) {
        // Selected-color swatch. It sits behind the canvas and shows through the
        // region the canvas leaves transparent - between the S/L triangle and the
        // inner edge of the hue ring. Created first so it renders underneath, and
        // sized to tuck a pixel under the ring so no seam shows. Recoloring it is
        // a style change, so dragging the triangle repaints nothing.
        _swatch = lv_obj_create(_panel);
        lv_obj_set_size(_swatch, (int32_t)(R_IN * 2.0f) + 2, (int32_t)(R_IN * 2.0f) + 2);
        lv_obj_set_style_radius(_swatch, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(_swatch, 0, 0);
        lv_obj_align(_swatch, LV_ALIGN_CENTER, 0, WHEEL_Y_OFFSET);
        lv_obj_clear_flag(_swatch, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        _canvas = lv_canvas_create(_panel);
        lv_canvas_set_buffer(_canvas, _canvas_buf, WHEEL_SIZE, WHEEL_SIZE, LV_COLOR_FORMAT_ARGB8888);
        lv_obj_align(_canvas, LV_ALIGN_CENTER, 0, WHEEL_Y_OFFSET);
        lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);

        renderRegion(0, 0, WHEEL_SIZE, WHEEL_SIZE);

        // Knob for the hue ring
        _hue_knob = lv_obj_create(_canvas);
        lv_obj_set_size(_hue_knob, KNOB_SIZE, KNOB_SIZE);
        lv_obj_set_style_radius(_hue_knob, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(_hue_knob, 3, 0);
        lv_obj_set_style_border_color(_hue_knob, lv_color_hex(0xFFFFFF), 0);
        lv_obj_clear_flag(_hue_knob, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        // Knob for the saturation/lightness triangle
        _sl_knob = lv_obj_create(_canvas);
        lv_obj_set_size(_sl_knob, KNOB_SIZE, KNOB_SIZE);
        lv_obj_set_style_radius(_sl_knob, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(_sl_knob, 3, 0);
        lv_obj_set_style_border_color(_sl_knob, lv_color_hex(0xFFFFFF), 0);
        lv_obj_clear_flag(_sl_knob, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        lv_obj_add_event_cb(_canvas, [](lv_event_t *e) {
            PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
            if (!screen) return;

            lv_indev_t *indev = lv_indev_get_act();
            if (!indev) return;

            lv_point_t point;
            lv_indev_get_point(indev, &point);

            lv_area_t coords;
            lv_obj_get_coords(screen->_canvas, &coords);

            screen->handlePress(point.x - coords.x1, point.y - coords.y1,
                                lv_event_get_code(e) == LV_EVENT_PRESSED);
        }, LV_EVENT_PRESSED, this);

        lv_obj_add_event_cb(_canvas, [](lv_event_t *e) {
            PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
            if (!screen) return;

            lv_indev_t *indev = lv_indev_get_act();
            if (!indev) return;

            lv_point_t point;
            lv_indev_get_point(indev, &point);

            lv_area_t coords;
            lv_obj_get_coords(screen->_canvas, &coords);

            screen->handlePress(point.x - coords.x1, point.y - coords.y1, false);
        }, LV_EVENT_PRESSING, this);
    }

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

    // Confirm button. Doubles as the swatch - it's filled with the picked color
    // in updateSelection(), so it needs a neutral border to stay visible when
    // that color is close to the panel background.
    _confirm_btn = lv_btn_create(btn_container);
    lv_obj_set_size(_confirm_btn, 100, 50);
    lv_obj_set_style_border_width(_confirm_btn, 2, 0);
    lv_obj_set_style_border_color(_confirm_btn, lv_color_hex(0x808080), 0);

    _confirm_label = lv_label_create(_confirm_btn);
    lv_label_set_text(_confirm_label, "OK");
    lv_obj_center(_confirm_label);

    lv_obj_add_event_cb(_confirm_btn, [](lv_event_t *e) {
        PickColorScreen *screen = (PickColorScreen *)lv_event_get_user_data(e);
        if (screen && screen->_on_color_picked) {
            screen->_on_color_picked(screen->_current_color);
        }
        if (screen) {
            screen->hide();
        }
    }, LV_EVENT_CLICKED, this);

    // Runs last so it can paint the confirm button swatch as well as the knobs.
    updateSelection();
}

PickColorScreen::~PickColorScreen()
{
    if (_container) {
        lv_obj_del(_container);
        _container = nullptr;
        _canvas = nullptr;
    }
    if (_canvas_buf) {
        heap_caps_free(_canvas_buf);
        _canvas_buf = nullptr;
    }
}

void PickColorScreen::renderRegion(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WHEEL_SIZE) x1 = WHEEL_SIZE;
    if (y1 > WHEEL_SIZE) y1 = WHEEL_SIZE;

    // The triangle is a single hue blended toward black and white, so its
    // vertex color is constant across the whole region.
    const lv_color_t hue_rgb = lv_color_hsv_to_rgb(_current_hue, 100, 100);

    for (int32_t y = y0; y < y1; y++) {
        uint32_t *row = _canvas_buf + (size_t)y * WHEEL_SIZE;
        const float py = y + 0.5f;
        const float dy = py - CENTER;

        for (int32_t x = x0; x < x1; x++) {
            const float px = x + 0.5f;
            const float dx = px - CENTER;
            const float dist = sqrtf(dx * dx + dy * dy);

            uint32_t pixel = 0; // transparent

            // Hue ring, antialiased across both edges.
            if (dist <= R_OUT + 1.0f && dist >= R_IN - 1.0f) {
                const float cov = fminf(R_OUT - dist, dist - R_IN) + 0.5f;
                if (cov > 0.0f) {
                    const lv_color_t c = lv_color_hsv_to_rgb(angleToHue(dx, dy), 100, 100);
                    pixel = ((uint32_t)clamp255(cov * 255.0f) << 24) |
                            ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | c.blue;
                }
            } else if (dist < R_IN) {
                // Saturation/lightness triangle. A barycentric weight doubles as
                // the normalized distance from the opposite edge, so the smallest
                // weight is a signed distance from the triangle's boundary:
                // positive inside, negative outside.
                float wa, wb, wc;
                barycentric(px, py, wa, wb, wc);
                const float edge = fminf(wa, fminf(wb, wc)) * TRI_H;

                if (edge > -SWATCH_GAP) {
                    // Triangle, fading into the opaque groove at its edge.
                    clampBarycentric(wa, wb, wc);
                    pixel = 0xFF000000u | mixRgb(GAP_COLOR, blendTriangle(hue_rgb, wa, wc), edge + 0.5f);
                } else {
                    // Beyond the groove the canvas is left transparent so the
                    // selected-color swatch behind it shows through.
                    const float cov = 1.0f - (-edge - SWATCH_GAP);
                    if (cov > 0.0f) {
                        pixel = ((uint32_t)clamp255(cov * 255.0f) << 24) | GAP_COLOR;
                    }
                }
            }

            row[x] = pixel;
        }
    }
}

void PickColorScreen::renderTriangle()
{
    renderRegion(TRI_X0, TRI_Y0, TRI_X1, TRI_Y1);
    lv_obj_invalidate(_canvas);
}

void PickColorScreen::updateSelection()
{
    float wa, wb, wc;
    barycentric(_sel_x, _sel_y, wa, wb, wc);
    clampBarycentric(wa, wb, wc);

    const lv_color_t hue_rgb = lv_color_hsv_to_rgb(_current_hue, 100, 100);
    _current_color = blendTriangle(hue_rgb, wa, wc);

    if (_swatch) {
        lv_obj_set_style_bg_color(_swatch, lv_color_hex(_current_color), 0);
    }

    if (_hue_knob) {
        const float rad = _current_hue * ((float)M_PI / 180.0f);
        lv_obj_align_to(_hue_knob, _canvas, LV_ALIGN_CENTER,
                        (int32_t)(R_KNOB * cosf(rad)), (int32_t)(R_KNOB * sinf(rad)));
        lv_obj_set_style_bg_color(_hue_knob, hue_rgb, 0);

        lv_obj_align_to(_sl_knob, _canvas, LV_ALIGN_CENTER,
                        (int32_t)(_sel_x - CENTER), (int32_t)(_sel_y - CENTER));
        lv_obj_set_style_bg_color(_sl_knob, lv_color_hex(_current_color), 0);
    }

    if (_confirm_label) {
        const lv_color_t swatch = lv_color_hex(_current_color);
        lv_obj_set_style_bg_color(_confirm_btn, swatch, 0);
        // Same fill while pressed, otherwise the theme's pressed color flashes
        // over the swatch.
        lv_obj_set_style_bg_color(_confirm_btn, swatch, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(_confirm_label, labelColorFor(_current_color), 0);
    }
}

void PickColorScreen::handlePress(int32_t canvas_x, int32_t canvas_y, bool is_new_press)
{
    const float dx = canvas_x - CENTER;
    const float dy = canvas_y - CENTER;
    const float dist = sqrtf(dx * dx + dy * dy);

    // Latch the region on press so a drag that strays across the boundary keeps
    // controlling whichever control it started on.
    if (is_new_press) {
        if (dist >= R_IN - 4.0f && dist <= R_OUT + 8.0f) {
            _drag_mode = DRAG_RING;
        } else if (dist < R_IN - 4.0f) {
            _drag_mode = DRAG_TRIANGLE;
        } else {
            _drag_mode = DRAG_NONE;
        }
    }

    if (_drag_mode == DRAG_RING) {
        const uint16_t hue = angleToHue(dx, dy);
        if (hue == _current_hue) return;
        _current_hue = hue;
        renderTriangle();
        updateSelection();
    } else if (_drag_mode == DRAG_TRIANGLE) {
        float wa, wb, wc;
        barycentric(canvas_x + 0.5f, canvas_y + 0.5f, wa, wb, wc);
        clampBarycentric(wa, wb, wc);
        _sel_x = wa * AX + wb * BX + wc * CX;
        _sel_y = wa * AY + wb * BY + wc * CY;
        updateSelection();
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
    _drag_mode = DRAG_NONE;

    // Notify that modal is hidden
    if (_on_modal_hidden) {
        _on_modal_hidden();
    }
}

} // namespace screens
} // namespace apps
} // namespace esp_brookesia
