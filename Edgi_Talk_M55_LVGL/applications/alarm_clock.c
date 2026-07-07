#include "alarm_clock.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
// #include <math.h>
#include <rtdevice.h>
#include <sys/time.h>
#include <time.h>
// #include <drivers/audio.h>

#define ALARM_LOG(fmt, ...)  rt_kprintf("[ALARM] " fmt "\r\n", ##__VA_ARGS__)

static alarm_store_t g_store;
static rt_alarm_t g_alarm_handles[ALARM_MAX_COUNT];
static rt_alarm_t g_weekly_handles[ALARM_MAX_COUNT][7];
static rt_alarm_t g_snooze_alarm = RT_NULL;
static volatile rt_bool_t g_ringing = RT_FALSE;
static rt_bool_t g_snoozing = RT_FALSE;

extern void alarm_ui_trigger_ring(int alarm_index);

int alarm_clock_load(alarm_store_t *store)
{
    memset(store, 0, sizeof(alarm_store_t));

    int fd = open(ALARM_FILE_PATH, O_RDONLY, 0);
    if (fd < 0) {
        ALARM_LOG("no saved alarm file, using defaults");
        return 0;
    }

    rt_uint16_t count;
    if (read(fd, &count, sizeof(count)) != sizeof(count)) {
        close(fd);
        return 0;
    }

    if (count > ALARM_MAX_COUNT) {
        count = ALARM_MAX_COUNT;
    }

    for (int i = 0; i < count; i++) {
        alarm_entry_t entry;
        if (read(fd, &entry, sizeof(entry)) == sizeof(entry)) {
            store->alarms[i] = entry;
        } else {
            break;
        }
    }

    close(fd);
    ALARM_LOG("loaded %d alarms", count);
    return count;
}

int alarm_clock_save(const alarm_store_t *store)
{
    int fd = open(ALARM_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        ALARM_LOG("failed to open alarm file for writing");
        return -RT_ERROR;
    }

    rt_uint16_t count = ALARM_MAX_COUNT;
    write(fd, &count, sizeof(count));
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        write(fd, &store->alarms[i], sizeof(alarm_entry_t));
    }

    close(fd);
    ALARM_LOG("saved %d alarms", count);
    return RT_EOK;
}

// #define ALARM_SOUND_STACK       2048
// #define ALARM_SOUND_PRIORITY    15
// #define BEEP_SAMPLE_RATE        16000
// #define BEEP_DURATION_MS        500
// #define BEEP_FREQ               1000
// #define BEEP_SAMPLES            (BEEP_SAMPLE_RATE * BEEP_DURATION_MS / 1000)

// static rt_thread_t g_sound_thread = RT_NULL;
// static volatile rt_bool_t g_sound_playing = RT_FALSE;

// static void generate_beep(int16_t *buf, int samples)
// {
//     for (int i = 0; i < samples; i++) {
//         int16_t val = (int16_t)(32767.0 * 0.3 *
//                       sinf(2.0f * 3.14159265f * BEEP_FREQ * i / BEEP_SAMPLE_RATE));
//         buf[i * 2]     = val;
//         buf[i * 2 + 1] = val;
//     }
// }

