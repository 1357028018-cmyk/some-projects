#include "wifi_saved.h"
#include "wifi_gui.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <wlan_mgnt.h>

#define WIFI_SAVED_LOG(fmt, ...)  rt_kprintf("[WIFI-SAVED] " fmt "\r\n", ##__VA_ARGS__)
#define WIFI_AUTO_CONNECT_TIMEOUT_MS  10000

int wifi_saved_load(wifi_saved_entry_t *entries, int max_count)
{
    int fd = open(WIFI_SAVED_FILE, O_RDONLY, 0);
    if (fd < 0) {
        return 0;
    }

    rt_uint16_t count;
    if (read(fd, &count, sizeof(count)) != sizeof(count)) {
        close(fd);
        return 0;
    }

    if (count > max_count) {
        count = max_count;
    }

    int read_count = 0;
    for (int i = 0; i < count; i++) {
        wifi_saved_entry_t entry;
        if (read(fd, &entry, sizeof(entry)) == sizeof(entry)) {
            entries[read_count++] = entry;
        } else {
            break;
        }
    }

    close(fd);
    WIFI_SAVED_LOG("loaded %d saved networks", read_count);
    return read_count;
}

static int wifi_saved_write_all(wifi_saved_entry_t *entries, int count)
{
    int fd = open(WIFI_SAVED_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        WIFI_SAVED_LOG("failed to open file for writing");
        return -RT_ERROR;
    }

    rt_uint16_t c = count;
    write(fd, &c, sizeof(c));
    for (int i = 0; i < count; i++) {
        write(fd, &entries[i], sizeof(wifi_saved_entry_t));
    }

    close(fd);
    return RT_EOK;
}

int wifi_saved_save(const char *ssid, const char *pwd)
{
    if (!ssid || !ssid[0] || !pwd) {
        return -RT_ERROR;
    }

    wifi_saved_entry_t entries[WIFI_SAVED_MAX_ENTRIES];
    int count = wifi_saved_load(entries, WIFI_SAVED_MAX_ENTRIES);

    for (int i = 0; i < count; i++) {
        if (rt_strcmp(entries[i].ssid, ssid) == 0) {
            rt_strncpy(entries[i].pwd, pwd, WIFI_SAVED_PWD_MAX - 1);
            entries[i].pwd[WIFI_SAVED_PWD_MAX - 1] = '\0';
            wifi_saved_write_all(entries, count);
            WIFI_SAVED_LOG("updated password for '%s'", ssid);
            return RT_EOK;
        }
    }

    if (count >= WIFI_SAVED_MAX_ENTRIES) {
        WIFI_SAVED_LOG("max entries reached, cannot save '%s'", ssid);
        return -RT_ERROR;
    }

    wifi_saved_entry_t *entry = &entries[count];
    memset(entry, 0, sizeof(wifi_saved_entry_t));
    rt_strncpy(entry->ssid, ssid, WIFI_SAVED_SSID_MAX - 1);
    entry->ssid[WIFI_SAVED_SSID_MAX - 1] = '\0';
    rt_strncpy(entry->pwd, pwd, WIFI_SAVED_PWD_MAX - 1);
    entry->pwd[WIFI_SAVED_PWD_MAX - 1] = '\0';
    count++;

    wifi_saved_write_all(entries, count);
    WIFI_SAVED_LOG("saved '%s'", ssid);
    return RT_EOK;
}

int wifi_saved_delete(const char *ssid)
{
    if (!ssid) {
        return -RT_ERROR;
    }

    wifi_saved_entry_t entries[WIFI_SAVED_MAX_ENTRIES];
    int count = wifi_saved_load(entries, WIFI_SAVED_MAX_ENTRIES);

    int found = -1;
    for (int i = 0; i < count; i++) {
        if (rt_strcmp(entries[i].ssid, ssid) == 0) {
            found = i;
            break;
        }
    }

    if (found < 0) {
        return -RT_ERROR;
    }

    for (int i = found; i < count - 1; i++) {
        entries[i] = entries[i + 1];
    }
    count--;

    wifi_saved_write_all(entries, count);
    WIFI_SAVED_LOG("deleted '%s'", ssid);
    return RT_EOK;
}

int wifi_saved_auto_connect(void)
{
    wifi_saved_entry_t entries[WIFI_SAVED_MAX_ENTRIES];
    int count = wifi_saved_load(entries, WIFI_SAVED_MAX_ENTRIES);
    if (count == 0) {
        WIFI_SAVED_LOG("no saved networks to auto-connect");
        return 0;
    }

    if (rt_wlan_is_connected()) {
        WIFI_SAVED_LOG("already connected, skipping auto-connect");
        return 1;
    }

    WIFI_SAVED_LOG("auto-connecting to %d saved network(s)...", count);

    for (int i = count - 1; i >= 0; i--) {
        WIFI_SAVED_LOG("trying '%s'...", entries[i].ssid);

        int ret = rt_wlan_connect(entries[i].ssid, entries[i].pwd);
        if (ret != RT_EOK) {
            WIFI_SAVED_LOG("'%s' connect failed immediately", entries[i].ssid);
            continue;
        }

        rt_tick_t start = rt_tick_get();
        while (rt_wlan_is_connected() != RT_TRUE) {
            if (rt_tick_get() - start > rt_tick_from_millisecond(WIFI_AUTO_CONNECT_TIMEOUT_MS)) {
                rt_wlan_disconnect();
                WIFI_SAVED_LOG("'%s' timeout %dms", entries[i].ssid, WIFI_AUTO_CONNECT_TIMEOUT_MS);
                break;
            }
            rt_thread_mdelay(200);
        }

        if (rt_wlan_is_connected()) {
            WIFI_SAVED_LOG("auto-connected to '%s'", entries[i].ssid);
            wifi_gui_update_status();
            return 1;
        }
    }

    WIFI_SAVED_LOG("all networks failed, waiting for manual connect");
    return 0;
}
