#include "wifi_gui.h"
#include "wifi_saved.h"
#include <wlan_mgnt.h>
#include <lwip/ip_addr.h>
#include <lwip/inet.h>
#include <netdev.h>
#include <string.h>

#define WIFI_GUI_MAX_SSID_LEN   32
#define WIFI_GUI_MAX_PWD_LEN    64

#define WIFI_LOG(fmt, ...)  rt_kprintf("[WIFI-GUI] " fmt "\r\n", ##__VA_ARGS__)

/* UI 引用 */
static lv_obj_t *g_wifi_status_label = NULL;
static lv_obj_t *g_wifi_mbox = NULL;
static lv_obj_t *g_ssid_textarea = NULL;
static lv_obj_t *g_pwd_textarea = NULL;
static lv_obj_t *g_keyboard = NULL;
static lv_obj_t *g_saved_list = NULL;
static lv_obj_t *g_status_label = NULL;

static wifi_saved_entry_t g_saved_entries[WIFI_SAVED_MAX_ENTRIES];
static int g_saved_count = 0;

/* 前向声明 */
static void wifi_connect_event_cb(lv_event_t *e);
static void wifi_disconnect_event_cb(lv_event_t *e);
static void wifi_close_event_cb(lv_event_t *e);
static void wifi_connected_event_cb(int event, struct rt_wlan_buff *buff, void *parameter);
static void wifi_connect_fail_event_cb(int event, struct rt_wlan_buff *buff, void *parameter);
static void wifi_disconnected_event_cb(int event, struct rt_wlan_buff *buff, void *parameter);
static void wifi_textarea_focus_event_cb(lv_event_t *e);
static void wifi_memorize_event_cb(lv_event_t *e);
static void wifi_saved_item_click_cb(lv_event_t *e);
static void wifi_delete_ssid_event_cb(lv_event_t *e);
static void wifi_gui_refresh_saved_list(void);

/* 更新状态标签 - 在 LVGL 线程中调用 */
static void wifi_update_status_label(void)
{
    if (g_wifi_status_label == NULL) return;

    if (rt_wlan_is_connected())
    {
        struct rt_wlan_info info;
        if (rt_wlan_get_info(&info) == RT_EOK)
        {
            char status[64];
            rt_snprintf(status, sizeof(status), "WiFi: Connected (%s)", info.ssid.val);
            lv_label_set_text(g_wifi_status_label, status);
            WIFI_LOG("status label updated: '%s'", status);
            return;
        }
    }
    lv_label_set_text(g_wifi_status_label, "WiFi: Disconnected");
    WIFI_LOG("status label updated: 'WiFi: Disconnected'");
}

/* WiFi 事件回调 - 在 wlan work 线程中调用 */
static void wifi_connected_event_cb(int event, struct rt_wlan_buff *buff, void *parameter)
{
    WIFI_LOG("=== STA_CONNECTED event ===");

    char ip_text[64] = "IP: N/A";
    struct netdev *netdev = netdev_get_by_name("w0");
    if (netdev != NULL)
        rt_snprintf(ip_text, sizeof(ip_text), "IP: %s", inet_ntoa(netdev->ip_addr));
    WIFI_LOG("IP: %s", ip_text);

    if (g_wifi_mbox && g_status_label)
    {
        char buf[96];
        rt_snprintf(buf, sizeof(buf), "Connected (%s)", ip_text);
        lv_label_set_text(g_status_label, buf);
    }
    else
    {
        WIFI_LOG("g_wifi_mbox or g_status_label is NULL, skip UI update");
    }
    wifi_update_status_label();
}

static void wifi_connect_fail_event_cb(int event, struct rt_wlan_buff *buff, void *parameter)
{
    WIFI_LOG("=== STA_CONNECTED_FAIL event ===");
    if (g_wifi_mbox && g_status_label)
    {
        lv_label_set_text(g_status_label, "Connection Failed!");
    }
    wifi_update_status_label();
}

static void wifi_disconnected_event_cb(int event, struct rt_wlan_buff *buff, void *parameter)
{
    WIFI_LOG("=== STA_DISCONNECTED event ===");
    wifi_update_status_label();
}

