#ifndef AIRBAG_CONTROL_H
#define AIRBAG_CONTROL_H

#include <rtthread.h>
#include <rtdevice.h>

/*
 * Phase 9: M55 只做决策, STM32 执行状态机
 * 通信: UART5 发送 0xA5 + STATE_ID + 0x00 + 0x5A
 * 反馈: 0xAA 压力帧 byte[262-267]
 *
 * STM32 GPIO:
 *   PB12=左充气泵 PB13=中充气泵 PB14=右充气泵 PB15=吸气泵
 *   PD8=左电磁阀 PD9=中电磁阀 PD10=右电磁阀 (HIGH=吸气通路)
 */

/* ========== STATE_ID (与 STM32 airbag_driver.h 一致) ========== */
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

#define ST_IS_DONE(s)   ((s) == ST_COMFORT_INFLATE_DONE || \
                         (s) == ST_INFLATING_SUPINE_D || \
                         (s) == ST_INFLATING_LEFT_LD || \
                         (s) == ST_INFLATING_RIGHT_LD || \
                         (s) == ST_HOLDING_DONE || (s) == ST_DEFLATING_DONE || (s) == ST_TEST_MODE_DONE || \
                         ((s) >= ST_MANUAL_INFLATE_LD && (s) <= ST_MANUAL_DEFLATE_RD))

#define ST_HAS_DONE(s)  ((s) == ST_COMFORT_INFLATE || \
                         (s) == ST_INFLATING_SUPINE || \
                         (s) == ST_INFLATING_LEFT_LAT || \
                         (s) == ST_INFLATING_RIGHT_LAT || \
                         (s) == ST_HOLDING || (s) == ST_DEFLATING || (s) == ST_TEST_MODE || \
                         ((s) >= ST_MANUAL_INFLATE_L && (s) <= ST_MANUAL_DEFLATE_R))

/* ========== 姿势索引 (与main.c一致, 模型5分类) ========== */
#define POSTURE_UNLABELED      0
#define POSTURE_SUPINE         1
#define POSTURE_LEFT_LATERAL   2
#define POSTURE_RIGHT_LATERAL  3
#define POSTURE_NOISE          4

/* ========== 公共变量 (STM32反馈) ========== */
extern volatile uint8_t g_airbag_fb_state;
extern volatile uint8_t g_airbag_fb_secs[3];
extern volatile uint8_t g_airbag_fb_test_active;
extern volatile uint8_t g_airbag_fb_error;

/* ========== 公共 API ========== */

/* 初始化: 发 SET_STATE(IDLE) */
void airbag_system_init(void);

/* 决策循环: 读 byte[262] 反馈 → 决定下一步 → 发 SET_STATE
 * 每 200ms 调用一次, 与姿势检测同频
 */
void airbag_process(int current_posture, int snore_detected);

/* 更新 STM32 反馈数据 (由 main.c UART5 解析调用) */
void airbag_update_feedback(uint8_t st, uint8_t l, uint8_t m, uint8_t r, uint8_t flags);

/* 查询当前 STM32 状态名称 */
const char *airbag_state_name(void);

/* 查询气囊秒数 (供 UI) */
uint8_t airbag_get_sec(int idx);

/* 测试模式 */
void airbag_test_mode_set(int enable);
int  airbag_test_mode_is_active(void);

/* 手动控制: bag=0/1/2 表示左/中/右, 排队等当前 _DONE 后自动发送 */
void airbag_manual_inflate(uint8_t bag);
void airbag_manual_deflate(uint8_t bag);
int  airbag_manual_is_active(void);

/* 直接发送 SET_STATE（供 test/debug 使用） */
void airbag_send_state(uint8_t state);

/* 一键充气：无视当前状态，强制进入 COMFORT 充气模式 */
void airbag_inflate_all(void);

/* 一键放气：无视当前状态，强制所有气囊依次放空 */
void airbag_deflate_all(void);

#endif /* AIRBAG_CONTROL_H */
