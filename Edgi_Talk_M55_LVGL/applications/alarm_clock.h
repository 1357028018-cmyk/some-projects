#ifndef __ALARM_CLOCK_H__
#define __ALARM_CLOCK_H__

#include <rtthread.h>
#include <drivers/alarm.h>

#define ALARM_MAX_COUNT         3
#define ALARM_FILE_PATH         "/flash/alarms.dat"
#define ALARM_SNOOZE_MIN        5

#define ALARM_REPEAT_SUN        0x01
#define ALARM_REPEAT_MON        0x02
#define ALARM_REPEAT_TUE        0x04
#define ALARM_REPEAT_WED        0x08
#define ALARM_REPEAT_THU        0x10
#define ALARM_REPEAT_FRI        0x20
#define ALARM_REPEAT_SAT        0x40
#define ALARM_REPEAT_EVERYDAY   0x7F
#define ALARM_REPEAT_WEEKDAY    (ALARM_REPEAT_MON|ALARM_REPEAT_TUE|ALARM_REPEAT_WED|ALARM_REPEAT_THU|ALARM_REPEAT_FRI)
#define ALARM_REPEAT_WEEKEND    (ALARM_REPEAT_SUN|ALARM_REPEAT_SAT)

typedef struct {
    rt_uint8_t enabled;
    rt_uint8_t hour;
    rt_uint8_t minute;
    rt_uint8_t repeat;
} alarm_entry_t;

typedef struct {
    alarm_entry_t alarms[ALARM_MAX_COUNT];
} alarm_store_t;

int  alarm_clock_init(void);
int  alarm_clock_load(alarm_store_t *store);
int  alarm_clock_save(const alarm_store_t *store);
void alarm_clock_apply(const alarm_store_t *store);
void alarm_clock_stop_ringing(void);
void alarm_clock_snooze(void);
rt_bool_t alarm_clock_is_ringing(void);
const alarm_entry_t *alarm_clock_get_entry(int index);

#endif