/* Textarea 获得焦点时 - 切换键盘到当前 textarea */
static void wifi_textarea_focus_event_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    WIFI_LOG("textarea focus, ta=%p, g_keyboard=%p", ta, g_keyboard);
    if (g_keyboard)
    {
        lv_keyboard_set_textarea(g_keyboard, ta);
        WIFI_LOG("keyboard textarea switched to %p", ta);
    }
}

/* Memorize 按钮 - 保存当前 SSID/密码到 Flash */
static void wifi_memorize_event_cb(lv_event_t *e)
{
    (void)e;
    WIFI_LOG("--- memorize button clicked ---");

    const char *ssid = lv_textarea_get_text(g_ssid_textarea);
    const char *pwd = lv_textarea_get_text(g_pwd_textarea);

    if (!ssid || ssid[0] == '\0') {
        WIFI_LOG("ssid is empty, abort");
        if (g_status_label) lv_label_set_text(g_status_label, "SSID is empty!");
        return;
    }
    if (!pwd) pwd = "";

    if (wifi_saved_save(ssid, pwd) == RT_EOK) {
        wifi_gui_refresh_saved_list();
        WIFI_LOG("memorized '%s'", ssid);
        if (g_status_label) {
            char buf[64];
            rt_snprintf(buf, sizeof(buf), "Saved: %s", ssid);
            lv_label_set_text(g_status_label, buf);
        }
    } else {
        if (g_status_label) lv_label_set_text(g_status_label, "Save failed!");
    }
}

/* 已存 WiFi 列表点击 - 填充密码到输入框，不自动连接 */
static void wifi_saved_item_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    WIFI_LOG("--- saved item clicked, btn=%p ---", btn);

    const char *ssid = (const char *)lv_obj_get_user_data(btn);
    if (!ssid || ssid[0] == '\0') {
        WIFI_LOG("ssid from user_data is empty");
        return;
    }
    WIFI_LOG("selected saved network: '%s'", ssid);

    for (int i = 0; i < g_saved_count; i++) {
        if (rt_strcmp(g_saved_entries[i].ssid, ssid) == 0) {
            lv_textarea_set_text(g_ssid_textarea, g_saved_entries[i].ssid);
            lv_textarea_set_text(g_pwd_textarea, g_saved_entries[i].pwd);
            if (g_status_label) {
                char buf[64];
                rt_snprintf(buf, sizeof(buf), "Loaded: %s", ssid);
                lv_label_set_text(g_status_label, buf);
            }
            return;
        }
    }
    WIFI_LOG("saved entry not found for '%s'", ssid);
}

