/**
  ******************************************************************************
  * @file    r60abd1.c
  * @brief   R60ABD1 UART receive driver
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "r60abd1.h"
#include "usart.h"
#include <string.h>

enum r60_parse_state {
    R60_S_WAIT_H1 = 0,
    R60_S_WAIT_H2,
    R60_S_WAIT_CTRL,
    R60_S_WAIT_CMD,
    R60_S_WAIT_LEN_H,
    R60_S_WAIT_LEN_L,
    R60_S_RECV_DATA,
    R60_S_WAIT_SUM,
    R60_S_WAIT_T1,
    R60_S_WAIT_T2,
};

#define R60_MAX_DATA_LEN   64

static enum r60_parse_state g_state = R60_S_WAIT_H1;

static uint8_t  g_ctrl = 0;
static uint8_t  g_cmd  = 0;
static uint16_t g_data_len = 0;
static uint16_t g_data_idx = 0;
static uint8_t  g_calc_sum = 0;
static uint8_t  g_data_buf[R60_MAX_DATA_LEN];

static uint8_t  g_recv_byte = 0;

static radar_agg_t g_radar = {0};

static uint8_t tlv_put(uint8_t *buf, uint16_t *pos, uint8_t type,
                       uint8_t *val, uint8_t len)
{
    if (*pos + 2 + len > R60_COMBINED_MAX)
        return 0;
    buf[(*pos)++] = type;
    buf[(*pos)++] = len;
    for (uint8_t i = 0; i < len; i++)
        buf[(*pos)++] = val[i];
    return 1;
}

static uint8_t calc_win_max_u8(const uint8_t *buf, uint8_t cnt)
{
    uint8_t m = 0;
    for (uint8_t i = 0; i < cnt && i < R60_WIN_SIZE; i++)
        if (buf[i] > m) m = buf[i];
    return m;
}

static uint8_t calc_win_avg_u8(const uint8_t *buf, uint8_t cnt)
{
    if (cnt == 0) return 0;
    uint32_t sum = 0;
    uint8_t n = (cnt > R60_WIN_SIZE) ? R60_WIN_SIZE : cnt;
    for (uint8_t i = 0; i < n; i++)
        sum += buf[i];
    return (uint8_t)(sum / n);
}

static uint16_t calc_win_min_u16(const uint16_t *buf, uint8_t cnt)
{
    uint16_t m = 0xFFFF;
    for (uint8_t i = 0; i < cnt && i < R60_WIN_SIZE; i++)
        if (buf[i] < m) m = buf[i];
    return (m == 0xFFFF) ? 0 : m;
}

static uint16_t calc_win_max_u16(const uint16_t *buf, uint8_t cnt)
{
    uint16_t m = 0;
    for (uint8_t i = 0; i < cnt && i < R60_WIN_SIZE; i++)
        if (buf[i] > m) m = buf[i];
    return m;
}

static uint8_t R60_ParseByte(uint8_t ch)
{
    switch (g_state) {

    case R60_S_WAIT_H1:
        if (ch == 0x53) {
            g_calc_sum = 0x53;
            g_state = R60_S_WAIT_H2;
        }
        break;

    case R60_S_WAIT_H2:
        if (ch == 0x59) {
            g_calc_sum += 0x59;
            g_state = R60_S_WAIT_CTRL;
        } else {
            g_state = R60_S_WAIT_H1;
        }
        break;

    case R60_S_WAIT_CTRL:
        g_ctrl = ch;
        g_calc_sum += ch;
        g_state = R60_S_WAIT_CMD;
        break;

    case R60_S_WAIT_CMD:
        g_cmd = ch;
        g_calc_sum += ch;
        g_state = R60_S_WAIT_LEN_H;
        break;

    case R60_S_WAIT_LEN_H:
        g_data_len = (uint16_t)ch << 8;
        g_calc_sum += ch;
        g_state = R60_S_WAIT_LEN_L;
        break;

    case R60_S_WAIT_LEN_L:
        g_data_len |= ch;
        g_calc_sum += ch;
        g_data_idx = 0;

        if (g_data_len > R60_MAX_DATA_LEN) {
            g_state = R60_S_WAIT_H1;
            break;
        }
        if (g_data_len == 0) {
            g_state = R60_S_WAIT_SUM;
        } else {
            g_state = R60_S_RECV_DATA;
        }
        break;

    case R60_S_RECV_DATA:
        g_data_buf[g_data_idx++] = ch;
        g_calc_sum += ch;
        if (g_data_idx >= g_data_len) {
            g_state = R60_S_WAIT_SUM;
        }
        break;

    case R60_S_WAIT_SUM:
        if (g_calc_sum == ch) {
            g_state = R60_S_WAIT_T1;
        } else {
            g_state = R60_S_WAIT_H1;
        }
        break;

    case R60_S_WAIT_T1:
        if (ch == 0x54) {
            g_state = R60_S_WAIT_T2;
        } else {
            g_state = R60_S_WAIT_H1;
        }
        break;

    case R60_S_WAIT_T2:
        if (ch == 0x43) {
            switch (g_ctrl) {
            case 0x85:
                if (g_cmd == 0x02 && g_data_len >= 1) {
                    g_radar.hr = g_data_buf[0];
                    g_radar.hr_valid = 1;
                }
                break;

            case 0x81:
                if (g_cmd == 0x02 && g_data_len >= 1) {
                    g_radar.br = g_data_buf[0];
                    g_radar.br_valid = 1;
                }
                break;

            case 0x80:
                if (g_cmd == 0x03 && g_data_len >= 1) {
                    g_radar.motion_buf[g_radar.win_idx] = g_data_buf[0];
                    g_radar.motion_cnt++;
                }
                if (g_cmd == 0x04 && g_data_len >= 2) {
                    g_radar.dist_buf[g_radar.win_idx] =
                        ((uint16_t)g_data_buf[0] << 8) | g_data_buf[1];
                    g_radar.dist_cnt++;
                }
                if (g_cmd == 0x05 && g_data_len >= 4) {
                    g_radar.exist   = g_data_buf[0];
                    g_radar.exist_valid = 1;
                    g_radar.param1  = g_data_buf[1];
                    g_radar.p1_valid = 1;
                    g_radar.param2  = g_data_buf[3];
                    g_radar.p2_valid = 1;
                }
                break;
            }

            if (g_ctrl == 0x80 &&
                (g_cmd == 0x03 || g_cmd == 0x04)) {
                g_radar.win_idx = (g_radar.win_idx + 1) % R60_WIN_SIZE;
            }
        }
        g_state = R60_S_WAIT_H1;
        return 1;

    default:
        g_state = R60_S_WAIT_H1;
        break;
    }
    return 0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        R60_ParseByte(g_recv_byte);
        HAL_UART_Receive_IT(&huart3, &g_recv_byte, 1);
    }
    if (huart->Instance == USART1) {
        extern void Airbag_ParseCmdByte(uint8_t ch);
        extern uint8_t g_usart1_rx_byte;
        Airbag_ParseCmdByte(g_usart1_rx_byte);
        HAL_UART_Receive_IT(&huart1, &g_usart1_rx_byte, 1);
    }
}

void R60ABD1_Init(void)
{
    HAL_UART_Receive_IT(&huart3, &g_recv_byte, 1);
}

void R60ABD1_BuildCombinedFrame(uint8_t *buf, uint16_t *len)
{
    uint16_t pos = 0;
    uint8_t tmp[2];

    buf[pos++] = 0xBB;
    buf[pos++] = 0;
    uint8_t tlv_len_pos = pos++;

    if (g_radar.hr_valid) {
        tmp[0] = g_radar.hr;
        tlv_put(buf, &pos, 0x01, tmp, 1);
    }

    if (g_radar.br_valid) {
        tmp[0] = g_radar.br;
        tlv_put(buf, &pos, 0x02, tmp, 1);
    }

    if (g_radar.motion_cnt > 0) {
        tmp[0] = calc_win_max_u8(g_radar.motion_buf, g_radar.motion_cnt);
        tlv_put(buf, &pos, 0x03, tmp, 1);
        tmp[0] = calc_win_avg_u8(g_radar.motion_buf, g_radar.motion_cnt);
        tlv_put(buf, &pos, 0x04, tmp, 1);
    }

    if (g_radar.dist_cnt > 0) {
        uint8_t last_idx = (g_radar.win_idx == 0)
                           ? R60_WIN_SIZE - 1
                           : g_radar.win_idx - 1;
        uint16_t last_d = g_radar.dist_buf[last_idx];
        tmp[0] = (last_d >> 8) & 0xFF;
        tmp[1] = last_d & 0xFF;
        tlv_put(buf, &pos, 0x05, tmp, 2);

        uint16_t min_d = calc_win_min_u16(g_radar.dist_buf, g_radar.dist_cnt);
        tmp[0] = (min_d >> 8) & 0xFF;
        tmp[1] = min_d & 0xFF;
        tlv_put(buf, &pos, 0x06, tmp, 2);

        uint16_t max_d = calc_win_max_u16(g_radar.dist_buf, g_radar.dist_cnt);
        tmp[0] = (max_d >> 8) & 0xFF;
        tmp[1] = max_d & 0xFF;
        tlv_put(buf, &pos, 0x07, tmp, 2);
    }

    if (g_radar.exist_valid) {
        tmp[0] = g_radar.exist;
        tlv_put(buf, &pos, 0x08, tmp, 1);
    }

    if (g_radar.p1_valid) {
        tmp[0] = g_radar.param1;
        tlv_put(buf, &pos, 0x09, tmp, 1);
    }

    if (g_radar.p2_valid) {
        tmp[0] = g_radar.param2;
        tlv_put(buf, &pos, 0x0A, tmp, 1);
    }

    buf[tlv_len_pos] = pos - tlv_len_pos - 1;

    uint8_t crc = 0;
    for (uint16_t i = 1; i < pos; i++)
        crc ^= buf[i];
    buf[pos++] = crc;
    buf[pos++] = 0x55;

    *len = pos;
}

void R60ABD1_ResetWindow(void)
{
    g_radar.motion_cnt = 0;
    g_radar.dist_cnt = 0;
    g_radar.win_idx = 0;
}
