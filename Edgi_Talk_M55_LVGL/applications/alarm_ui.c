#include "alarm_ui.h"
#include "alarm_clock.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static lv_obj_t *g_settings_mbox = NULL;
static lv_obj_t *g_time_labels[ALARM_MAX_COUNT];
static lv_obj_t *g_repeat_labels[ALARM_MAX_COUNT];
static lv_obj_t *g_time_picker_mbox = NULL;
static lv_obj_t *g_picker_hour_roller = NULL;
static lv_obj_t *g_picker_min_roller = NULL;
static int g_picker_idx = -1;

static alarm_store_t g_edit_store;

static const char *repeat_to_str(rt_uint8_t repeat)
{
    if (repeat == ALARM_REPEAT_EVERYDAY)  return "Everyday";
    if (repeat == ALARM_REPEAT_WEEKDAY)   return "Weekdays";
    if (repeat == ALARM_REPEAT_WEEKEND)   return "Weekend";
    if (repeat == 0)                       return "Once";
    return "Custom";
}

static void time_label_update(int idx)
{
    char buf[16];
    rt_snprintf(buf, sizeof(buf), "%02d:%02d",
                g_edit_store.alarms[idx].hour,
                g_edit_store.alarms[idx].minute);
    lv_label_set_text(g_time_labels[idx], buf);
}

static void repeat_label_update(int idx)
{
    lv_label_set_text(g_repeat_labels[idx],
                      repeat_to_str(g_edit_store.alarms[idx].repeat));
}

static void cycle_repeat(int idx)
{
    rt_uint8_t r = g_edit_store.alarms[idx].repeat;
    if (r == 0)                          r = ALARM_REPEAT_EVERYDAY;
    else if (r == ALARM_REPEAT_EVERYDAY) r = ALARM_REPEAT_WEEKDAY;
    else if (r == ALARM_REPEAT_WEEKDAY)  r = ALARM_REPEAT_WEEKEND;
    else                                 r = 0;
    g_edit_store.alarms[idx].repeat = r;
    repeat_label_update(idx);
}

static void time_label_update(int idx);

static void picker_ok_cb(lv_event_t *e)
{
    (void)e;
    if (g_picker_idx < 0 || !g_time_picker_mbox) return;

    g_edit_store.alarms[g_picker_idx].hour   = lv_roller_get_selected(g_picker_hour_roller);
    g_edit_store.alarms[g_picker_idx].minute = lv_roller_get_selected(g_picker_min_roller);
    time_label_update(g_picker_idx);

    lv_msgbox_close(g_time_picker_mbox);
    g_time_picker_mbox = NULL;
    g_picker_hour_roller = NULL;
    g_picker_min_roller = NULL;
    g_picker_idx = -1;
}

static void picker_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_time_picker_mbox) {
        lv_msgbox_close(g_time_picker_mbox);
        g_time_picker_mbox = NULL;
    }
    g_picker_idx = -1;
}

static void setting_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (g_time_picker_mbox) return;

    g_picker_idx = idx;
    g_time_picker_mbox = lv_msgbox_create(lv_scr_act());
    lv_obj_set_size(g_time_picker_mbox, 380, 260);
    lv_obj_align(g_time_picker_mbox, LV_ALIGN_CENTER, 0, 0);

    char title[32];
    rt_snprintf(title, sizeof(title), "Set Time - Alarm %d", idx + 1);
    lv_msgbox_add_title(g_time_picker_mbox, title);

    lv_obj_t *content = lv_msgbox_get_content(g_time_picker_mbox);
    lv_obj_set_layout(content, LV_LAYOUT_NONE);

    lv_obj_t *roller_row = lv_obj_create(content);
    lv_obj_set_size(roller_row, 350, 130);
    lv_obj_align(roller_row, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_layout(roller_row, LV_LAYOUT_NONE);
    lv_obj_set_style_border_width(roller_row, 0, 0);
    lv_obj_set_style_pad_all(roller_row, 0, 0);
    lv_obj_set_style_bg_opa(roller_row, LV_OPA_TRANSP, 0);

    g_picker_hour_roller = lv_roller_create(roller_row);
    lv_roller_set_options(g_picker_hour_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_picker_hour_roller, 3);
    lv_roller_set_selected(g_picker_hour_roller, g_edit_store.alarms[idx].hour, LV_ANIM_OFF);
    lv_obj_align(g_picker_hour_roller, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t *colon = lv_label_create(roller_row);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_24, 0);
    lv_obj_align(colon, LV_ALIGN_CENTER, 0, 0);

    g_picker_min_roller = lv_roller_create(roller_row);
    lv_roller_set_options(g_picker_min_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(g_picker_min_roller, 3);
    lv_roller_set_selected(g_picker_min_roller, g_edit_store.alarms[idx].minute, LV_ANIM_OFF);
    lv_obj_align(g_picker_min_roller, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_t *btn_row = lv_obj_create(content);
    lv_obj_set_size(btn_row, 350, 50);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_layout(btn_row, LV_LAYOUT_NONE);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);

    lv_obj_t *ok_btn = lv_button_create(btn_row);
    lv_obj_align(ok_btn, LV_ALIGN_LEFT_MID, 30, 0);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_add_event_cb(ok_btn, picker_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_btn = lv_button_create(btn_row);
    lv_obj_align(cancel_btn, LV_ALIGN_RIGHT_MID, -30, 0);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_add_event_cb(cancel_btn, picker_cancel_cb, LV_EVENT_CLICKED, NULL);
}

static void switch_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    g_edit_store.alarms[idx].enabled = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
}

static void repeat_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    cycle_repeat(idx);
}