/* Delete 按钮 - 根据输入框中的 SSID 删除已存记录 */
static void wifi_delete_ssid_event_cb(lv_event_t *e)
{
    (void)e;
    WIFI_LOG("--- delete button clicked ---");

    const char *ssid = lv_textarea_get_text(g_ssid_textarea);
    if (!ssid || ssid[0] == '\0') {
        WIFI_LOG("ssid is empty, abort");
        if (g_status_label) lv_label_set_text(g_status_label, "SSID is empty!");
        return;
    }

    int found = 0;
    for (int i = 0; i < g_saved_count; i++) {
        if (rt_strcmp(g_saved_entries[i].ssid, ssid) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        WIFI_LOG("'%s' not in saved list", ssid);
        if (g_status_label) {
            char buf[64];
            rt_snprintf(buf, sizeof(buf), "Not found: %s", ssid);
            lv_label_set_text(g_status_label, buf);
        }
        return;
    }

    wifi_saved_delete(ssid);

    lv_textarea_set_text(g_ssid_textarea, "");
    lv_textarea_set_text(g_pwd_textarea, "");

    wifi_gui_refresh_saved_list();

    if (g_status_label) {
        char buf[64];
        rt_snprintf(buf, sizeof(buf), "Deleted: %s", ssid);
        lv_label_set_text(g_status_label, buf);
    }
    WIFI_LOG("deleted '%s'", ssid);
}

/* 刷新已存 WiFi 列表 */
static void wifi_gui_refresh_saved_list(void)
{
    if (!g_saved_list) return;

    lv_obj_clean(g_saved_list);

    lv_obj_t *header = lv_list_add_text(g_saved_list, "Saved WiFi");
    lv_obj_set_style_text_color(header, lv_color_hex(0x3498DB), 0);

    g_saved_count = wifi_saved_load(g_saved_entries, WIFI_SAVED_MAX_ENTRIES);
    for (int i = 0; i < g_saved_count; i++) {
        lv_obj_t *btn = lv_list_add_button(g_saved_list, LV_SYMBOL_WIFI, g_saved_entries[i].ssid);
        lv_obj_set_user_data(btn, g_saved_entries[i].ssid);
        lv_obj_add_event_cb(btn, wifi_saved_item_click_cb, LV_EVENT_CLICKED, NULL);
    }
    WIFI_LOG("refreshed saved list: %d networks", g_saved_count);
}

/* Connect 按钮 - 直接调用 rt_wlan_connect,非阻塞 */
static void wifi_connect_event_cb(lv_event_t *e)
{
    (void)e;
    WIFI_LOG("--- connect button clicked ---");
    if (g_ssid_textarea == NULL || g_pwd_textarea == NULL)
    {
        WIFI_LOG("ERROR: textareas are NULL, abort");
        return;
    }

    const char *ssid = lv_textarea_get_text(g_ssid_textarea);
    const char *password = lv_textarea_get_text(g_pwd_textarea);
    WIFI_LOG("ssid='%s', password length=%d (0=open wifi)",
             ssid ? ssid : "(null)",
             password ? (int)rt_strlen(password) : 0);

    if (!ssid || ssid[0] == '\0')
    {
        WIFI_LOG("ssid is empty, abort");
        return;
    }

    if (rt_wlan_is_connected())
    {
        WIFI_LOG("already connected, disconnecting first");
        rt_wlan_disconnect();
        rt_thread_mdelay(500);
    }

    WIFI_LOG("registering event handlers...");
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED, wifi_connected_event_cb, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED_FAIL, wifi_connect_fail_event_cb, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wifi_disconnected_event_cb, RT_NULL);
    WIFI_LOG("event handlers registered");

    if (!password) password = "";

    WIFI_LOG("calling rt_wlan_connect('%s', '%s')...", ssid, password);
    rt_err_t ret = rt_wlan_connect(ssid, password);
    WIFI_LOG("rt_wlan_connect returned %d (0=ok)", ret);

    if (g_wifi_mbox && g_status_label)
    {
        char buf[64];
        rt_snprintf(buf, sizeof(buf), "Connecting: %s...", ssid);
        lv_label_set_text(g_status_label, buf);
    }
}

/* Disconnect 按钮 - 直接调用 rt_wlan_disconnect,非阻塞 */
static void wifi_disconnect_event_cb(lv_event_t *e)
{
    (void)e;
    WIFI_LOG("--- disconnect button clicked ---");
    rt_bool_t was_connected = rt_wlan_is_connected();
    WIFI_LOG("is_connected before = %d", was_connected);

    if (was_connected)
    {
        rt_wlan_disconnect();
        WIFI_LOG("rt_wlan_disconnect called");
    }
    wifi_update_status_label();
    if (g_wifi_mbox && g_status_label)
    {
        lv_label_set_text(g_status_label, "Disconnected");
    }
}

/* Close 按钮 */
static void wifi_close_event_cb(lv_event_t *e)
{
    (void)e;
    WIFI_LOG("--- close button clicked ---");
    if (g_keyboard)
    {
        WIFI_LOG("deleting keyboard=%p", g_keyboard);
        lv_obj_del(g_keyboard);
        g_keyboard = NULL;
    }
    if (g_wifi_mbox)
    {
        WIFI_LOG("closing msgbox=%p", g_wifi_mbox);
        lv_msgbox_close(g_wifi_mbox);
        g_wifi_mbox = NULL;
    }
    g_ssid_textarea = NULL;
    g_pwd_textarea = NULL;
    g_saved_list = NULL;
    g_status_label = NULL;
}

