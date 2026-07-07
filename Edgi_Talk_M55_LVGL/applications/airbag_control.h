#ifndef AIRBAG_CONTROL_H
#define AIRBAG_CONTROL_H

#include <stdint.h>

#define NUM_AIRBAGS 3

enum { BAG_L = 0, BAG_M = 1, BAG_R = 2 };

enum feedback_state {
    ST_IDLE            = 0x00,
    ST_COMFORT_INFLATE = 0x01,
    ST_C_INFLATE_DONE  = 0x02,
    ST_COMFORT         = 0x03,
    ST_INFLATE_SUPINE  = 0x04,
    ST_INFLATE_SUP_D   = 0x05,
    ST_INFLATE_LLAT    = 0x06,
    ST_INFLATE_LL_D    = 0x07,
    ST_INFLATE_RLAT    = 0x08,
    ST_INFLATE_RL_D    = 0x09,
    ST_HOLDING         = 0x0A,
    ST_HOLDING_DONE    = 0x0B,
    ST_DEFLATING       = 0x0C,
    ST_DEFLATING_DONE  = 0x0D,
    ST_TEST_MODE       = 0x0E,
    ST_TEST_DONE       = 0x0F,
    ST_MAN_INFLATE_L   = 0x20,
    ST_MAN_INFLATE_L_D = 0x21,
    ST_MAN_INFLATE_M   = 0x22,
    ST_MAN_INFLATE_M_D = 0x23,
    ST_MAN_INFLATE_R   = 0x24,
    ST_MAN_INFLATE_R_D = 0x25,
    ST_MAN_DEFLATE_L   = 0x26,
    ST_MAN_DEFLATE_L_D = 0x27,
    ST_MAN_DEFLATE_M   = 0x28,
    ST_MAN_DEFLATE_M_D = 0x29,
    ST_MAN_DEFLATE_R   = 0x2A,
    ST_MAN_DEFLATE_R_D = 0x2B,
};

extern volatile uint8_t g_airbag_fb_state;
extern volatile uint8_t g_airbag_fb_secs[3];
extern volatile uint8_t g_airbag_fb_test_active;
extern volatile uint8_t g_airbag_fb_error;

const char *airbag_state_name(void);

#endif
