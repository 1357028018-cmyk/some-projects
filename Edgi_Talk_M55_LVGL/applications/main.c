#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <lv_rt_thread_conf.h>
#include "vg_lite.h"
#include "vg_lite_platform.h"
#include "lv_port_disp.h"
#include "ui_main.h"
#include "model/model.h"
#include "wifi_tcp_sender.h"
#include "cy_pdl.h"
#include "cycfg_peripherals.h"
#include "drv_ipc.h"

/* WiFi 相关头文件 */
#include <wlan_mgnt.h>
#include <lwip/inet.h>
#include <netdev.h>

/* WiFi GUI 管理 */
#include "wifi_gui.h"

/* 气囊控制系统 */
#include "airbag_control.h"

/* 闹钟功能 */
#include "alarm_clock.h"

/* string 操作 (rt_memcpy) */
#include <string.h>

#define LED_PIN_G               GET_PIN(16, 6)

/* ========== UART 透传相关宏定义 ========== */
#define UART5_DEV_NAME             "uart5"
#define UART2_DEV_NAME             "uart2"
#define UART_RECV_BUF_SIZE         1024
#define UART_BAUDRATE              115200

#define RELAY_STACK_SIZE           2048
#define RELAY_PRIORITY             5
#define RELAY_TIMESLICE            10

/* ============================================================================
 * UART5 接收帧格式（STM32 压力传感器板经 USART1 透传过来）
 *
 *   [0] 压力数据帧（268 字节，固定长度，5Hz）
 *       Byte[0]      : 0xAA (帧头)
 *       Byte[1]      : 帧序号 (0x00-0xFF 循环)
 *       Byte[2-3]    : 数据长度 (0x0100 = 256, 大端)
 *       Byte[4-259]  : 256 字节 16x16 矩阵压力值 (每字节 0~255)
 *       Byte[260]    : CRC8 (Byte[1]~Byte[259] 异或)
 *       Byte[261]    : 0x55 (帧尾)
 *       Byte[262-267]: 6 字节全 0 填充
 *
 *   [1] 0xBB TLV 雷达合并帧（27 字节，每 3s 一次）
 *       Byte[0]      : 0xBB (帧头)
 *       Byte[1]      : 预留
 *       Byte[2]      : TLV 条目总长度
 *       Byte[3~]     : {Type(1B) + Length(1B) + Value(NB)} × N
 *       Byte[N-1]    : CRC8 (XOR)
 *       Byte[N]      : 0x55 (帧尾)
 *
 *       0x01:心率 0x02:呼吸率 0x03:体动max 0x04:体动avg
 *       0x05:距离最新 0x06:距离min 0x07:距离max
 *       0x08:存在标志 0x09:参数1 0x0A:参数2
 *
 *   [2] R60ABD1 原始雷达帧（向后兼容，直连雷达时使用）
 *       Byte[0-1]   : 0x53 0x59 (帧头)
 * ============================================================================ */

#define FRAME_HEADER_SIZE          4
#define FRAME_ADC_DATA_LEN         256
#define FRAME_ADC_START            4
#define FRAME_ADC_END              (FRAME_ADC_START + FRAME_ADC_DATA_LEN)
#define FRAME_CRC_POS              260
#define FRAME_TAIL_POS             261
#define FRAME_TOTAL                268

#define DEC_COLS                   16

/* R60ABD1 雷达帧常量 */
#define R60_HDR0                   0x53
#define R60_HDR1                   0x59
#define R60_TAIL0                  0x54
#define R60_TAIL1                  0x43
#define R60_MAX_DATA_LEN           64
#define R60_HEADER_LEN             6                           /* 帧头+控制+命令+长度 */
#define R60_TAIL_LEN               3                           /* 校验+帧尾 */
#define R60_MAX_FRAME              (R60_HEADER_LEN + R60_MAX_DATA_LEN + R60_TAIL_LEN)  /* 73 */

/* ========== 全局变量 ========== */
static rt_device_t uart5_dev = RT_NULL;
static rt_device_t uart2_dev = RT_NULL;

static volatile rt_uint32_t rx_isr_call_count = 0;
static volatile rt_uint32_t total_fwd_bytes  = 0;
static volatile rt_uint32_t poll_cnt         = 0;
static volatile rt_uint32_t frame_cnt        = 0;
static volatile rt_uint32_t split_frame_cnt   = 0;

/* ========== 压力数据全局缓冲区（供LVGL读取）========== */
static volatile uint8_t g_pressure_data[FRAME_ADC_DATA_LEN];
static uint8_t g_pressure_filtered[FRAME_ADC_DATA_LEN];
static volatile uint8_t g_data_ready = 0;

/* ========== 0xBB TLV 合并雷达数据全局缓冲区 ========== */
typedef struct {
    int8_t   hr;          uint8_t  hr_valid;
    int8_t   br;          uint8_t  br_valid;
    uint8_t  motion_max;  uint8_t  motion_max_valid;
    uint8_t  motion_avg;  uint8_t  motion_avg_valid;
    uint16_t dist_latest; uint8_t  dist_l_valid;
    uint16_t dist_min;    uint8_t  dist_min_valid;
    uint16_t dist_max;    uint8_t  dist_max_valid;
    uint8_t  exist;       uint8_t  exist_valid;
    uint8_t  param1;      uint8_t  p1_valid;
    uint8_t  param2;      uint8_t  p2_valid;

    uint8_t  valid;       /* 新帧标志 */
    uint32_t tick;        /* 收到时间戳 */
} radar_combined_t;
static volatile radar_combined_t g_radar = {0};

