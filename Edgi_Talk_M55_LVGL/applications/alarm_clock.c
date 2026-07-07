#include "alarm_clock.h"
#include <string.h>
#include <stdio.h>

static Alarm alarms[MAX_ALARMS];
static int alarm_count = 0;

void alarm_init(void)
{
    alarm_count = 0;
    memset(alarms, 0, sizeof(alarms));
}

int alarm_add(const char *time_str, uint8_t hour, uint8_t min, uint8_t enabled)
{
    if (alarm_count >= MAX_ALARMS) return -1;
    Alarm *a = &alarms[alarm_count++];
    strncpy(a->time_str, time_str, sizeof(a->time_str) - 1);
    a->hour = hour;
    a->min  = min;
    a->enabled = enabled;
    a->triggered = 0;
    return alarm_count - 1;
}

int alarm_check(uint8_t hour, uint8_t min)
{
    for (int i = 0; i < alarm_count; i++) {
        if (alarms[i].enabled && alarms[i].hour == hour && alarms[i].min == min && !alarms[i].triggered) {
            alarms[i].triggered = 1;
            return i;
        }
    }
    return -1;
}

void alarm_clear_triggered(int idx)
{
    if (idx >= 0 && idx < alarm_count)
        alarms[idx].triggered = 0;
}

int alarm_get_count(void) { return alarm_count; }

const Alarm *alarm_get(int idx)
{
    if (idx < 0 || idx >= alarm_count) return NULL;
    return &alarms[idx];
}
