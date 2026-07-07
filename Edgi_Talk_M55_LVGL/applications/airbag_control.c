#include "airbag_control.h"
#include <string.h>

/* ========== STM32 反馈数据 (由 main.c UART5 解析更新) ========== */
volatile uint8_t g_airbag_fb_state       = ST_IDLE;
volatile uint8_t g_airbag_fb_secs[3]     = {0, 0, 0};
volatile uint8_t g_airbag_fb_test_active = 0;
volatile uint8_t g_airbag_fb_error       = 0;

/* ========== M55 内部决策状态 ========== */
static uint8_t      g_waiting_for_done    = 0;
static uint8_t      g_pending_state       = 0xFF;
static uint8_t      g_pending_manual      = 0xFF;   /* 0xFF=无, 否则为待发手动操作 */
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

uint8_t airbag_get_sec(int idx)
{
    if (idx < 0 || idx > 2) return 0;
    return g_airbag_fb_secs[idx];
}

void airbag_update_feedback(uint8_t st, uint8_t l, uint8_t m, uint8_t r, uint8_t flags)
{
    g_airbag_fb_state     = st;
    g_airbag_fb_secs[0]   = l;
    g_airbag_fb_secs[1]   = m;
    g_airbag_fb_secs[2]   = r;
    g_airbag_fb_test_active = (flags & 0x01) ? 1 : 0;
    g_airbag_fb_error       = (flags & 0x02) ? 1 : 0;
}

/* ========== UART5 命令发送 ========== */
static rt_device_t g_uart5_dev = RT_NULL;

static void send_set_state(uint8_t state)
{
    if (g_uart5_dev == RT_NULL) {
        g_uart5_dev = rt_device_find("uart5");
        if (g_uart5_dev == RT_NULL) return;
    }
    uint8_t buf[4] = {0xA5, state, 0x00, 0x5A};
    rt_device_write(g_uart5_dev, 0, buf, sizeof(buf));

    if (ST_HAS_DONE(state)) {
        g_waiting_for_done = 1;
        g_pending_state = state;
    } else {
        g_waiting_for_done = 0;
        g_pending_state = 0xFF;
    }
}

/* ========== 辅助函数 ========== */
static int is_lateral(int p)
{
    return (p == POSTURE_LEFT_LATERAL || p == POSTURE_RIGHT_LATERAL);
}

static int is_intervenable(int p)
{
    return (p == POSTURE_SUPINE);
}

static uint8_t snore_pattern_for_posture(int posture)
{
    switch (posture) {
    case POSTURE_SUPINE:        return ST_INFLATING_SUPINE;
    case POSTURE_LEFT_LATERAL:  return ST_INFLATING_LEFT_LAT;
    case POSTURE_RIGHT_LATERAL: return ST_INFLATING_RIGHT_LAT;
    default:                    return 0xFF;
    }
}

/* ========== 决策循环 ========== */