/* 闭运算滤波 */
static void close_filter_3x3(const volatile uint8_t *src, uint8_t *dst, int w, int h)
{
    static uint8_t tmp[FRAME_ADC_DATA_LEN];
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t max_val = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy, nx = x + dx;
                    if (ny >= 0 && ny < h && nx >= 0 && nx < w) {
                        uint8_t v = src[ny * w + nx];
                        if (v > max_val) max_val = v;
                    }
                }
            tmp[y * w + x] = max_val;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t min_val = 255;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy, nx = x + dx;
                    if (ny >= 0 && ny < h && nx >= 0 && nx < w) {
                        uint8_t v = tmp[ny * w + nx];
                        if (v < min_val) min_val = v;
                    }
                }
            dst[y * w + x] = min_val;
        }
    }
}

/* ========== 帧边界提取缓冲区 ========== */
#define RAW_BUF_SIZE               1024
static rt_uint8_t raw_buf[RAW_BUF_SIZE];
static int         raw_len    = 0;

/* ========== 诊断计数器 ========== */
static volatile rt_uint32_t uart_poll_cnt         = 0;
static volatile rt_uint32_t uart_frame_cnt        = 0;
static volatile rt_uint32_t uart_split_frame_cnt  = 0;

/* ========== AI 推理相关 ========== */
static float g_ai_output[IMPRESSURE_DATA_OUT_COUNT];
static volatile rt_uint32_t ai_inference_cnt = 0;
static const char *posture_labels[] = {
    "unlabeled", "supine", "left_lateral", "right_lateral", "noise"
};

/* ========== 10帧批处理多数投票逻辑 ========== */
#define POSTURE_BATCH_SIZE  10
static struct {
    int idx;
    int conf;
} g_posture_batch[POSTURE_BATCH_SIZE];
static int g_posture_batch_pos = 0;
static int g_majority_posture = -1;
static int g_majority_votes  = 0;
static int g_last_batch_posture   = -1;
static int g_posture_batch_debounce = 0;
static int g_posture_confirmed    = -1;
#define POSTURE_BATCH_CONFIRM 3

int posture_get_majority_index(void)
{
    return g_majority_posture;
}

int posture_get_confirmed(void)
{
    return g_posture_confirmed;
}

static void posture_vote_update(int idx, int conf)
{
    g_posture_batch[g_posture_batch_pos].idx  = idx;
    g_posture_batch[g_posture_batch_pos].conf = conf;
    g_posture_batch_pos++;
    if (g_posture_batch_pos < POSTURE_BATCH_SIZE)
        return;
    int votes[IMPRESSURE_DATA_OUT_COUNT] = {0};
    int conf_sums[IMPRESSURE_DATA_OUT_COUNT] = {0};
    for (int i = 0; i < POSTURE_BATCH_SIZE; i++) {
        int pi = g_posture_batch[i].idx;
        votes[pi]++;
        conf_sums[pi] += g_posture_batch[i].conf;
    }
    int max_votes = 0;
    int candidates[IMPRESSURE_DATA_OUT_COUNT];
    int cand_count = 0;
    for (int i = 0; i < IMPRESSURE_DATA_OUT_COUNT; i++) {
        if (votes[i] > max_votes) {
            max_votes = votes[i];
            cand_count = 0;
            candidates[cand_count++] = i;
        } else if (votes[i] == max_votes && max_votes > 0) {
            candidates[cand_count++] = i;
        }
    }
    int best = candidates[0];
    for (int i = 1; i < cand_count; i++) {
        if (conf_sums[candidates[i]] > conf_sums[best])
            best = candidates[i];
    }
    g_majority_posture = best;
    g_majority_votes  = votes[best];
    g_posture_batch_pos = 0;

    if (best == g_last_batch_posture) {
        if (g_posture_batch_debounce < POSTURE_BATCH_CONFIRM - 1)
            g_posture_batch_debounce++;
        else
            g_posture_confirmed = best;
    } else {
        g_posture_batch_debounce = 0;
        g_last_batch_posture = best;
    }
}

const char *posture_get_majority_label(void)
{
    static char buf[64];
    if (g_majority_posture < 0)
        return "collecting...";
    rt_snprintf(buf, sizeof(buf), "%s (%d/%d)",
                posture_labels[g_majority_posture],
                g_majority_votes, POSTURE_BATCH_SIZE);
    return buf;
}

/* ========== 温湿度数据全局缓冲区 ========== */
typedef struct {
    float temperature;
    float humidity;
    rt_uint8_t valid;
} sensor_env_data_t;

static sensor_env_data_t g_sensor_data = {0};
static rt_mutex_t g_sensor_mutex = RT_NULL;

/* M33 鼾声检测结果（来自 IPC） — 10帧投票：每 10 帧一组，≥6 票（单票分数≥80）判为鼾声 */
static volatile int g_snore_detected = 0;
static volatile float g_snore_confidence = 0.0f;
static volatile rt_tick_t g_snore_last_tick = 0;
#define SNORE_VOTE_BATCH_SIZE       10    /* 投票窗口大小 */
#define SNORE_VOTE_THRESHOLD         2    /* 通过阈值：≥2/10（最近 10 帧中 ≥2 帧分数≥90 判为鼾声） */
#define SNORE_VOTE_SCORE_THRESHOLD  90    /* 单帧分数阈值：channel[1]>=90 视为阳性 */
#define SNORE_IPC_TIMEOUT_MS       5000   /* M33 静默超时：5 秒强制清零 */
static int g_snore_vote_score_buffer[SNORE_VOTE_BATCH_SIZE] = {0};  /* 环形缓冲：每帧分数（0 或 channel[1]） */
static int g_snore_vote_index = 0;                                    /* 当前写入位置 0..9 */
static int g_snore_vote_positive_count = 0;                           /* 当前窗口内阳性票数 */
static int g_snore_vote_positive_score_sum = 0;                       /* 当前窗口内阳性分数总和 */
static int g_snore_total_frame_count = 0;                              /* 累计收到的 IPC 帧数（滑窗用） */

