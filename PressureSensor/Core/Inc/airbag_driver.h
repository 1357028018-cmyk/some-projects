#ifndef __AIRBAG_DRIVER_H__
#define __AIRBAG_DRIVER_H__

#include "main.h"
#include <string.h>

#define CMD_FRAME_HEADER    0xA5
#define CMD_FRAME_FOOTER    0x5A
#define CMD_FRAME_LEN       4

#define FB_STATE            262
#define FB_LEFT_SEC         263
#define FB_MID_SEC          264
#define FB_RIGHT_SEC        265
#define FB_FLAGS            266
#define FB_FLAG_TEST        0x01
#define FB_FLAG_ERROR       0x02

#define ST_IDLE                 0x00
#define ST_COMFORT_INFLATE      0x01
#define ST_COMFORT_INFLATE_DONE 0x02
#define ST_COMFORT              0x03
#define ST_INFLATING_SUPINE     0x04
#define ST_INFLATING_SUPINE_D   0x05
#define ST_INFLATING_LEFT_LAT   0x06
#define ST_INFLATING_LEFT_LD    0x07
#define ST_INFLATING_RIGHT_LAT  0x08
#define ST_INFLATING_RIGHT_LD   0x09
#define ST_HOLDING              0x0A
#define ST_HOLDING_DONE         0x0B
#define ST_DEFLATING            0x0C
#define ST_DEFLATING_DONE       0x0D
#define ST_TEST_MODE            0x0E
#define ST_TEST_MODE_DONE       0x0F
#define ST_MANUAL_INFLATE_L     0x20
#define ST_MANUAL_INFLATE_LD    0x21
#define ST_MANUAL_INFLATE_M     0x22
#define ST_MANUAL_INFLATE_MD    0x23
#define ST_MANUAL_INFLATE_R     0x24
#define ST_MANUAL_INFLATE_RD    0x25
#define ST_MANUAL_DEFLATE_L     0x26
#define ST_MANUAL_DEFLATE_LD    0x27
#define ST_MANUAL_DEFLATE_M     0x28
#define ST_MANUAL_DEFLATE_MD    0x29
#define ST_MANUAL_DEFLATE_R     0x2A
#define ST_MANUAL_DEFLATE_RD    0x2B

#define ST_HAS_DONE(s)  ((s) == ST_COMFORT_INFLATE || \
                         (s) == ST_INFLATING_SUPINE || \
                         (s) == ST_INFLATING_LEFT_LAT || \
                         (s) == ST_INFLATING_RIGHT_LAT || \
                         (s) == ST_HOLDING || (s) == ST_DEFLATING || (s) == ST_TEST_MODE || \
                         ((s) >= ST_MANUAL_INFLATE_L && (s) <= ST_MANUAL_DEFLATE_R))

#define ST_IS_DONE(s)   ((s) == ST_COMFORT_INFLATE_DONE || \
                         (s) == ST_INFLATING_SUPINE_D || \
                         (s) == ST_INFLATING_LEFT_LD || \
                         (s) == ST_INFLATING_RIGHT_LD || \
                         (s) == ST_HOLDING_DONE || (s) == ST_DEFLATING_DONE || (s) == ST_TEST_MODE_DONE || \
                         ((s) >= ST_MANUAL_INFLATE_LD && (s) <= ST_MANUAL_DEFLATE_RD))

#define INFLATE_TIME_HIGH_MS    12000
#define INFLATE_TIME_LOW_MS     12000
#define INFLATE_TIME_NONE_MS    0
#define HOLD_TIME_MS            5000
#define MIN_DEFLATE_MS          3000
#define BOOT_DEFLATE_MS         5000
#define PUMP_MAX_RUN_MS         15000
#define MAX_CONCURRENT_PUMPS    2

#define AIRBAG_LEFT     0
#define AIRBAG_MID      1
#define AIRBAG_RIGHT    2
#define AIRBAG_COUNT    3

#define PUMP_LEFT       0
#define PUMP_MID        1
#define PUMP_RIGHT      2
#define PUMP_SUCTION    3
#define PUMP_COUNT      4

#define VALVE_LEFT      0
#define VALVE_MID       1
#define VALVE_RIGHT     2
#define VALVE_COUNT     3

typedef struct {
    int time_ms[AIRBAG_COUNT];
} inflate_pattern_t;

#define AIRBAG_NVM_ADDR         0x0807F800
#define AIRBAG_NVM_MAGIC        0xA1B2C3D4

typedef struct {
    uint32_t magic;
    uint32_t state_id;
    uint32_t inflate_ms[AIRBAG_COUNT];
    uint32_t checksum;
} airbag_nvm_t;

extern volatile uint8_t g_airbag_current_state;
extern volatile uint8_t g_airbag_target_state;
extern volatile uint8_t g_airbag_fb_state;
extern int g_inflate_elapsed[3];
extern volatile uint8_t g_airbag_fb_left_sec;
extern volatile uint8_t g_airbag_fb_mid_sec;
extern volatile uint8_t g_airbag_fb_right_sec;
extern volatile uint8_t g_airbag_fb_flags;
extern TIM_HandleTypeDef htim4_pwm;

void AirbagDriver_Init(void);
void Airbag_ParseCmdByte(uint8_t ch);
void Airbag_CheckTimers(void);
void Airbag_UpdateFeedback(void);
void Airbag_PWM_ISR(void);

void Airbag_PumpOn(uint8_t id);
void Airbag_PumpOff(uint8_t id);
void Airbag_ValveSet(uint8_t bag, uint8_t mode);
void Airbag_AllValvesInflate(void);
void Airbag_AllOff(void);

void Airbag_NVM_Save(void);
uint8_t Airbag_NVM_Load(airbag_nvm_t *nvm);
void Airbag_NVM_Clear(void);

#endif
