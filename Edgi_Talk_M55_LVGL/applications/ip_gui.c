#include "ip_gui.h"
#include "wifi_tcp_sender.h"
#include <string.h>

#define IP_LOG(fmt, ...)  rt_kprintf("[IP-GUI] " fmt "\r\n", ##__VA_ARGS__)

/* UI 引用 */
static lv_obj_t *g_ip_mbox = NULL;
static lv_obj_t *g_ip_textarea = NULL;
static lv_obj_t *g_ip_keyboard = NULL;

/* 检查 IP 格式是否合法 (x.x.x.x, 每段 0-255) */
static int ip_validate(const char *ip)
{
    int segs = 0, val = 0, len = 0;
    const char *p = ip;
    if (!ip || !*ip) return 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            len++;
            if (len > 3 || val > 255) return 0;
        } else if (*p == '.') {
            if (len == 0) return 0;
            segs++;
            val = 0;
            len = 0;
        } else {
            return 0;
        }
        p++;
    }
    if (len == 0) return 0;
    segs++;
    return (segs == 4) ? 1 : 0;
}

/* Textarea 获得焦点时切换键盘 */
static void ip_textarea_focus_event_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    IP_LOG("textarea focus, ta=%p, keyboard=%p", ta, g_ip_keyboard);
    if (g_ip_keyboard) {
        lv_keyboard_set_textarea(g_ip_keyboard, ta);
    }
}

/* Connect 按钮 */
static void ip_connect_event_cb(lv_event_t *e)
{
    IP_LOG("--- IP connect button clicked ---");
    if (g_ip_textarea == NULL) return;

    const char *new_ip = lv_textarea_get_text(g_ip_textarea);
    IP_LOG("new IP='%s'", new_ip ? new_ip : "(null)");

    if (!new_ip || !ip_validate(new_ip)) {
        IP_LOG("invalid IP, abort");
        if (g_ip_mbox)
            lv_msgbox_add_text(g_ip_mbox, "Invalid IP format!");
        return;
    }

    /* 更新目标 IP 并重新连接 */
    rt_strncpy(g_wifi_tcp.server_ip, new_ip, sizeof(g_wifi_tcp.server_ip) - 1);
    wifi_tcp_reconnect();
    IP_LOG("IP updated to %s, reconnecting...", new_ip);

    if (g_ip_mbox)
        lv_msgbox_add_text(g_ip_mbox, "IP updated! Reconnecting...");
}

/* Close 按钮 */
static void ip_close_event_cb(lv_event_t *e)
{
    IP_LOG("--- IP close button clicked ---");
    if (g_ip_keyboard) {
        lv_obj_del(g_ip_keyboard);
        g_ip_keyboard = NULL;
    }
    if (g_ip_mbox) {
        lv_msgbox_close(g_ip_mbox);
        g_ip_mbox = NULL;
    }
    g_ip_textarea = NULL;
}

/* IP 设置按钮点击 — 弹出输入框 */
void ip_gui_show_settings(void)
{
    IP_LOG("=== ip_gui_show_settings called ===");

    if (g_ip_mbox) {
        lv_msgbox_close(g_ip_mbox);
        g_ip_mbox = NULL;
    }

    g_ip_mbox = lv_msgbox_create(lv_scr_act());
    IP_LOG("created msgbox=%p", g_ip_mbox);
    lv_obj_set_size(g_ip_mbox, 360, 200);
    lv_obj_align(g_ip_mbox, LV_ALIGN_CENTER, 0, 0);

    /* 标题：显示当前目标 IP */
    {
        char title[64];
        rt_snprintf(title, sizeof(title), "Target IP: %s",
                    g_wifi_tcp.server_ip[0] ? g_wifi_tcp.server_ip : "(none)");
        lv_msgbox_add_title(g_ip_mbox, title);
    }

    /* IP 输入 */
    lv_obj_t *ip_label = lv_label_create(lv_msgbox_get_content(g_ip_mbox));
    lv_label_set_text(ip_label, "New IP:");
    lv_obj_set_width(ip_label, lv_pct(100));

    g_ip_textarea = lv_textarea_create(lv_msgbox_get_content(g_ip_mbox));
    lv_textarea_set_one_line(g_ip_textarea, true);
    lv_textarea_set_placeholder_text(g_ip_textarea, "e.g. 192.168.43.100");
    lv_obj_set_width(g_ip_textarea, lv_pct(100));
    lv_obj_add_event_cb(g_ip_textarea, ip_textarea_focus_event_cb, LV_EVENT_FOCUSED, NULL);

    /* 键盘 */
    g_ip_keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(g_ip_keyboard, 420, 188);
    lv_keyboard_set_textarea(g_ip_keyboard, g_ip_textarea);
    lv_obj_align_to(g_ip_keyboard, g_ip_mbox, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    /* 按钮 */
    lv_obj_t *connect_btn = lv_msgbox_add_footer_button(g_ip_mbox, "Connect");
    lv_obj_add_event_cb(connect_btn, ip_connect_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_btn = lv_msgbox_add_footer_button(g_ip_mbox, "Close");
    lv_obj_add_event_cb(close_btn, ip_close_event_cb, LV_EVENT_CLICKED, NULL);

    IP_LOG("=== ip_gui_show_settings done ===");
}