/* M33 通信健康检测 */
static volatile rt_tick_t g_m33_last_tick = 0;
static volatile rt_uint32_t g_m33_rx_count = 0;

rt_err_t sensor_get_data(float *temperature, float *humidity)
{
    if (g_sensor_mutex == RT_NULL || temperature == RT_NULL || humidity == RT_NULL)
        return -RT_ERROR;
    if (rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER) != RT_EOK)
        return -RT_ERROR;
    if (!g_sensor_data.valid) {
        rt_mutex_release(g_sensor_mutex);
        return -RT_ERROR;
    }
    *temperature = g_sensor_data.temperature;
    *humidity = g_sensor_data.humidity;
    rt_mutex_release(g_sensor_mutex);
    return RT_EOK;
}

rt_uint8_t sensor_is_valid(void) { return g_sensor_data.valid; }

/* ========== IPC 温湿度接收线程 ========== */
static void ipc_sensor_entry(void *parameter)
{
    rt_device_t ipc_dev;
    edge_rc_frame_t rx_frame;
    rt_kprintf("[IPC] thread started\n");
    ipc_dev = edge_ipc_device_find();
    if (ipc_dev == RT_NULL) {
        rt_kprintf("[IPC] device not found, registering...\n");
        if (edge_ipc_device_register() != RT_EOK) {
            rt_kprintf("[IPC] register FAILED\n");
            return;
        }
        ipc_dev = edge_ipc_device_find();
        if (ipc_dev == RT_NULL) {
            rt_kprintf("[IPC] find after register FAILED\n");
            return;
        }
    }
    rt_kprintf("[IPC] device found, opening...\n");
    if (rt_device_open(ipc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK) {
        rt_kprintf("[IPC] open FAILED\n");
        return;
    }
    // rt_kprintf("[IPC] ready, waiting for frames...\n");
    g_sensor_mutex = rt_mutex_create("sensor_mtx", RT_IPC_FLAG_FIFO);
    if (g_sensor_mutex == RT_NULL) return;
    g_m33_last_tick = rt_tick_get();
    while (1) {
        if (rt_device_read(ipc_dev, 0, &rx_frame, 1) == 1) {
            g_m33_last_tick = rt_tick_get();
            g_m33_rx_count++;
            /* 调试: 打印所有收到的 IPC 帧 */
            // rt_kprintf("[IPC-RX] magic=0x%08X role=0x%02X chk=%d seq=%lu\n",
            //            rx_frame.magic, rx_frame.role,
            //            edge_rc_checksum(&rx_frame) == rx_frame.checksum,
            //            (unsigned long)rx_frame.seq);

            if (rx_frame.magic == RC_MAGIC_WORD &&
                rx_frame.role == RC_ROLE_M33_SENSOR &&
                edge_rc_checksum(&rx_frame) == rx_frame.checksum) {
                uint32_t *data_ptr = (uint32_t *)rx_frame.channel;
                float temp = *(float *)&data_ptr[0];
                float humi = *(float *)&data_ptr[1];
                if (rt_mutex_take(g_sensor_mutex, RT_WAITING_FOREVER) == RT_EOK) {
                    g_sensor_data.temperature = temp;
                    g_sensor_data.humidity = humi;
                    g_sensor_data.valid = 1;
                    rt_mutex_release(g_sensor_mutex);
                }
                int ti = (int)temp, tf = (int)((temp >= 0 ? temp - ti : ti - temp) * 10);
                int hi = (int)humi, hf = (int)((humi - hi) * 10);
                // rt_kprintf("[IPC-SENSOR] temp=%d.%d humi=%d.%d\n", ti, tf, hi, hf);
            } else if (rx_frame.magic == RC_MAGIC_WORD &&
                       rx_frame.role == RC_ROLE_M33 &&
                       edge_rc_checksum(&rx_frame) == rx_frame.checksum) {
                /* M33 Audio: 鼾声检测结果 — 滑窗投票，每收1帧用最近10帧投票，≥6票（单票分数≥80）判鼾声 */
                int score = (int)rx_frame.channel[1];
                int has_snore = (score >= SNORE_VOTE_SCORE_THRESHOLD);
                int new_score = has_snore ? score : 0;
                int old_score = g_snore_vote_score_buffer[g_snore_vote_index];

                /* 维护环形缓冲的 running count 和 sum */
                if (old_score > 0) {
                    g_snore_vote_positive_count--;
                    g_snore_vote_positive_score_sum -= old_score;
                }
                if (new_score > 0) {
                    g_snore_vote_positive_count++;
                    g_snore_vote_positive_score_sum += new_score;
                }
                g_snore_vote_score_buffer[g_snore_vote_index] = new_score;
                g_snore_vote_index = (g_snore_vote_index + 1) % SNORE_VOTE_BATCH_SIZE;
                g_snore_total_frame_count++;

                /* D4: confidence = 当前窗口内阳性帧分数的平均值 */
                if (g_snore_vote_positive_count > 0) {
                    g_snore_confidence = (float)g_snore_vote_positive_score_sum /
                                         (float)g_snore_vote_positive_count / 100.0f;
                } else {
                    g_snore_confidence = 0.0f;
                }

                int conf_pct = (int)(g_snore_confidence * 100.0f + 0.5f);

                /* 滑窗：累计 ≥10 帧后，每帧都投票 */
                if (g_snore_total_frame_count >= SNORE_VOTE_BATCH_SIZE) {
                    if (g_snore_vote_positive_count >= SNORE_VOTE_THRESHOLD) {
                        g_snore_detected = 1;
                    } else {
                        g_snore_detected = 0;
                    }
                    if (wifi_tcp_is_connected()) {
                        char snore_buf[128];
                        int snore_len = rt_snprintf(snore_buf, sizeof(snore_buf),
                            "[SNORE] snore=%d conf=%d%%\r\n",
                            g_snore_detected, conf_pct);
                        wifi_tcp_send(snore_buf, snore_len);
                    }
                    // rt_kprintf("[SNORE-VOTE] window: pos=%d/%d -> %s, conf=%d%% (tot=%d)\r\n",
                    //            g_snore_vote_positive_count, SNORE_VOTE_BATCH_SIZE,
                    //            g_snore_detected ? "YES" : "NO",
                    //            conf_pct,
                    //            g_snore_total_frame_count);
                } else {
                    // rt_kprintf("[SNORE-IPC] vote=%d idx=%d pos=%d/%d conf=%d%% (tot=%d/%d)\r\n",
                    //            has_snore, g_snore_vote_index,
                    //            g_snore_vote_positive_count, SNORE_VOTE_BATCH_SIZE,
                    //            conf_pct,
                    //            g_snore_total_frame_count, SNORE_VOTE_BATCH_SIZE);
                }

                g_snore_last_tick = rt_tick_get();
            }
        }
        rt_thread_mdelay(10);
        /* M33 存活检测: 超过 12s 无任何 IPC 则报警 */
        if ((rt_tick_get() - g_m33_last_tick) > rt_tick_from_millisecond(12000)) {
            rt_kprintf("[M33-WATCHDOG] M33 silent >12s! last_rx=%lu total=%lu\n",
                       (unsigned long)g_m33_last_tick, (unsigned long)g_m33_rx_count);
            g_m33_last_tick = rt_tick_get();
        }
    }
}

static int ipc_sensor_auto_start(void)
{
    rt_thread_t tid = rt_thread_create("ipc_rx", ipc_sensor_entry, RT_NULL, 2048, 15, 20);
    if (tid != RT_NULL) rt_thread_startup(tid);
    return 0;
}
INIT_APP_EXPORT(ipc_sensor_auto_start);

/* ========== 数据获取接口 ========== */
uint8_t* pressure_get_data(void) { return g_pressure_filtered; }
uint8_t pressure_is_ready(void) { return g_data_ready; }
void pressure_clear_ready(void) { g_data_ready = 0; }

/* ---------- 0xBB 雷达合并帧数据获取接口 ---------- */
uint8_t radar_is_ready(void)   { return g_radar.valid; }
void   radar_clear_ready(void) { g_radar.valid = 0; }
const radar_combined_t *radar_get_data(void) { return &g_radar; }

int radar_get_heart_rate(void)
{
    return g_radar.hr_valid ? (int)g_radar.hr : -1;
}

int radar_get_breath_rate(void)
{
    return g_radar.br_valid ? (int)g_radar.br : -1;
}

int radar_get_motion_max(void)
{
    return g_radar.motion_max_valid ? (int)g_radar.motion_max : -1;
}

int radar_get_motion_avg(void)
{
    return g_radar.motion_avg_valid ? (int)g_radar.motion_avg : -1;
}

int radar_get_dist_latest(void)
{
    return g_radar.dist_l_valid ? (int)g_radar.dist_latest : -1;
}

int radar_get_exist(void)
{
    return g_radar.exist_valid ? (int)g_radar.exist : -1;
}

/* ========== 鼾声数据查询接口 ========== */
int snore_get_detected(void) { return g_snore_detected; }
float snore_get_confidence(void) { return g_snore_confidence; }

/* ========== 辅助函数 ========== */
static void uart2_only_puts(const char *s) { rt_device_write(uart2_dev, 0, (rt_uint8_t *)s, rt_strlen(s)); }

/* u2_puts: unused, kept for reference
static void u2_puts(const char *s)
{
    int slen = rt_strlen(s);
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)s, slen);
    if (wifi_tcp_is_connected()) wifi_tcp_send_str(s);
}*/

/* ========== 帧格式化打印 ========== */
static void send_pressure_tcp(const rt_uint8_t *data, rt_uint8_t seq);
static void dump_frame(const rt_uint8_t *data, int len, unsigned long fno)
{
    char line[320];
    int pos, i;
    if (len >= FRAME_HEADER_SIZE) {
        rt_uint16_t dlen = ((rt_uint16_t)data[2] << 8) | data[3];
        pos = rt_snprintf(line, sizeof(line),
            "\r\n[FRAME #%lu] len=%d | seq=0x%02X | data_len=0x%04X(%d)\r\n",
            fno, len, data[1], dlen, dlen);
    } else {
        pos = rt_snprintf(line, sizeof(line),
            "\r\n[FRAME #%lu] len=%d (SHORT FRAME!)\r\n", fno, len);
    }
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    uart2_only_puts("HDR(hex): ");
    int hdr_end = (len < FRAME_HEADER_SIZE) ? len : FRAME_HEADER_SIZE;
    for (i = 0; i < hdr_end; i++) {
        pos = rt_snprintf(line, sizeof(line), "%02X ", data[i]);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }
    uart2_only_puts("\r\n");
    int adc_start = FRAME_ADC_START;
    int adc_end   = (len < FRAME_ADC_END) ? len : FRAME_ADC_END;
    if (adc_end > adc_start) {
        int adc_count = adc_end - adc_start;
        pos = rt_snprintf(line, sizeof(line),
            "FILTERED (16x16, %d vals):\r\n", adc_count);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
        for (i = 0; i < adc_count; i++) {
            if (i % DEC_COLS == 0) {
                pos = rt_snprintf(line, sizeof(line), "  R%02d: ", i / DEC_COLS);
                rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
            }
            pos = rt_snprintf(line, sizeof(line), "%4d ", g_pressure_filtered[i]);
            rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
            if ((i + 1) % DEC_COLS == 0) uart2_only_puts("\r\n");
        }
    }
    if (len > FRAME_CRC_POS) {
        uart2_only_puts("TAIL(hex): ");
        for (i = FRAME_CRC_POS; i < len; i++) {
            pos = rt_snprintf(line, sizeof(line), "%02X ", data[i]);
            rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
        }
        uart2_only_puts("\r\n");
    }
    uart2_only_puts("----------------------------------------\r\n");
}

static void send_pressure_tcp(const rt_uint8_t *data, rt_uint8_t seq)
{
    if (!wifi_tcp_is_connected() || data == RT_NULL) return;
    static char buf[1300];
    int pos = rt_snprintf(buf, sizeof(buf), "[PRESS seq=%u] ", (unsigned int)seq);
    if (pos < 0 || pos >= (int)sizeof(buf)) return;
    for (int i = 0; i < 256; i++) {
        int ret = rt_snprintf(buf + pos, sizeof(buf) - pos, "%d%c", (int)data[i], (i < 255) ? ',' : '\r');
        if (ret < 0 || ret >= (int)(sizeof(buf) - pos)) break;
        pos += ret;
    }
    if (pos < (int)sizeof(buf) - 1) buf[pos++] = '\n';
    wifi_tcp_send(buf, pos);
}

/* ========== R60ABD1 雷达帧辅助函数 ========== */

/* 校验和核对: 对 frame[0..total-4) 求和取低 8 位, 与 frame[total-3] 比较 */
static int R60_VerifyChecksum(const rt_uint8_t *frame, uint16_t total)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < total - R60_TAIL_LEN; i++)
        sum += frame[i];
    return sum == frame[total - R60_TAIL_LEN];
}

