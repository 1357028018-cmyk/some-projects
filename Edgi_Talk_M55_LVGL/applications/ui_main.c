#include "ui_main.h"
#include "ui_pressure.h"
#include "wifi_gui.h"
#include "ip_gui.h"
#include "alarm_ui.h"
#include <rtthread.h>
#include "lvgl.h"
#include <time.h>

static lv_obj_t * main_scr = NULL;
static lv_obj_t * heatmap_scr = NULL;
static lv_obj_t * g_wifi_panel = NULL;

static lv_obj_t * temp_label = NULL;
static lv_obj_t * humi_label = NULL;
static lv_obj_t * clock_label = NULL;
static lv_obj_t * date_label = NULL;
static lv_obj_t * top_wifi_status = NULL;
static lv_obj_t * temp_bar = NULL;
static lv_obj_t * humi_bar = NULL;
static lv_timer_t * sensor_timer = NULL;

static void sensor_update_timer_cb(lv_timer_t * timer)
{
    float temp, humi;
    char buf[32];
    int temp_i, temp_f, humi_i, humi_f;

    (void)timer;

    if (sensor_get_data(&temp, &humi) == RT_EOK) {
        temp_i = (int)temp;
        temp_f = (int)((temp >= 0 ? temp - (float)temp_i : (float)temp_i - temp) * 10);
        if (temp_f < 0) temp_f = -temp_f;
        humi_i = (int)humi;
        humi_f = (int)((humi >= 0 ? humi - (float)humi_i : (float)humi_i - humi) * 10);
        if (humi_f < 0) humi_f = -humi_f;

        if (temp_label) {
            rt_snprintf(buf, sizeof(buf), "%d°C", temp_i);
            lv_label_set_text(temp_label, buf);
        }
        if (humi_label) {
            rt_snprintf(buf, sizeof(buf), "%d%%", humi_i);
            lv_label_set_text(humi_label, buf);
        }
        if (temp_bar) {
            lv_bar_set_value(temp_bar, temp_i > 50 ? 50 : temp_i, LV_ANIM_OFF);
        }
        if (humi_bar) {
            lv_bar_set_value(humi_bar, humi_i > 100 ? 100 : humi_i, LV_ANIM_OFF);
        }
    } else {
        if (temp_label) {
            lv_label_set_text(temp_label, "--°C");
        }
        if (humi_label) {
            lv_label_set_text(humi_label, "--%");
        }
        if (temp_bar) {
            lv_bar_set_value(temp_bar, 0, LV_ANIM_OFF);
        }
        if (humi_bar) {
            lv_bar_set_value(humi_bar, 0, LV_ANIM_OFF);
        }
    }

    if (clock_label) {
        time_t now = time(RT_NULL);
        struct tm *lt = localtime(&now);
        if (lt && (lt->tm_year + 1900) > 2020) {
            rt_snprintf(buf, sizeof(buf), "%02d:%02d", lt->tm_hour, lt->tm_min);
        } else {
            rt_snprintf(buf, sizeof(buf), "--:--");
        }
        lv_label_set_text(clock_label, buf);
    }

    if (date_label) {
        time_t now = time(RT_NULL);
        struct tm *lt = localtime(&now);
        if (lt && (lt->tm_year + 1900) > 2020) {
            static const char *wdays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            static const char *mons[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
            rt_snprintf(buf, sizeof(buf), "%s, %s %d", wdays[lt->tm_wday], mons[lt->tm_mon], lt->tm_mday);
            lv_label_set_text(date_label, buf);
        }
    }
}

lv_obj_t * ui_main_get_main_screen(void)
{
    return main_scr;
}

static void icon_click_event_handler(lv_event_t * e)
{
    (void)e;
    if (heatmap_scr != NULL)
    {
        lv_scr_load(heatmap_scr);
    }
}

static void icon_bg_event_handler(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        icon_click_event_handler(e);
    }
}

static void wifi_click_event_handler(lv_event_t * e)
{
    (void)e;
    rt_kprintf("[UI] WiFi button clicked\r\n");
    wifi_gui_show_settings();
}

static void wifi_bg_event_handler(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        wifi_click_event_handler(e);
    }
}

static void ip_click_event_handler(lv_event_t * e)
{
    (void)e;
    rt_kprintf("[UI] IP button clicked\r\n");
    ip_gui_show_settings();
}

static void ip_bg_event_handler(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        ip_click_event_handler(e);
    }
}

static void alarm_click_event_handler(lv_event_t * e)
{
    (void)e;
    rt_kprintf("[UI] Alarm button clicked\r\n");
    alarm_ui_show_settings();
}

static void alarm_bg_event_handler(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        alarm_click_event_handler(e);
    }
}