// static void alarm_sound_entry(void *param)
// {
//     (void)param;
// 
//     rt_device_t snd = rt_device_find("sound0");
//     if (snd == RT_NULL) {
//         ALARM_LOG("sound0 device not found!");
//         return;
//     }
// 
//     struct rt_audio_caps caps;
//     memset(&caps, 0, sizeof(caps));
//     caps.main_type = AUDIO_TYPE_MIXER;
//     caps.sub_type  = AUDIO_MIXER_VOLUME;
//     caps.udata.value = 80;
//     rt_device_control(snd, AUDIO_CTL_CONFIGURE, &caps);
// 
//     rt_err_t err = rt_device_open(snd, RT_DEVICE_OFLAG_WRONLY);
//     if (err != RT_EOK) {
//         ALARM_LOG("failed to open sound0, err=%d", err);
//         return;
//     }
//     ALARM_LOG("sound0 opened, playing beep...");
// 
//     int16_t *buf = (int16_t *)rt_malloc(BEEP_SAMPLES * 2 * sizeof(int16_t));
//     if (buf == RT_NULL) {
//         ALARM_LOG("failed to alloc beep buffer");
//         rt_device_close(snd);
//         return;
//     }
// 
//     generate_beep(buf, BEEP_SAMPLES);
// 
//     int total_bytes = BEEP_SAMPLES * 2 * sizeof(int16_t);
//     int chunk = 1024;
// 
//     while (g_sound_playing) {
//         for (int off = 0; off < total_bytes; off += chunk) {
//             if (!g_sound_playing) break;
//             int len = (total_bytes - off < chunk) ? (total_bytes - off) : chunk;
//             rt_device_write(snd, 0, (rt_uint8_t *)buf + off, len);
//         }
//         rt_thread_mdelay(BEEP_DURATION_MS);
//         rt_thread_mdelay(BEEP_DURATION_MS);
//     }
// 
//     rt_free(buf);
//     rt_device_close(snd);
//     ALARM_LOG("sound thread exit");
// }
// 
// static void alarm_play_start(void)
// {
//     if (g_sound_playing) return;
//     g_sound_playing = RT_TRUE;
//     g_sound_thread = rt_thread_create("alarm_snd", alarm_sound_entry, RT_NULL,
//                                        ALARM_SOUND_STACK, ALARM_SOUND_PRIORITY, 10);
//     if (g_sound_thread) {
//         rt_thread_startup(g_sound_thread);
//         ALARM_LOG("alarm sound started");
//     }
// }
// 
// static void alarm_play_stop(void)
// {
//     g_sound_playing = RT_FALSE;
//     ALARM_LOG("alarm sound stopped");
// }

static void alarm_trigger_cb(rt_alarm_t alarm, time_t timestamp)
{
    int idx = -1;
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        if (g_alarm_handles[i] == alarm) {
            idx = i;
            break;
        }
        for (int d = 0; d < 7; d++) {
            if (g_weekly_handles[i][d] == alarm) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) break;
    }

    if (g_snoozing && alarm == g_snooze_alarm) {
        g_snoozing = RT_FALSE;
        g_snooze_alarm = RT_NULL;
        ALARM_LOG("snooze alarm triggered");
        idx = -1;
    }

    ALARM_LOG("alarm triggered, idx=%d, time=%ld", idx, (long)timestamp);

    if (idx >= 0 && g_store.alarms[idx].repeat == 0) {
        g_store.alarms[idx].enabled = 0;
        alarm_clock_save(&g_store);
    }

    g_ringing = RT_TRUE;
    // alarm_play_start();
    alarm_ui_trigger_ring(idx);
}

static void alarm_create_one(int idx)
{
    alarm_entry_t *e = &g_store.alarms[idx];
    if (!e->enabled) return;

    struct rt_alarm_setup setup;
    memset(&setup, 0, sizeof(setup));

    /* alarm framework uses gmtime_r internally, so convert local time to UTC */
    time_t now = time(RT_NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);

    /* e->hour/e->minute are local time; convert to UTC by subtracting timezone offset */
    int utc_hour = e->hour;
    int utc_min  = e->minute;
    /* RT_LIBC_TZ_DEFAULT_HOUR is 8 (UTC+8), subtract it */
#if defined(RT_LIBC_USING_LIGHT_TZ_DST)
    extern long rt_tz_get(void);
    long tz_sec = rt_tz_get();
    int tz_offset_min = (int)(tz_sec / 60);
#else
    int tz_offset_min = 8 * 60;
#endif
    int total_min = utc_hour * 60 + utc_min - tz_offset_min;
    while (total_min < 0) total_min += 24 * 60;
    while (total_min >= 24 * 60) total_min -= 24 * 60;
    utc_hour = total_min / 60;
    utc_min  = total_min % 60;

    setup.wktime.tm_hour = utc_hour;
    setup.wktime.tm_min  = utc_min;
    setup.wktime.tm_sec  = 0;

    if (e->repeat == ALARM_REPEAT_EVERYDAY) {
        setup.flag = RT_ALARM_DAILY;
        setup.wktime.tm_wday = RT_ALARM_TM_NOW;
        g_alarm_handles[idx] = rt_alarm_create(alarm_trigger_cb, &setup);
        if (g_alarm_handles[idx]) rt_alarm_start(g_alarm_handles[idx]);
        ALARM_LOG("alarm %d: DAILY %02d:%02d (utc %02d:%02d)", idx, e->hour, e->minute, utc_hour, utc_min);
    } else if (e->repeat == 0) {
        setup.flag = RT_ALARM_ONESHOT;
        setup.wktime.tm_year = tm_utc.tm_year;
        setup.wktime.tm_mon  = tm_utc.tm_mon;
        setup.wktime.tm_mday = tm_utc.tm_mday;
        if (utc_hour < tm_utc.tm_hour ||
            (utc_hour == tm_utc.tm_hour && utc_min <= tm_utc.tm_min)) {
            setup.wktime.tm_mday += 1;
        }
        g_alarm_handles[idx] = rt_alarm_create(alarm_trigger_cb, &setup);
        if (g_alarm_handles[idx]) rt_alarm_start(g_alarm_handles[idx]);
        ALARM_LOG("alarm %d: ONESHOT %02d:%02d (utc %02d:%02d)", idx, e->hour, e->minute, utc_hour, utc_min);
    } else {
        setup.flag = RT_ALARM_WEEKLY;
        for (int d = 0; d < 7; d++) {
            if (e->repeat & (1 << d)) {
                setup.wktime.tm_wday = d;
                rt_alarm_t a = rt_alarm_create(alarm_trigger_cb, &setup);
                if (a) {
                    rt_alarm_start(a);
                    g_weekly_handles[idx][d] = a;
                }
                ALARM_LOG("alarm %d: WEEKLY wday=%d %02d:%02d (utc %02d:%02d)", idx, d, e->hour, e->minute, utc_hour, utc_min);
            }
        }
    }
}

