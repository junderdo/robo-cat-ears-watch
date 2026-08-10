/*
 * Description: Animation control screen for Robo cat ears controller app
 * Author: Jeff Underdown (junderdo)
 * Copyright (C) 2026 Milk Lab Creations
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "animate_screen.hpp"
#include "animation_mode_service.hpp"
#include "animation_store_service.hpp"
#include "bluetooth_service.hpp"

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:AnimateScreen"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps::screens {

namespace {

const char *BUILT_IN_LABELS[] = {"Right", "Left", "Happy :)", "Sad :(", "Wiggle", "Radar", "Curious", "Alert"};
constexpr int BUILT_IN_COUNT = 8;

constexpr lv_opa_t DIMMED_OPA = LV_OPA_40;

bool wasSwipe()
{
    return lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_NONE;
}

const char *storeMessage()
{
    auto *store = robo_cat_ears::AnimationStoreService::getInstance();
    auto *bt = robo_cat_ears::BluetoothService::getInstance();

    switch (store->getState()) {
    case robo_cat_ears::AnimationStoreState::NO_CONNECTION:
        return (bt && bt->isConnecting()) ? "Connecting to the ears..." : "Not connected to the ears";
    case robo_cat_ears::AnimationStoreState::FETCHING:
        return "Loading the ears' animations...";
    case robo_cat_ears::AnimationStoreState::READY:
        if (store->getEntries().empty()) {
            return "No animations stored on the ears";
        }
        return store->wasLastPlayStale() ? "That animation is no longer on the ears" : nullptr;
    case robo_cat_ears::AnimationStoreState::FETCH_FAILED:
        return "Couldn't read the ears' animations";
    case robo_cat_ears::AnimationStoreState::VERSION_MISMATCH:
        return store->isWatchStale() ? "Watch firmware is out of date" : "Ears firmware is out of date";
    case robo_cat_ears::AnimationStoreState::LINK_LOST:
        return "Disconnected - reconnecting...";
    }
    return nullptr;
}

} // namespace

void AnimateScreen::onBuiltInClicked(lv_event_t *e)
{
    if (wasSwipe()) {
        return;
    }

    lv_obj_t *btn = (lv_obj_t *)lv_event_get_current_target(e);
    AnimateScreen *screen = (AnimateScreen *)lv_obj_get_user_data(btn);
    if (!screen || !screen->_on_command_clicked) {
        return;
    }

    int index = (int)(intptr_t)lv_event_get_user_data(e);
    screen->_on_command_clicked(std::to_string(index + 1));
}

void AnimateScreen::onStoredSlotClicked(lv_event_t *e)
{
    if (wasSwipe()) {
        return;
    }

    uint8_t slot = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    robo_cat_ears::AnimationStoreService::getInstance()->play(slot);
}

AnimateScreen::AnimateScreen(lv_obj_t *parent_screen,
                             std::function<void(const std::string&)> on_command_clicked)
    : _container(nullptr),
      _status_label(nullptr),
      _auto_animate_switch(nullptr),
      _scroll_container(nullptr),
      _btn_width(0),
      _btn_height(0),
      _loading_from_device(false),
      _on_command_clicked(on_command_clicked)
{
    ESP_UTILS_LOGD("Creating animate screen");

    // Create a container for the animate screen
    _container = lv_obj_create(parent_screen);
    lv_obj_set_size(_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);

    // Create a connection status label at the top (same as scan screen)
    _status_label = lv_label_create(_container);
    lv_label_set_text(_status_label, "Not connected");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_status_label, LV_ALIGN_TOP_MID, 0, 36);

    // Calculate screen dimensions
    int screen_width = lv_obj_get_width(parent_screen);
    int screen_height = lv_obj_get_height(parent_screen);

    // Create Auto-animate toggle row (below status label)
    lv_obj_t *auto_row = lv_obj_create(_container);
    lv_obj_set_size(auto_row, screen_width - 20, 50);
    lv_obj_align(auto_row, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_opa(auto_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(auto_row, 0, 0);
    lv_obj_set_style_pad_hor(auto_row, 10, 0);
    lv_obj_set_style_pad_ver(auto_row, 5, 0);
    lv_obj_set_flex_flow(auto_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(auto_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(auto_row, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *auto_label = lv_label_create(auto_row);
    lv_label_set_text(auto_label, "Auto-animate");
    lv_obj_set_style_text_font(auto_label, &lv_font_montserrat_24, 0);
    lv_obj_add_flag(auto_label, LV_OBJ_FLAG_GESTURE_BUBBLE);

    _auto_animate_switch = lv_switch_create(auto_row);
    lv_obj_set_size(_auto_animate_switch, 60, 30);

    // Toggle event: send animation mode data over BLE
    lv_obj_add_event_cb(_auto_animate_switch, [](lv_event_t *e) {
        AnimateScreen *screen = (AnimateScreen *)lv_event_get_user_data(e);
        if (!screen || screen->_loading_from_device) {
            return;
        }

        robo_cat_ears::AnimationModeService *anim_service = robo_cat_ears::AnimationModeService::getInstance();
        if (!anim_service || !anim_service->init()) {
            return;
        }

        bool checked = lv_obj_has_state(screen->_auto_animate_switch, LV_STATE_CHECKED);
        robo_cat_ears::AnimationModeData data;
        if (checked) {
            data.mode_id = 1;
            data.frequency = 100;
        } else {
            data.mode_id = 0;
            data.frequency = 0;
        }

        ESP_UTILS_LOGI("Auto-animate toggled: %s (mode_id=%d, frequency=%d)",
                       checked ? "ON" : "OFF", data.mode_id, data.frequency);
        anim_service->writeAnimationModeData(&data);
    }, LV_EVENT_VALUE_CHANGED, this);

    // Button grid layout: account for toggle row height
    _btn_width = (screen_width - 30) / 2;  // 30 = padding + gap
    _btn_height = (screen_height - 160) / 2;  // 160 = top padding for status + toggle + gaps

    // Create a scrollable container for the buttons (below toggle row)
    _scroll_container = lv_obj_create(_container);
    lv_obj_set_size(_scroll_container, screen_width, screen_height - 136);
    lv_obj_set_pos(_scroll_container, 0, 136);
    lv_obj_set_style_bg_opa(_scroll_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_scroll_container, 0, 0);
    lv_obj_set_style_pad_all(_scroll_container, 0, 0);
    lv_obj_set_scroll_dir(_scroll_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_scroll_container, LV_SCROLLBAR_MODE_OFF);

    robo_cat_ears::AnimationStoreService::getInstance()->setChangedCallback([this]() {
        refreshAnimations();
    });

    refreshAnimations();

    ESP_UTILS_LOGD("Animate screen created successfully");
}

lv_obj_t *AnimateScreen::createAnimationButton(int position, const char *name, const lv_font_t *font,
                                               lv_event_cb_t on_click, void *action)
{
    int row = position / 2;
    int col = position % 2;

    lv_obj_t *btn = lv_btn_create(_scroll_container);
    lv_obj_set_size(btn, _btn_width, _btn_height);
    lv_obj_set_pos(btn, col * (_btn_width + 10) + 10, row * (_btn_height + 10) + 10);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_font(label, font, 0);
    // Names are arbitrary UTF-8 up to 32 bytes. LVGL's own long mode truncates
    // on a character boundary; glyphs Montserrat lacks simply do not draw.
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, _btn_width - 16);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, action);
    lv_obj_set_user_data(btn, this);

    return btn;
}

void AnimateScreen::refreshAnimations()
{
    if (!_scroll_container) {
        return;
    }

    lv_obj_clean(_scroll_container);

    auto *store = robo_cat_ears::AnimationStoreService::getInstance();
    auto *bt = robo_cat_ears::BluetoothService::getInstance();

    // Built-ins need only a link. A stored slot needs a list we know is current
    // for the device we are talking to, which is exactly the READY state.
    bool link_up = bt && bt->isConnected();
    bool slots_current = store->getState() == robo_cat_ears::AnimationStoreState::READY;

    int position = 0;
    for (int i = 0; i < BUILT_IN_COUNT; i++) {
        lv_obj_t *btn = createAnimationButton(position++, BUILT_IN_LABELS[i], &lv_font_montserrat_28,
                                              onBuiltInClicked, (void *)(intptr_t)i);
        if (!link_up) {
            lv_obj_set_style_opa(btn, DIMMED_OPA, 0);
        }
    }

    const auto &entries = store->getEntries();
    for (const auto &entry : entries) {
        lv_obj_t *btn = createAnimationButton(position++, entry.name.c_str(), &lv_font_montserrat_18,
                                              onStoredSlotClicked, (void *)(intptr_t)entry.slot);
        if (!slots_current) {
            lv_obj_set_style_opa(btn, DIMMED_OPA, 0);
        }
    }

    const char *message = storeMessage();
    if (message) {
        int rows = (position + 1) / 2;
        lv_obj_t *label = lv_label_create(_scroll_container);
        lv_label_set_text(label, message);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, _btn_width * 2);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 10, rows * (_btn_height + 10) + 20);
    }

    ESP_UTILS_LOGD("Animate grid rebuilt: %d built-ins + %d stored", BUILT_IN_COUNT, (int)entries.size());
}

AnimateScreen::~AnimateScreen()
{
    // The store service outlives this screen, so drop the callback capturing it
    robo_cat_ears::AnimationStoreService::getInstance()->setChangedCallback(nullptr);

    // LVGL objects are automatically cleaned up when parent is deleted
}

void AnimateScreen::loadAnimationModeData()
{
    robo_cat_ears::BluetoothService *bt_service = robo_cat_ears::BluetoothService::getInstance();
    if (!bt_service || !bt_service->isConnected() || bt_service->getCharHandleABF2() == 0) {
        ESP_UTILS_LOGD("Not ready to load animation mode data (not connected or ABF2 not discovered)");
        return;
    }

    // The store's connect sequence owns the link while it runs; this read waits
    // for the session-complete callback rather than racing it
    if (robo_cat_ears::AnimationStoreService::getInstance()->getState() ==
        robo_cat_ears::AnimationStoreState::FETCHING) {
        ESP_UTILS_LOGD("Store connect sequence in flight, deferring animation mode read");
        return;
    }

    robo_cat_ears::AnimationModeService *anim_service = robo_cat_ears::AnimationModeService::getInstance();
    if (!anim_service || !anim_service->init()) {
        ESP_UTILS_LOGE("Failed to get/init animation mode service");
        return;
    }

    ESP_UTILS_LOGI("Loading animation mode data from device");

    robo_cat_ears::AnimationModeData anim_data;
    anim_service->readAnimationModeData(&anim_data, [this](const robo_cat_ears::AnimationModeData &data) {
        ESP_UTILS_LOGI("Animation mode data loaded: mode_id=%d, frequency=%d", data.mode_id, data.frequency);

        _loading_from_device = true;
        if (data.mode_id == 0) {
            lv_obj_clear_state(_auto_animate_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(_auto_animate_switch, LV_STATE_CHECKED);
        }
        _loading_from_device = false;
    });
}

} // namespace esp_brookesia::apps::screens