void ui_main_init(void)
{
    rt_kprintf("[UI] ui_main_init start\r\n");

    ui_pressure_init();
    heatmap_scr = ui_pressure_get_screen();

    main_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_scr, lv_color_hex(0xF0F4F8), 0);
    lv_obj_set_style_bg_opa(main_scr, LV_OPA_COVER, 0);

    sensor_timer = lv_timer_create(sensor_update_timer_cb, 1000, NULL);

    /* ── 时钟区（居中大号） ── */
    clock_label = lv_label_create(main_scr);
    lv_label_set_text(clock_label, "--:--");
    lv_obj_set_style_text_color(clock_label, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_48, 0);
    lv_obj_align(clock_label, LV_ALIGN_TOP_MID, 0, 40);

    date_label = lv_label_create(main_scr);
    lv_label_set_text(date_label, "---, --- --");
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 98);

    top_wifi_status = lv_label_create(main_scr);
    lv_label_set_text(top_wifi_status, "WiFi: Disconnected");
    lv_obj_set_style_text_color(top_wifi_status, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(top_wifi_status, &lv_font_montserrat_12, 0);
    lv_obj_align(top_wifi_status, LV_ALIGN_TOP_MID, 0, 120);
    wifi_gui_set_status_label(top_wifi_status);

    /* ── 温湿度水平进度条 (y≈150) ── */
    /* 温度条 - 红色 */
    temp_bar = lv_bar_create(main_scr);
    lv_obj_set_size(temp_bar, 280, 18);
    lv_obj_set_style_radius(temp_bar, 9, 0);
    lv_obj_set_style_bg_color(temp_bar, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_border_width(temp_bar, 0, 0);
    lv_bar_set_range(temp_bar, 0, 50);
    lv_bar_set_value(temp_bar, 26, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(temp_bar, lv_color_hex(0xE74C3C), LV_PART_INDICATOR);
    lv_obj_set_style_radius(temp_bar, 9, LV_PART_INDICATOR);
    lv_obj_align(temp_bar, LV_ALIGN_TOP_MID, -42, 150);
    /* "Temp" 标签 */
    lv_obj_t *temp_lbl = lv_label_create(main_scr);
    lv_label_set_text(temp_lbl, "Temp");
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(temp_lbl, temp_bar, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    /* 温度值 */
    temp_label = lv_label_create(main_scr);
    lv_label_set_text(temp_label, "--°C");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(temp_label, temp_bar, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    /* 湿度条 - 蓝色 */
    humi_bar = lv_bar_create(main_scr);
    lv_obj_set_size(humi_bar, 280, 18);
    lv_obj_set_style_radius(humi_bar, 9, 0);
    lv_obj_set_style_bg_color(humi_bar, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_border_width(humi_bar, 0, 0);
    lv_bar_set_range(humi_bar, 0, 100);
    lv_bar_set_value(humi_bar, 58, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(humi_bar, lv_color_hex(0x3498DB), LV_PART_INDICATOR);
    lv_obj_set_style_radius(humi_bar, 9, LV_PART_INDICATOR);
    lv_obj_align(humi_bar, LV_ALIGN_TOP_MID, -42, 178);
    /* "Humi" 标签 */
    lv_obj_t *humi_lbl = lv_label_create(main_scr);
    lv_label_set_text(humi_lbl, "Humi");
    lv_obj_set_style_text_color(humi_lbl, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(humi_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(humi_lbl, humi_bar, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    /* 湿度值 */
    humi_label = lv_label_create(main_scr);
    lv_label_set_text(humi_label, "--%");
    lv_obj_set_style_text_color(humi_label, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_font(humi_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(humi_label, humi_bar, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    /* ── 卡片 2×2 ── */
    /* 坐标计算: 512px 宽, 卡片 120px, 行内间隙 60px
     * 左卡 x = (512 - 120*2 - 60) / 2 = 106
     * 右卡 x = 106 + 120 + 60 = 286
     * 第 1 行 y = 230, 第 2 行 y = 230 + 130 + 24 = 384 */
    lv_obj_t * icon_container = lv_obj_create(main_scr);
    lv_obj_remove_style_all(icon_container);
    lv_obj_set_size(icon_container, 120, 130);
    lv_obj_align(icon_container, LV_ALIGN_TOP_LEFT, 106, 230);
    lv_obj_set_style_radius(icon_container, 14, 0);
    lv_obj_set_style_bg_color(icon_container, lv_color_hex(0x2D3E50), 0);
    lv_obj_set_style_bg_opa(icon_container, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(icon_container, 4, 0);
    lv_obj_set_style_shadow_opa(icon_container, LV_OPA_10, 0);
    lv_obj_set_style_pad_all(icon_container, 0, 0);

    lv_obj_t * icon_bg = lv_obj_create(icon_container);
    lv_obj_remove_style_all(icon_bg);
    lv_obj_set_size(icon_bg, 58, 58);
    lv_obj_align(icon_bg, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_radius(icon_bg, 29, 0);
    lv_obj_set_style_bg_color(icon_bg, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_opa(icon_bg, LV_OPA_COVER, 0);

    lv_obj_t * health_icon = lv_label_create(icon_bg);
    lv_label_set_text(health_icon, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(health_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(health_icon, &lv_font_montserrat_24, 0);
    lv_obj_center(health_icon);

    lv_obj_t * label = lv_label_create(icon_container);
    lv_label_set_text(label, "Health");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 88);

    lv_obj_add_flag(icon_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(icon_container, icon_click_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(icon_bg, icon_bg_event_handler, LV_EVENT_CLICKED, NULL);

    /* WiFi 按钮 */
    {
        g_wifi_panel = lv_obj_create(main_scr);
        lv_obj_remove_style_all(g_wifi_panel);
        lv_obj_set_size(g_wifi_panel, 120, 130);
        lv_obj_align(g_wifi_panel, LV_ALIGN_TOP_LEFT, 286, 230);
        lv_obj_set_style_radius(g_wifi_panel, 14, 0);
        lv_obj_set_style_bg_color(g_wifi_panel, lv_color_hex(0x2D3E50), 0);
        lv_obj_set_style_bg_opa(g_wifi_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(g_wifi_panel, 4, 0);
        lv_obj_set_style_shadow_opa(g_wifi_panel, LV_OPA_10, 0);
        lv_obj_set_style_pad_all(g_wifi_panel, 0, 0);

        lv_obj_t * wifi_bg = lv_obj_create(g_wifi_panel);
        lv_obj_remove_style_all(wifi_bg);
        lv_obj_set_size(wifi_bg, 58, 58);
        lv_obj_align(wifi_bg, LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_set_style_radius(wifi_bg, 29, 0);
        lv_obj_set_style_bg_color(wifi_bg, lv_color_hex(0x3498DB), 0);
        lv_obj_set_style_bg_opa(wifi_bg, LV_OPA_COVER, 0);

        lv_obj_t * wifi_icon = lv_label_create(wifi_bg);
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_20, 0);
        lv_obj_center(wifi_icon);

        lv_obj_t * wifi_label = lv_label_create(g_wifi_panel);
        lv_label_set_text(wifi_label, "WiFi");
        lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
        lv_obj_align(wifi_label, LV_ALIGN_TOP_MID, 0, 88);

        lv_obj_add_flag(g_wifi_panel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_wifi_panel, wifi_click_event_handler, LV_EVENT_CLICKED, NULL);

        lv_obj_add_flag(wifi_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(wifi_bg, wifi_bg_event_handler, LV_EVENT_CLICKED, NULL);
    }

    /* IP Config 按钮 */
    {
        lv_obj_t *ip_panel = lv_obj_create(main_scr);
        lv_obj_remove_style_all(ip_panel);
        lv_obj_set_size(ip_panel, 120, 130);
        lv_obj_align(ip_panel, LV_ALIGN_TOP_LEFT, 106, 384);
        lv_obj_set_style_radius(ip_panel, 14, 0);
        lv_obj_set_style_bg_color(ip_panel, lv_color_hex(0x2D3E50), 0);
        lv_obj_set_style_bg_opa(ip_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(ip_panel, 4, 0);
        lv_obj_set_style_shadow_opa(ip_panel, LV_OPA_10, 0);
        lv_obj_set_style_pad_all(ip_panel, 0, 0);

        lv_obj_t *ip_bg = lv_obj_create(ip_panel);
        lv_obj_remove_style_all(ip_bg);
        lv_obj_set_size(ip_bg, 58, 58);
        lv_obj_align(ip_bg, LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_set_style_radius(ip_bg, 29, 0);
        lv_obj_set_style_bg_color(ip_bg, lv_color_hex(0xE67E22), 0);
        lv_obj_set_style_bg_opa(ip_bg, LV_OPA_COVER, 0);

        lv_obj_t *ip_icon = lv_label_create(ip_bg);
        lv_label_set_text(ip_icon, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_color(ip_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ip_icon, &lv_font_montserrat_20, 0);
        lv_obj_center(ip_icon);

        lv_obj_t *ip_label = lv_label_create(ip_panel);
        lv_label_set_text(ip_label, "IP Config");
        lv_obj_set_style_text_color(ip_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
        lv_obj_align(ip_label, LV_ALIGN_TOP_MID, 0, 88);

        lv_obj_add_flag(ip_panel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ip_panel, ip_click_event_handler, LV_EVENT_CLICKED, NULL);

        lv_obj_add_flag(ip_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ip_bg, ip_bg_event_handler, LV_EVENT_CLICKED, NULL);
    }

    /* Alarm 按钮 */
    {
        lv_obj_t *alarm_panel = lv_obj_create(main_scr);
        lv_obj_remove_style_all(alarm_panel);
        lv_obj_set_size(alarm_panel, 120, 130);
        lv_obj_align(alarm_panel, LV_ALIGN_TOP_LEFT, 286, 384);
        lv_obj_set_style_radius(alarm_panel, 14, 0);
        lv_obj_set_style_bg_color(alarm_panel, lv_color_hex(0x2D3E50), 0);
        lv_obj_set_style_bg_opa(alarm_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(alarm_panel, 4, 0);
        lv_obj_set_style_shadow_opa(alarm_panel, LV_OPA_10, 0);
        lv_obj_set_style_pad_all(alarm_panel, 0, 0);

        lv_obj_t *alarm_bg = lv_obj_create(alarm_panel);
        lv_obj_remove_style_all(alarm_bg);
        lv_obj_set_size(alarm_bg, 58, 58);
        lv_obj_align(alarm_bg, LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_set_style_radius(alarm_bg, 29, 0);
        lv_obj_set_style_bg_color(alarm_bg, lv_color_hex(0xE74C3C), 0);
        lv_obj_set_style_bg_opa(alarm_bg, LV_OPA_COVER, 0);

        lv_obj_t *alarm_icon = lv_label_create(alarm_bg);
        lv_label_set_text(alarm_icon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_color(alarm_icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(alarm_icon, &lv_font_montserrat_20, 0);
        lv_obj_center(alarm_icon);

        lv_obj_t *alarm_label = lv_label_create(alarm_panel);
        lv_label_set_text(alarm_label, "Alarm");
        lv_obj_set_style_text_color(alarm_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(alarm_label, &lv_font_montserrat_16, 0);
        lv_obj_align(alarm_label, LV_ALIGN_TOP_MID, 0, 88);

        lv_obj_add_flag(alarm_panel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(alarm_panel, alarm_click_event_handler, LV_EVENT_CLICKED, NULL);

        lv_obj_add_flag(alarm_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(alarm_bg, alarm_bg_event_handler, LV_EVENT_CLICKED, NULL);
    }

    /* ── Tagline ── */
    {
        lv_obj_t *tagline = lv_label_create(main_scr);
        lv_label_set_text(tagline, "Design for AI & AI for Design");
        lv_obj_set_style_text_color(tagline, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(tagline, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_letter_space(tagline, 2, 0);
        lv_obj_align(tagline, LV_ALIGN_TOP_MID, 0, 540);
    }

    /* ── 底部状态栏 ── */
    {
        lv_obj_t *sys_bar = lv_obj_create(main_scr);
        lv_obj_remove_style_all(sys_bar);
        lv_obj_set_size(sys_bar, 300, 34);
        lv_obj_align(sys_bar, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_radius(sys_bar, 8, 0);
        lv_obj_set_style_bg_color(sys_bar, lv_color_hex(0xE2E8F0), 0);
        lv_obj_set_style_bg_opa(sys_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(sys_bar, 0, 0);

        lv_obj_t *ok_dot = lv_obj_create(sys_bar);
        lv_obj_remove_style_all(ok_dot);
        lv_obj_set_size(ok_dot, 5, 5);
        lv_obj_set_style_radius(ok_dot, 3, 0);
        lv_obj_set_style_bg_color(ok_dot, lv_color_hex(0x10B981), 0);
        lv_obj_set_style_bg_opa(ok_dot, LV_OPA_COVER, 0);
        lv_obj_align(ok_dot, LV_ALIGN_LEFT_MID, 8, 0);

        lv_obj_t *ok_lbl = lv_label_create(sys_bar);
        lv_label_set_text(ok_lbl, "System OK");
        lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(ok_lbl, LV_ALIGN_LEFT_MID, 18, 0);

        lv_obj_t *fw_lbl = lv_label_create(sys_bar);
        lv_label_set_text(fw_lbl, "FW v2.1.3");
        lv_obj_set_style_text_color(fw_lbl, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_text_font(fw_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(fw_lbl, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t *uptime_lbl = lv_label_create(sys_bar);
        lv_label_set_text(uptime_lbl, "12:34:56");
        lv_obj_set_style_text_color(uptime_lbl, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_text_font(uptime_lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(uptime_lbl, LV_ALIGN_RIGHT_MID, -8, 0);
    }

    if (main_scr != NULL)
    {
        lv_scr_load(main_scr);
    }

    rt_kprintf("[UI] ui_main_init done\r\n");
}

void ui_main_wifi_init(lv_obj_t *parent)
{
    (void)parent;
    wifi_gui_init(g_wifi_panel);
}