/* R60ABD1 原始帧存储（已废弃——STM32 不再透传原始 R60 帧，改用 0xBB TLV） */
static void R60_StoreFrame(const rt_uint8_t *frame, uint16_t total)
{
    (void)frame; (void)total;
}

/* 调试打印: 把原始二进制帧转换成人类可读的 ASCII 串, 输出到 uart2
 * 格式 (示例):
 *   [RADAR #5] len=20 | hdr=0x53 0x59 | ctrl=0x02 | cmd=0x0E |
 *     dlen=14 (hex 0x000E)  data(14B dec): [ 72][ 18][  3]...
 *     checksum=0x7F (calc 0x7F OK)
 *     tail=0x54 0x43
 *   ----------------------------------------
 */
static void dump_radar_frame(const rt_uint8_t *f, int len, unsigned long fno)
{
    char line[160];
    int pos = 0;

    pos = rt_snprintf(line, sizeof(line),
        "\r\n[RADAR #%lu] len=%d | hdr=0x%02X 0x%02X | ctrl=0x%02X | cmd=0x%02X\r\n",
        fno, len, f[0], f[1], f[2], f[3]);
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);

    uint16_t dlen = ((uint16_t)f[4] << 8) | f[5];
    pos = rt_snprintf(line, sizeof(line), "  dlen=%u (hex 0x%04X)  data(%dB dec): ",
                      (unsigned)dlen, (unsigned)dlen, (int)dlen);
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);

    for (int i = 0; i < dlen; i++) {
        pos = rt_snprintf(line, sizeof(line), "[%3u]", (unsigned)f[R60_HEADER_LEN + i]);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
        if ((i + 1) % DEC_COLS == 0) uart2_only_puts("\r\n                            ");
    }
    uart2_only_puts("\r\n");

    /* 校验和 */
    uint8_t calc = 0;
    for (uint16_t i = 0; i < len - R60_TAIL_LEN; i++) calc += f[i];
    pos = rt_snprintf(line, sizeof(line),
        "  checksum=0x%02X (calc 0x%02X %s)\r\n",
        f[len - R60_TAIL_LEN], calc,
        (calc == f[len - R60_TAIL_LEN]) ? "OK" : "FAIL");
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);

    pos = rt_snprintf(line, sizeof(line), "  tail=0x%02X 0x%02X\r\n",
        f[len - 2], f[len - 1]);
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);

    uart2_only_puts("----------------------------------------\r\n");
}