static void save_cb(lv_event_t *e)
{
    (void)e;
    alarm_clock_apply(&g_edit_store);
    alarm_clock_save(&g_edit_store);
    if (g_settings_mbox) {
        lv_msgbox_close(g_settings_mbox);
        g_settings_mbox = NULL;
    }
}

static void cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_settings_mbox) {
        lv_msgbox_close(g_settings_mbox);
        g_settings_mbox = NULL;
    }
}

void alarm_ui_show_settings(void)
{
    if (g_settings_mbox) return;

    alarm_clock_load(&g_edit_store);

    g_settings_mbox = lv_msgbox_create(lv_scr_act());
    lv_obj_set_size(g_settings_mbox, 440, 380);
    lv_obj_align(g_settings_mbox, LV_ALIGN_CENTER, 0, 0);
    lv_msgbox_add_title(g_settings_mbox, "Alarm Settings");
    lv_obj_t *content = lv_msgbox_get_content(g_settings_mbox);

    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(content);
        lv_obj_set_size(row, 410, 60);
        lv_obj_set_style_pad_all(row, 5, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 5, 0);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_align(sw, LV_ALIGN_LEFT_MID, 5, 0);
        if (g_edit_store.alarms[i].enabled) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(sw, switch_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);

        lv_obj_t *time_lbl = lv_label_create(row);
        lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_20, 0);
        lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 60, 0);
        g_time_labels[i] = time_lbl;
        time_label_update(i);

        lv_obj_t *rep_lbl = lv_label_create(row);
        lv_obj_align(rep_lbl, LV_ALIGN_LEFT_MID, 180, 0);
        lv_obj_add_flag(rep_lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(rep_lbl, repeat_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        g_repeat_labels[i] = rep_lbl;
        repeat_label_update(i);

        lv_obj_t *setting_btn = lv_button_create(row);
        lv_obj_align(setting_btn, LV_ALIGN_RIGHT_MID, -5, 0);
        lv_obj_t *setting_lbl = lv_label_create(setting_btn);
        lv_label_set_text(setting_lbl, "Setting");
        lv_obj_add_event_cb(setting_btn, setting_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    /* 底部按钮容器: Save 左, Cancel 右, 同一行 */
    lv_obj_t *footer = lv_obj_create(content);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, 410, 44);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *save_btn = lv_button_create(footer);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_bg_opa(save_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(save_btn, 10, 0);
    lv_obj_set_size(save_btn, 140, 36);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "Save");
    lv_obj_set_style_text_color(save_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_btn = lv_button_create(footer);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cancel_btn, 10, 0);
    lv_obj_set_size(cancel_btn, 140, 36);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, cancel_cb, LV_EVENT_CLICKED, NULL);
}

static lv_obj_t *g_ring_mbox = NULL;

static void ring_dismiss_cb(lv_event_t *e)
{
    (void)e;
    alarm_clock_stop_ringing();
    if (g_ring_mbox) {
        lv_msgbox_close(g_ring_mbox);
        g_ring_mbox = NULL;
    }
}

static void ring_snooze_cb(lv_event_t *e)
{
    (void)e;
    alarm_clock_snooze();
    if (g_ring_mbox) {
        lv_msgbox_close(g_ring_mbox);
        g_ring_mbox = NULL;
    }
}

static void ring_ui_create(int alarm_index)
{
    if (g_ring_mbox) return;

    g_ring_mbox = lv_msgbox_create(lv_scr_act());
    lv_obj_set_size(g_ring_mbox, 400, 250);
    lv_obj_align(g_ring_mbox, LV_ALIGN_CENTER, 0, 0);

    char title[32];
    if (alarm_index >= 0) {
        const alarm_entry_t *e = alarm_clock_get_entry(alarm_index);
        if (e) {
            rt_snprintf(title, sizeof(title), "Alarm %02d:%02d", e->hour, e->minute);
        } else {
            rt_snprintf(title, sizeof(title), "Alarm!");
        }
    } else {
        rt_snprintf(title, sizeof(title), "Snooze Alarm");
    }
    lv_msgbox_add_title(g_ring_mbox, title);

    lv_obj_t *content = lv_msgbox_get_content(g_ring_mbox);

    lv_obj_t *big_label = lv_label_create(content);
    lv_label_set_text(big_label, LV_SYMBOL_BELL " Alarm!");
    lv_obj_set_style_text_font(big_label, &lv_font_montserrat_24, 0);
    lv_obj_align(big_label, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *dismiss_btn = lv_button_create(content);
    lv_obj_align(dismiss_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *dismiss_lbl = lv_label_create(dismiss_btn);
    lv_label_set_text(dismiss_lbl, "Dismiss");
    lv_obj_add_event_cb(dismiss_btn, ring_dismiss_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *snooze_btn = lv_button_create(content);
    lv_obj_align(snooze_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t *snooze_lbl = lv_label_create(snooze_btn);
    lv_label_set_text(snooze_lbl, "Snooze 5min");
    lv_obj_add_event_cb(snooze_btn, ring_snooze_cb, LV_EVENT_CLICKED, NULL);
}

static void ring_ui_async_cb(void *user_data)
{
    int idx = (int)(intptr_t)user_data;
    ring_ui_create(idx);
}

void alarm_ui_trigger_ring(int alarm_index)
{
    lv_async_call(ring_ui_async_cb, (void*)(intptr_t)alarm_index);
}
