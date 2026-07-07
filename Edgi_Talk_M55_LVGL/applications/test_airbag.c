#include "airbag_control.h"
#include <rtthread.h>

static int g_test_running = 0;

/* 等待 STM32 反馈达到 done_state, timeout=100ms 次数 */
static int wait_done(uint8_t done_state, int timeout_100ms)
{
    while (timeout_100ms-- > 0) {
        if (g_airbag_fb_state == done_state) return 1;
        rt_thread_mdelay(100);
    }
    rt_kprintf("[TEST] TIMEOUT waiting for 0x%02X\r\n", done_state);
    return 0;
}

/* 状态序列 */
static const uint8_t g_test_seq[] = {
    ST_COMFORT_INFLATE,
    ST_COMFORT,
    ST_INFLATING_SUPINE,
    ST_INFLATING_LEFT_LAT,
    ST_INFLATING_RIGHT_LAT,
    ST_HOLDING,
    ST_DEFLATING,
    ST_IDLE,
};

/* 状态对应的 _DONE 超时 (100ms tick) */
static const int g_test_timeout[] = {
    250,    /* COMFORT_INFLATE 16s */
    50,     /* COMFORT 无 DONE, 等 5s */
    150,    /* INFLATING_SUPINE 8s */
    150,    /* INFLATING_LEFT_LAT 8s */
    150,    /* INFLATING_RIGHT_LAT 8s */
    100,    /* HOLDING 5s */
    400,    /* DEFLATING ≤24s */
    50,     /* IDLE 无 DONE, 等 5s */
};

static void test_airbag_all(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (g_test_running) {
        rt_kprintf("[TEST] Already running!\r\n");
        return;
    }
    g_test_running = 1;
    int had_error = 0;

    rt_kprintf("========== STATE MACHINE TEST START ==========\r\n");

    airbag_test_mode_set(1);

    /* 先确保在 IDLE */
    if (g_airbag_fb_state != ST_IDLE) {
        airbag_send_state(ST_DEFLATING);
        if (!wait_done(ST_DEFLATING_DONE, 400)) had_error = 1;
        airbag_send_state(ST_IDLE);
        rt_thread_mdelay(500);
    }

    for (int i = 0; i < (int)(sizeof(g_test_seq) / sizeof(g_test_seq[0])); i++) {
        uint8_t s = g_test_seq[i];
        airbag_send_state(s);
        rt_kprintf("[TEST] step %d: SET_STATE(0x%02X)\r\n", i + 1, s);

        if (ST_HAS_DONE(s)) {
            uint8_t done = s + 1;
            if (!wait_done(done, g_test_timeout[i])) {
                had_error = 1;
                break;
            }
            rt_kprintf("[TEST] step %d: DONE\r\n", i + 1);
        } else {
            int timeout = g_test_timeout[i];
            while (timeout-- > 0 && g_airbag_fb_state != s)
                rt_thread_mdelay(100);
            if (g_airbag_fb_state != s) {
                rt_kprintf("[TEST] step %d: FAIL (state=0x%02X)\r\n", i + 1, g_airbag_fb_state);
                had_error = 1;
                break;
            }
        }
    }

    if (!had_error && g_airbag_fb_state != ST_IDLE) {
        airbag_send_state(ST_IDLE);
        rt_thread_mdelay(500);
    }

    if (had_error) {
        rt_kprintf("========== TEST FAILED (state=0x%02X) ==========\r\n", g_airbag_fb_state);
    } else {
        rt_kprintf("========== ALL TESTS PASSED ==========\r\n");
    }

    airbag_test_mode_set(0);
    g_test_running = 0;
}

MSH_CMD_EXPORT(test_airbag_all, "State machine test: full sequence via SET_STATE");

static void test_airbag_thread_entry(void *param)
{
    (void)param;
    test_airbag_all(0, RT_NULL);
}

void airbag_test_run(void)
{
    if (g_test_running) {
        rt_kprintf("[TEST] Already running!\r\n");
        return;
    }
    rt_thread_t tid = rt_thread_create("airbag_test", test_airbag_thread_entry,
                                       RT_NULL, 2048, 8, 10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
    else
        rt_kprintf("[TEST] Failed to create thread\r\n");
}