/* ========== 0xBB TLV 合并帧解析 ========== */

#define BB_HEADER     0xBB
#define BB_TAIL       0x55

/* TLV type 常量 */
#define TLV_TYPE_HR         0x01
#define TLV_TYPE_BR         0x02
#define TLV_TYPE_MOTION_MAX 0x03
#define TLV_TYPE_MOTION_AVG 0x04
#define TLV_TYPE_DIST_LATEST 0x05
#define TLV_TYPE_DIST_MIN   0x06
#define TLV_TYPE_DIST_MAX   0x07
#define TLV_TYPE_EXIST      0x08
#define TLV_TYPE_PARAM1     0x09
#define TLV_TYPE_PARAM2     0x0A

static int BB_VerifyCRC(const rt_uint8_t *frame, int total)
{
    uint8_t crc = 0;
    for (int i = 1; i < total - 2; i++)
        crc ^= frame[i];
    return crc == frame[total - 2];
}

static void BB_Parse(const rt_uint8_t *frame, int total)
{
    uint8_t tlv_len = frame[2];
    int pos = 3;
    int end = 3 + tlv_len;

    while (pos + 1 < end) {
        uint8_t type = frame[pos++];
        uint8_t len  = frame[pos++];
        if (pos + len > end) break;

        switch (type) {
        case TLV_TYPE_HR:
            g_radar.hr = (int8_t)frame[pos]; g_radar.hr_valid = 1; break;
        case TLV_TYPE_BR:
            g_radar.br = (int8_t)frame[pos]; g_radar.br_valid = 1; break;
        case TLV_TYPE_MOTION_MAX:
            g_radar.motion_max = frame[pos]; g_radar.motion_max_valid = 1; break;
        case TLV_TYPE_MOTION_AVG:
            g_radar.motion_avg = frame[pos]; g_radar.motion_avg_valid = 1; break;
        case TLV_TYPE_DIST_LATEST:
            if (len >= 2) { g_radar.dist_latest = ((uint16_t)frame[pos] << 8) | frame[pos+1]; g_radar.dist_l_valid = 1; }
            break;
        case TLV_TYPE_DIST_MIN:
            if (len >= 2) { g_radar.dist_min = ((uint16_t)frame[pos] << 8) | frame[pos+1]; g_radar.dist_min_valid = 1; }
            break;
        case TLV_TYPE_DIST_MAX:
            if (len >= 2) { g_radar.dist_max = ((uint16_t)frame[pos] << 8) | frame[pos+1]; g_radar.dist_max_valid = 1; }
            break;
        case TLV_TYPE_EXIST:
            g_radar.exist = frame[pos]; g_radar.exist_valid = 1; break;
        case TLV_TYPE_PARAM1:
            g_radar.param1 = frame[pos]; g_radar.p1_valid = 1; break;
        case TLV_TYPE_PARAM2:
            g_radar.param2 = frame[pos]; g_radar.p2_valid = 1; break;
        }
        pos += len;
    }

    g_radar.valid = 1;
    g_radar.tick = rt_tick_get();
}

