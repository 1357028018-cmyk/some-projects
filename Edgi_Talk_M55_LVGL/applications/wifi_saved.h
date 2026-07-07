#ifndef WIFI_SAVED_H
#define WIFI_SAVED_H

#include <rtthread.h>

#define WIFI_SAVED_SSID_MAX     32
#define WIFI_SAVED_PWD_MAX      64
#define WIFI_SAVED_MAX_ENTRIES  10
#define WIFI_SAVED_FILE         "/flash/wifi_saved.dat"

typedef struct {
    char ssid[WIFI_SAVED_SSID_MAX];
    char pwd[WIFI_SAVED_PWD_MAX];
} wifi_saved_entry_t;

int wifi_saved_load(wifi_saved_entry_t *entries, int max_count);
int wifi_saved_save(const char *ssid, const char *pwd);
int wifi_saved_delete(const char *ssid);
int wifi_saved_auto_connect(void);

#endif