static void alarm_delete_one(int idx)
{
    if (g_alarm_handles[idx]) {
        rt_alarm_delete(g_alarm_handles[idx]);
        g_alarm_handles[idx] = RT_NULL;
    }
    for (int d = 0; d < 7; d++) {
        if (g_weekly_handles[idx][d]) {
            rt_alarm_delete(g_weekly_handles[idx][d]);
            g_weekly_handles[idx][d] = RT_NULL;
        }
    }
}

void alarm_clock_apply(const alarm_store_t *store)
{
    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        alarm_delete_one(i);
    }

    if (store) {
        memcpy(&g_store, store, sizeof(alarm_store_t));
    }

    for (int i = 0; i < ALARM_MAX_COUNT; i++) {
        alarm_create_one(i);
    }
}

void alarm_clock_stop_ringing(void)
{
    g_ringing = RT_FALSE;
    // alarm_play_stop();
}

rt_bool_t alarm_clock_is_ringing(void)
{
    return g_ringing;
}

const alarm_entry_t *alarm_clock_get_entry(int index)
{
    if (index < 0 || index >= ALARM_MAX_COUNT) return RT_NULL;
    return &g_store.alarms[index];
}

void alarm_clock_snooze(void)
{
    // alarm_play_stop();

    if (g_snooze_alarm) {
        rt_alarm_delete(g_snooze_alarm);
        g_snooze_alarm = RT_NULL;
    }

    /* alarm framework uses gmtime_r, so use UTC time for ONESHOT */
    time_t now = time(RT_NULL) + ALARM_SNOOZE_MIN * 60;
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);

    struct rt_alarm_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.flag = RT_ALARM_ONESHOT;
    setup.wktime = tm_utc;

    g_snooze_alarm = rt_alarm_create(alarm_trigger_cb, &setup);
    if (g_snooze_alarm) {
        rt_alarm_start(g_snooze_alarm);
        g_snoozing = RT_TRUE;
        g_ringing = RT_FALSE;
        struct tm tm_local;
        localtime_r(&now, &tm_local);
        ALARM_LOG("snooze set for %02d:%02d (local)", tm_local.tm_hour, tm_local.tm_min);
    }
}

static rt_device_t g_rtc_dev = RT_NULL;

static void alarm_ticker_entry(void *param)
{
    (void)param;

    while (1) {
        rt_thread_mdelay(1000);
        if (g_rtc_dev == RT_NULL) {
            g_rtc_dev = rt_device_find("rtc");
        }
        if (g_rtc_dev) {
            rt_alarm_update(g_rtc_dev, 1);
        }
    }
}

int alarm_clock_init(void)
{
    memset(g_alarm_handles, 0, sizeof(g_alarm_handles));
    memset(g_weekly_handles, 0, sizeof(g_weekly_handles));
    memset(&g_store, 0, sizeof(g_store));

    alarm_clock_load(&g_store);
    alarm_clock_apply(RT_NULL);

    rt_thread_t ticker = rt_thread_create("alm_tick", alarm_ticker_entry, RT_NULL,
                                           1024, 12, 10);
    if (ticker) {
        rt_thread_startup(ticker);
    }

    ALARM_LOG("alarm clock initialized");
    return 0;
}