static void dump_combined_frame(const rt_uint8_t *frame, int total, unsigned long fno)
{
    char line[160];
    int pos;

    pos = rt_snprintf(line, sizeof(line),
        "\r\n[COMBINED #%lu] len=%d TLV:\r\n", fno, total);
    rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);

    /* RAW hex dump of full TLV body */
    uart2_only_puts("  RAW:");
    for (int i = 0; i < total; i++) {
        pos = rt_snprintf(line, sizeof(line), " %02X", frame[i]);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }
    uart2_only_puts("\r\n");

    if (g_radar.exist_valid) {
        const char *tag = (g_radar.exist & 0x80) ? "有人在" : "无人";
        pos = rt_snprintf(line, sizeof(line), "  存在:0x%02X(%s)\r\n", g_radar.exist, tag);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }

    if (g_radar.hr_valid) {
        const char *tag = "正常";
        if      (g_radar.hr < 40)  tag = "过低";
        else if (g_radar.hr < 60)  tag = "偏低";
        else if (g_radar.hr > 100) tag = "偏高";
        pos = rt_snprintf(line, sizeof(line), "  心率:%dbpm(%s)\r\n", g_radar.hr, tag);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }

    if (g_radar.br_valid) {
        const char *tag = "正常";
        if      (g_radar.br < 8)   tag = "过缓";
        else if (g_radar.br > 20)  tag = "过速";
        pos = rt_snprintf(line, sizeof(line), "  呼吸:%d次/分(%s)\r\n", g_radar.br, tag);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }

    if (g_radar.motion_max_valid) {
        const char *tag_m = "几乎静止";
        if      (g_radar.motion_max > 100) tag_m = "剧烈体动";
        else if (g_radar.motion_max > 60)  tag_m = "明显体动";
        else if (g_radar.motion_max > 30)  tag_m = "中等体动";
        else if (g_radar.motion_max > 10)  tag_m = "轻微体动";

        const char *tag_a = "几乎静止";
        int a = g_radar.motion_avg;
        if      (a > 60)  tag_a = "持续活跃";
        else if (a > 30)  tag_a = "中度活跃";
        else if (a > 10)  tag_a = "轻度活跃";

        pos = rt_snprintf(line, sizeof(line),
            "  体动:max=%d(%s) avg=%d(%s)\r\n",
            g_radar.motion_max, tag_m, g_radar.motion_avg, tag_a);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }

    if (g_radar.dist_l_valid) {
        int range = g_radar.dist_max - g_radar.dist_min;
        const char *tag = "位置稳定";
        if      (range > 30) tag = "大幅移动/翻身";
        else if (range > 10) tag = "小幅移动";

        pos = rt_snprintf(line, sizeof(line),
            "  距离:%dcm(%s) [min=%d max=%d]\r\n",
            g_radar.dist_latest, tag, g_radar.dist_min, g_radar.dist_max);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }

    if (g_radar.p1_valid || g_radar.p2_valid) {
        pos = rt_snprintf(line, sizeof(line),
            "  综合参数: p1=%d p2=%d\r\n", g_radar.param1, g_radar.param2);
        rt_device_write(uart2_dev, 0, (rt_uint8_t *)line, pos);
    }

    uart2_only_puts("----------------------------------------\r\n");
}

/* ========== UART5 接收回调 ========== */
static rt_err_t uart5_rx_ind(rt_device_t dev, rt_size_t size) { rx_isr_call_count++; return RT_EOK; }