/* WiFi 按钮点击 - 弹出输入框 */
void wifi_gui_show_settings(void)
{
    WIFI_LOG("=== wifi_gui_show_settings called ===");
    WIFI_LOG("is_connected=%d", rt_wlan_is_connected());

    if (g_wifi_mbox)
    {
        WIFI_LOG("closing existing msgbox=%p first", g_wifi_mbox);
        lv_msgbox_close(g_wifi_mbox);
        g_wifi_mbox = NULL;
    }

    g_wifi_mbox = lv_msgbox_create(lv_scr_act());
    WIFI_LOG("created msgbox=%p", g_wifi_mbox);
    lv_obj_set_size(g_wifi_mbox, 400, 300);
    lv_obj_align(g_wifi_mbox, LV_ALIGN_CENTER, 0, 0);

    if (rt_wlan_is_connected())
    {
        struct rt_wlan_info info;
        rt_wlan_get_info(&info);
        char title[64];
        rt_snprintf(title, sizeof(title), "Connected: %s", info.ssid.val);
        lv_msgbox_add_title(g_wifi_mbox, title);
    }
    else
    {
        lv_msgbox_add_title(g_wifi_mbox, "WiFi Settings");
    }

    lv_obj_t *content = lv_msgbox_get_content(g_wifi_mbox);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(content, 2, 0);

    /* ========== 左列：已存 WiFi 列表 ========== */
    lv_obj_t *left_panel = lv_obj_create(content);
    lv_obj_set_size(left_panel, 136, lv_pct(100));
    lv_obj_set_pos(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 0, 0);
    lv_obj_set_style_border_width(left_panel, 1, 0);
    lv_obj_set_style_border_color(left_panel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(left_panel, 6, 0);

    g_saved_list = lv_list_create(left_panel);
    lv_obj_set_size(g_saved_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_border_width(g_saved_list, 0, 0);

    wifi_gui_refresh_saved_list();

    /* ========== 右列：输入区域 ========== */
    lv_obj_t *right_panel = lv_obj_create(content);
    lv_obj_set_size(right_panel, 250, lv_pct(100));
    lv_obj_set_pos(right_panel, 144, 0);
    lv_obj_set_style_pad_all(right_panel, 0, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_radius(right_panel, 6, 0);

    g_status_label = lv_label_create(right_panel);
    lv_label_set_text(g_status_label, "");
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x3498DB), 0);
    lv_obj_set_pos(g_status_label, 6, 4);
    lv_obj_set_width(g_status_label, 238);

    lv_obj_t *ssid_label = lv_label_create(right_panel);
    lv_label_set_text(ssid_label, "SSID:");
    lv_obj_set_pos(ssid_label, 6, 28);
    lv_obj_set_width(ssid_label, 238);

    g_ssid_textarea = lv_textarea_create(right_panel);
    lv_textarea_set_one_line(g_ssid_textarea, true);
    lv_textarea_set_placeholder_text(g_ssid_textarea, "WiFi name");
    lv_obj_set_pos(g_ssid_textarea, 6, 44);
    lv_obj_set_width(g_ssid_textarea, 238);
    lv_obj_set_height(g_ssid_textarea, 40);
    lv_obj_add_event_cb(g_ssid_textarea, wifi_textarea_focus_event_cb, LV_EVENT_FOCUSED, NULL);
    WIFI_LOG("ssid_textarea=%p", g_ssid_textarea);

    lv_obj_t *pwd_label = lv_label_create(right_panel);
    lv_label_set_text(pwd_label, "Password:");
    lv_obj_set_pos(pwd_label, 6, 94);
    lv_obj_set_width(pwd_label, 238);

    g_pwd_textarea = lv_textarea_create(right_panel);
    lv_textarea_set_one_line(g_pwd_textarea, true);
    lv_textarea_set_password_mode(g_pwd_textarea, true);
    lv_textarea_set_placeholder_text(g_pwd_textarea, "password (empty for open WiFi)");
    lv_obj_set_pos(g_pwd_textarea, 6, 110);
    lv_obj_set_width(g_pwd_textarea, 238);
    lv_obj_set_height(g_pwd_textarea, 40);
    lv_obj_add_event_cb(g_pwd_textarea, wifi_textarea_focus_event_cb, LV_EVENT_FOCUSED, NULL);
    WIFI_LOG("pwd_textarea=%p", g_pwd_textarea);

    lv_obj_t *memorize_btn = lv_button_create(right_panel);
    lv_obj_set_width(memorize_btn, 110);
    lv_obj_set_pos(memorize_btn, 6, 160);
    lv_obj_set_style_bg_color(memorize_btn, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(memorize_btn, 6, 0);

    lv_obj_t *memorize_label = lv_label_create(memorize_btn);
    lv_label_set_text(memorize_label, "Save");
    lv_obj_set_style_text_color(memorize_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(memorize_label);

    lv_obj_add_event_cb(memorize_btn, wifi_memorize_event_cb, LV_EVENT_CLICKED, NULL);
    WIFI_LOG("memorize_btn=%p", memorize_btn);

    lv_obj_t *delete_btn = lv_button_create(right_panel);
    lv_obj_set_width(delete_btn, 110);
    lv_obj_set_pos(delete_btn, 124, 160);
    lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_radius(delete_btn, 6, 0);

    lv_obj_t *delete_label = lv_label_create(delete_btn);
    lv_label_set_text(delete_label, "Delete");
    lv_obj_set_style_text_color(delete_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(delete_label);

    lv_obj_add_event_cb(delete_btn, wifi_delete_ssid_event_cb, LV_EVENT_CLICKED, NULL);
    WIFI_LOG("delete_btn=%p", delete_btn);

    /* 键盘 */
    g_keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(g_keyboard, 400, 188);
    lv_keyboard_set_textarea(g_keyboard, g_ssid_textarea);
    lv_obj_align_to(g_keyboard, g_wifi_mbox, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    WIFI_LOG("keyboard=%p", g_keyboard);

    /* 底部按钮 */
    lv_obj_t *connect_btn = lv_msgbox_add_footer_button(g_wifi_mbox, "Connect");
    lv_obj_add_event_cb(connect_btn, wifi_connect_event_cb, LV_EVENT_CLICKED, NULL);
    WIFI_LOG("connect_btn=%p", connect_btn);

    lv_obj_t *disconnect_btn = lv_msgbox_add_footer_button(g_wifi_mbox, "Disconnect");
    lv_obj_add_event_cb(disconnect_btn, wifi_disconnect_event_cb, LV_EVENT_CLICKED, NULL);
    WIFI_LOG("disconnect_btn=%p", disconnect_btn);

    lv_obj_t *close_btn = lv_msgbox_add_footer_button(g_wifi_mbox, "Close");
    lv_obj_add_event_cb(close_btn, wifi_close_event_cb, LV_EVENT_CLICKED, NULL);
    WIFI_LOG("close_btn=%p", close_btn);

    WIFI_LOG("=== wifi_gui_show_settings done ===");
}

void wifi_gui_set_status_label(lv_obj_t *label)
{
    g_wifi_status_label = label;
    wifi_update_status_label();
}

void wifi_gui_update_status(void)
{
    wifi_update_status_label();
}

/* WiFi 自动连接线程 — 防止阻塞 LVGL 主线程 */
static void wifi_auto_connect_entry(void *parameter)
{
    wifi_saved_auto_connect();
}

void wifi_gui_init(lv_obj_t *parent)
{
    (void)parent;
    WIFI_LOG("=== wifi_gui_init called ===");

    g_saved_count = wifi_saved_load(g_saved_entries, WIFI_SAVED_MAX_ENTRIES);
    WIFI_LOG("loaded %d saved networks on init", g_saved_count);

    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_CONNECTED, wifi_connected_event_cb, RT_NULL);
    rt_wlan_register_event_handler(RT_WLAN_EVT_STA_DISCONNECTED, wifi_disconnected_event_cb, RT_NULL);
    WIFI_LOG("registered CONNECTED/DISCONNECTED event handlers");

    /* 独立线程自动连接，不阻塞 LVGL */
    rt_thread_t tid = rt_thread_create("wifi_con",
        wifi_auto_connect_entry, RT_NULL,
        4096, RT_THREAD_PRIORITY_MAX - 2, 10);
    if (tid) {
        rt_thread_startup(tid);
        WIFI_LOG("started wifi_auto_connect thread");
    } else {
        WIFI_LOG("ERROR: create thread failed, fallback to sync connect");
        wifi_saved_auto_connect();
    }

    WIFI_LOG("=== wifi_gui_init done ===");
}
