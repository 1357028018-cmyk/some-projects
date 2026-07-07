#include "airbag_control.h"
#include <string.h>

volatile uint8_t g_airbag_fb_state       = ST_IDLE;
volatile uint8_t g_airbag_fb_secs[3]     = {0, 0, 0};
volatile uint8_t g_airbag_fb_test_active = 0;
volatile uint8_t g_airbag_fb_error       = 0;

static uint8_t      g_waiting_for_done    = 0;
static uint8_t      g_pending_state       = 0xFF;
static uint8_t      g_pending_manual      = 0xFF;
static int          g_current_steady_post = -1;
static int          g_prev_fb_state       = -1;
static int          g_test_mode           = 0;
static int          g_manual_deflate_all  = 0;
static int          g_comfort_holding     = 0;

static const char *state_names[] = {
    [0x00] = "IDLE",
    [0x01] = "COMFORT_INFLATE",
    [0x02] = "C_INFLATE_DONE",
    [0x03] = "COMFORT",
    [0x04] = "INFLATE_SUPINE",
    [0x05] = "INFLATE_SUP_D",
    [0x06] = "INFLATE_LLAT",
    [0x07] = "INFLATE_LL_D",
    [0x08] = "INFLATE_RLAT",
    [0x09] = "INFLATE_RL_D",
    [0x0A] = "HOLDING",
    [0x0B] = "HOLDING_DONE",
    [0x0C] = "DEFLATING",
    [0x0D] = "DEFLATING_DONE",
    [0x0E] = "TEST_MODE",
    [0x0F] = "TEST_DONE",
    [0x20] = "MAN_INFLATE_L",
    [0x21] = "MAN_INFLATE_L_D",
    [0x22] = "MAN_INFLATE_M",
    [0x23] = "MAN_INFLATE_M_D",
    [0x24] = "MAN_INFLATE_R",
    [0x25] = "MAN_INFLATE_R_D",
    [0x26] = "MAN_DEFLATE_L",
    [0x27] = "MAN_DEFLATE_L_D",
    [0x28] = "MAN_DEFLATE_M",
    [0x29] = "MAN_DEFLATE_M_D",
    [0x2A] = "MAN_DEFLATE_R",
    [0x2B] = "MAN_DEFLATE_R_D",
};

const char *airbag_state_name(void)
{
    int n = sizeof(state_names) / sizeof(state_names[0]);
    if (g_airbag_fb_state < n && state_names[g_airbag_fb_state] != NULL)
        return state_names[g_airbag_fb_state];
    return "UNKNOWN";
}