/* ========== 帧边界提取 + 透传线程 ========== */
static void uart_relay_thread_entry(void *parameter)
{
    static rt_uint8_t recv_buf[UART_RECV_BUF_SIZE];
    rt_size_t read_len;
    char diag_msg[128];
    int diag_len;
    int search_pos;
    int frames_this_round;

    while (1) {
        uart_poll_cnt++;
        read_len = rt_device_read(uart5_dev, 0, recv_buf, UART_RECV_BUF_SIZE);
        if (read_len > 0) {
            /* RAW hex 输出已注释，仅保留雷达帧和合并帧输出 */
            total_fwd_bytes += read_len;
            if (raw_len + (int)read_len > RAW_BUF_SIZE) raw_len = 0;
            rt_memcpy(&raw_buf[raw_len], recv_buf, read_len);
            raw_len += (int)read_len;
            frames_this_round = 0;
            search_pos = 0;
            while (search_pos < raw_len) {
                /* ---------- 分支 1: R60ABD1 雷达帧 0x53 0x59 (优先) ---------- */
                if (raw_buf[search_pos] == R60_HDR0) {
                    if (search_pos + 1 >= raw_len) break;          /* 等下一字节, 下次再判 */
                    if (raw_buf[search_pos + 1] != R60_HDR1) { search_pos++; continue; }
                    if (search_pos + R60_HEADER_LEN > raw_len) break;  /* 帧头未到齐 */
                    uint16_t dlen = ((uint16_t)raw_buf[search_pos + 4] << 8) | raw_buf[search_pos + 5];
                    if (dlen > R60_MAX_DATA_LEN) { search_pos += 2; continue; }
                    uint16_t total = R60_HEADER_LEN + dlen + R60_TAIL_LEN;
                    if (search_pos + total > raw_len) break;             /* 整帧未到齐, 留待下次 */
                    /* 校验和 + 帧尾核对 */
                    if (R60_VerifyChecksum(&raw_buf[search_pos], total) &&
                        raw_buf[search_pos + total - 2] == R60_TAIL0 &&
                        raw_buf[search_pos + total - 1] == R60_TAIL1) {
                        frame_cnt++;
                        frames_this_round++;
                        R60_StoreFrame(&raw_buf[search_pos], total);
                    }
                    search_pos += total;
                    continue;
                }

                /* ---------- 分支 2: 0xBB 雷达合并帧 ---------- */
                if (raw_buf[search_pos] == BB_HEADER) {
                    /* 最小长度: 帧头(1) + seq(1) + tlv_len(1) + crc(1) + tail(1) = 5 */
                    if (search_pos + 5 > raw_len) break;
                    uint8_t tlv_total = raw_buf[search_pos + 2];
                    int bb_total = 3 + tlv_total + 2;  /* BB+seq+tlv_len + TLV body + CRC+tail */
                    if (search_pos + bb_total > raw_len) break;
                    if (raw_buf[search_pos + bb_total - 1] == BB_TAIL &&
                        BB_VerifyCRC(&raw_buf[search_pos], bb_total)) {
                        frame_cnt++;
                        frames_this_round++;
                        BB_Parse(&raw_buf[search_pos], bb_total);
                        if (wifi_tcp_is_connected() && g_radar.valid) {
                            diag_len = rt_snprintf(diag_msg, sizeof(diag_msg),
                                "[RADAR] HR=%d BR=%d motion=%d dist=%d exist=0x%02X\r\n",
                                g_radar.hr_valid ? (int)g_radar.hr : -1,
                                g_radar.br_valid ? (int)g_radar.br : -1,
                                g_radar.motion_max_valid ? (int)g_radar.motion_max : -1,
                                g_radar.dist_l_valid ? (int)g_radar.dist_latest : -1,
                                g_radar.exist);
                            wifi_tcp_send(diag_msg, diag_len);
                        }
                    }
                    search_pos += bb_total;
                    continue;
                }

                /* ---------- 分支 3: 压力数据帧 0xAA ---------- */
                if (raw_buf[search_pos] == 0xAA) {
                    if (search_pos + FRAME_TOTAL > raw_len) break;
                    /* 严格校验: 长度==256 且 帧尾==0x55 才认 (避免雷达 0xAA 被误切) */
                    rt_uint16_t dlen = ((rt_uint16_t)raw_buf[search_pos + 2] << 8) | raw_buf[search_pos + 3];
                    if (dlen != FRAME_ADC_DATA_LEN || raw_buf[search_pos + FRAME_TAIL_POS] != 0x55) {
                        search_pos++;
                        continue;
                    }
                    frame_cnt++;
                    frames_this_round++;
                    /*压力帧矩阵输出已注释，仅保留解析逻辑*/
                    for (int i = 0; i < FRAME_ADC_DATA_LEN; i++)
                        g_pressure_data[i] = raw_buf[search_pos + FRAME_ADC_START + i];
                    close_filter_3x3(g_pressure_data, g_pressure_filtered, 16, 16);
                    g_data_ready = 1;
                    send_pressure_tcp(g_pressure_filtered, raw_buf[search_pos + 1]);
                    /* UART2 输出：将滤波后的 ADC 数据覆盖回缓冲区，写出完整 268 字节 */
                    rt_memcpy(&raw_buf[search_pos + FRAME_ADC_START], g_pressure_filtered, FRAME_ADC_DATA_LEN);
                    rt_device_write(uart2_dev, 0, &raw_buf[search_pos], FRAME_TOTAL);
                    {
                        float input_data[256];
                        for (int i = 0; i < 256; i++) input_data[i] = (float)g_pressure_filtered[i];
                        int ret = IMPressure_compute(input_data, g_ai_output);
                        if (ret == IPWIN_RET_SUCCESS) {
                            ai_inference_cnt++;
                            int max_idx = 0;
                            int max_val = (int)(g_ai_output[0] * 100);
                            for (int k = 1; k < IMPRESSURE_DATA_OUT_COUNT; k++) {
                                int v = (int)(g_ai_output[k] * 100);
                                if (v > max_val) { max_val = v; max_idx = k; }
                            }
                            diag_len = rt_snprintf(diag_msg, sizeof(diag_msg),
                                " => posture=%s conf=%d%% cnt=%lu\r\n",
                                posture_labels[max_idx], max_val, (unsigned long)ai_inference_cnt);
                            if (wifi_tcp_is_connected()) wifi_tcp_send(diag_msg, diag_len);
                            posture_vote_update(max_idx, max_val);
                        }
                    }
                    /* Phase 9: 读取气囊反馈 byte[262-267] */
                    airbag_update_feedback(
                        raw_buf[search_pos + 262],
                        raw_buf[search_pos + 263],
                        raw_buf[search_pos + 264],
                        raw_buf[search_pos + 265],
                        raw_buf[search_pos + 266]
                    );
                    if (wifi_tcp_is_connected()) {
                        char stat_buf[256];
                        int slen = rt_snprintf(stat_buf, sizeof(stat_buf),
                            "[STAT] F=%lu isr=%lu B=%lu raw_rem=%d\r\n",
                            (unsigned long)frame_cnt, (unsigned long)rx_isr_call_count,
                            (unsigned long)total_fwd_bytes, raw_len);
                        wifi_tcp_send(stat_buf, slen);
                    }
                    search_pos += FRAME_TOTAL;
                    continue;
                }

                /* ---------- 未知字节, 跳过 ---------- */
                search_pos++;
            }
            if (search_pos < raw_len) {
                int leftover = raw_len - search_pos;
                rt_memmove(raw_buf, &raw_buf[search_pos], leftover);
                raw_len = leftover;
            } else { raw_len = 0; }
        }
        rt_thread_mdelay(10);
    }
}