void airbag_process(int current_posture, int snore_detected)
{
    uint8_t fb = g_airbag_fb_state;

    /* 1. 如果等 _DONE, 检查是否收到了 */
    if (g_waiting_for_done) {
        if (ST_IS_DONE(fb)) {
            g_waiting_for_done = 0;
            /* 1a. 收到 _DONE 后, 优先发 pending manual */
            if (g_pending_manual != 0xFF) {
                send_set_state(g_pending_manual);
                g_pending_manual = 0xFF;
                return;
            }
        } else {
            return;
        }
    }

    /* 2. 即使没在等, 有 pending manual 也立即发 (在 IDLE/COMFORT 时) */
    if (g_pending_manual != 0xFF) {
        send_set_state(g_pending_manual);
        g_pending_manual = 0xFF;
        return;
    }

    switch (fb) {

    case ST_IDLE: {
        if (current_posture >= 0) {
            g_current_steady_post = current_posture;
            if (is_lateral(current_posture)) {
                send_set_state(ST_COMFORT_INFLATE);
                break;
            }
            if (snore_detected && is_intervenable(current_posture)) {
                uint8_t s = snore_pattern_for_posture(current_posture);
                if (s != 0xFF) send_set_state(s);
            }
        }
        break;
    }

    case ST_COMFORT_INFLATE_DONE: {
        /* 充气完成后先进入 HOLDING，在 HOLDING_DONE 时重新判断睡姿 */
        g_comfort_holding = 1;
        send_set_state(ST_HOLDING);
        break;
    }

    case ST_COMFORT: {
        if (current_posture >= 0 && current_posture != g_current_steady_post) {
            g_current_steady_post = current_posture;
            if (!is_lateral(current_posture)) {
                send_set_state(ST_DEFLATING);
                break;
            }
        }
        if (snore_detected) {
            uint8_t s = snore_pattern_for_posture(g_current_steady_post);
            if (s != 0xFF) {
                send_set_state(ST_DEFLATING);
                g_pending_state = s;
            }
        }
        break;
    }

    case ST_INFLATING_SUPINE_D:
    case ST_INFLATING_LEFT_LD:
    case ST_INFLATING_RIGHT_LD: {
        send_set_state(ST_HOLDING);
        break;
    }

    case ST_HOLDING_DONE: {
        if (g_comfort_holding) {
            g_comfort_holding = 0;
            if (current_posture >= 0)
                g_current_steady_post = current_posture;
            if (current_posture >= 0 && is_lateral(current_posture))
                send_set_state(ST_COMFORT);
            else
                send_set_state(ST_DEFLATING);
        } else {
            send_set_state(snore_detected ? ST_HOLDING : ST_DEFLATING);
        }
        break;
    }

    case ST_DEFLATING_DONE: {
        if (g_manual_deflate_all) {
            g_manual_deflate_all = 0;
            send_set_state(ST_IDLE);
        } else if (is_lateral(g_current_steady_post)) {
            send_set_state(ST_COMFORT_INFLATE);
        } else if (snore_detected && is_intervenable(g_current_steady_post)) {
            uint8_t s = snore_pattern_for_posture(g_current_steady_post);
            if (s != 0xFF) send_set_state(s);
        } else {
            send_set_state(ST_IDLE);
        }
        break;
    }

    case ST_TEST_MODE_DONE: {
        g_test_mode = 0;
        send_set_state(ST_IDLE);
        break;
    }

    case ST_MANUAL_INFLATE_LD:
    case ST_MANUAL_INFLATE_MD:
    case ST_MANUAL_INFLATE_RD:
        /* 手动充气完成, 不做自动跳转, 等待下一次按钮 */
        break;

    case ST_MANUAL_DEFLATE_LD:
    case ST_MANUAL_DEFLATE_MD:
    case ST_MANUAL_DEFLATE_RD:
        /* 手动放气完成: 如果三个气囊都是0则自动回到IDLE */
        if (g_airbag_fb_secs[0] == 0 && g_airbag_fb_secs[1] == 0 && g_airbag_fb_secs[2] == 0) {
            send_set_state(ST_IDLE);
        }
        break;

    default:
        break;
    }
}

/* ========== 初始化 ========== */

void airbag_system_init(void)
{
    g_uart5_dev = rt_device_find("uart5");
    send_set_state(ST_IDLE);
}

/* ========== 测试模式 ========== */

void airbag_test_mode_set(int enable)
{
    g_test_mode = enable ? 1 : 0;
    send_set_state(g_test_mode ? ST_TEST_MODE : ST_IDLE);
}

int airbag_test_mode_is_active(void)
{
    return g_test_mode;
}

/* ========== 手动控制 (UI 按钮调用) ========== */

void airbag_manual_inflate(uint8_t bag)
{
    if (bag > 2) return;
    g_pending_manual = ST_MANUAL_INFLATE_L + bag * 2;
}

void airbag_manual_deflate(uint8_t bag)
{
    if (bag > 2) return;
    g_pending_manual = ST_MANUAL_DEFLATE_L + bag * 2;
}

/* 查询: 是否有手动操作待发或正在执行 (供 UI 按钮高亮) */
int airbag_manual_is_active(void)
{
    if (g_pending_manual != 0xFF) return 1;
    if (g_waiting_for_done) {
        uint8_t s = g_airbag_fb_state;
        if (s >= ST_MANUAL_INFLATE_L && s <= ST_MANUAL_DEFLATE_R) return 1;
    }
    return 0;
}

void airbag_send_state(uint8_t state)
{
    send_set_state(state);
}

void airbag_inflate_all(void)
{
    airbag_send_state(ST_COMFORT_INFLATE);
}

void airbag_deflate_all(void)
{
    g_manual_deflate_all = 1;
    airbag_send_state(ST_DEFLATING);
}