/* ========== 初始化 ========== */
static int uart_relay_init(void)
{
    rt_err_t ret;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    uart5_dev = rt_device_find(UART5_DEV_NAME);
    if (uart5_dev == RT_NULL) { rt_kprintf("[ERR] uart5 not found!\r\n"); return -RT_ERROR; }
    config.baud_rate = UART_BAUDRATE;
    config.data_bits = DATA_BITS_8; config.stop_bits = STOP_BITS_1;
    config.parity = PARITY_NONE; config.bit_order = BIT_ORDER_LSB;
    config.invert = NRZ_NORMAL; config.bufsz = UART_RECV_BUF_SIZE;
    config.flowcontrol = RT_SERIAL_FLOWCONTROL_NONE;
    ret = rt_device_control(uart5_dev, RT_DEVICE_CTRL_CONFIG, &config);
    if (ret != RT_EOK) return ret;
    ret = rt_device_open(uart5_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_OFLAG_RDWR);
    if (ret != RT_EOK) return ret;
    rt_device_set_rx_indicate(uart5_dev, uart5_rx_ind);
    rt_kprintf("[OK] uart5 @ %dbps INT_RX\r\n", UART_BAUDRATE);
    uart2_dev = rt_device_find(UART2_DEV_NAME);
    if (uart2_dev == RT_NULL) return -RT_ERROR;
    ret = rt_device_open(uart2_dev, RT_DEVICE_OFLAG_RDWR);
    if (ret != RT_EOK) return ret;
    rt_kprintf("[OK] uart2 TX + WiFi TCP\r\n");
    rt_thread_t thread = rt_thread_create("uart_relay", uart_relay_thread_entry, RT_NULL,
                                          RELAY_STACK_SIZE, RELAY_PRIORITY, RELAY_TIMESLICE);
    if (thread == RT_NULL) return -RT_ENOMEM;
    rt_thread_startup(thread);
    wifi_tcp_init(WIFI_TCP_DEFAULT_SERVER_IP, WIFI_TCP_DEFAULT_SERVER_PORT);
    wifi_tcp_cmd_start();
    return RT_EOK;
}
INIT_APP_EXPORT(uart_relay_init);

static int ai_model_early_init(void)
{
    rt_thread_mdelay(500);
    IMPressure_init();
    rt_kprintf("[OK] AI model initialized\r\n");
    return 0;
}
INIT_APP_EXPORT(ai_model_early_init);

void lv_user_gui_init(void)
{
    ui_main_init();
    ui_main_wifi_init(ui_main_get_main_screen());
}

int main(void)
{
    rt_kprintf("================================\r\n");
    rt_kprintf(" Hello RT-Thread (Cortex-M55)\r\n");
    rt_kprintf(" Pressure Data Receiver + LVGL\r\n");
    rt_kprintf(" IPC Sensor Receiver\r\n");
    rt_kprintf(" Airbag Posture Control\r\n");
    rt_kprintf("================================\r\n");

    rt_pin_mode(LED_PIN_G, PIN_MODE_OUTPUT);
    rt_kprintf("[MAIN] LED pin mode set\r\n");

    airbag_system_init();

    alarm_clock_init();

    lvgl_thread_init();
    rt_kprintf("[MAIN] LVGL thread started\r\n");

    int led_toggle = 0;
    while (1) {
        rt_thread_mdelay(100);
        led_toggle++;
        if (led_toggle >= 5) {
            rt_pin_write(LED_PIN_G, !rt_pin_read(LED_PIN_G));
            led_toggle = 0;
        }
        if (!airbag_test_mode_is_active()) {
            int posture = posture_get_confirmed();
            /* 鼾声超时保护: IPC 超过 5s 未到 → 强制清 0（防 M33 掉线卡死） */
            int snore = g_snore_detected;
            if (rt_tick_get() - g_snore_last_tick > rt_tick_from_millisecond(SNORE_IPC_TIMEOUT_MS)) {
                if (snore) {
                    rt_kprintf("[SNORE-IPC] Timeout: M33 silent >%dms, clearing snore\r\n",
                               SNORE_IPC_TIMEOUT_MS);
                }
                /* B1: 批次不完整时保持当前批次不变，不清空投票缓冲区 */
                g_snore_detected = 0;
                snore = 0;
            }
            airbag_process(posture, snore);
        }
    }
    return 0;
}