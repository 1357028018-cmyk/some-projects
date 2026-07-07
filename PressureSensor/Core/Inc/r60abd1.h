/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    r60abd1.h
 * @brief   R60ABD1 毫米波雷达模块 UART 接收驱动
 *          通过 USART3 接收雷达主动推送的数据帧（PB10/PB11），
 *          解析后通过 USART1 转发给 Edgi-Talk 开发板。
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
/* USER CODE END Header */
#ifndef __R60ABD1_H__
#define __R60ABD1_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define R60_WIN_SIZE        15
#define R60_COMBINED_MAX    64
#define RADAR_TX_INTERVAL   15

typedef struct {
    uint8_t  hr;      uint8_t  hr_valid;
    uint8_t  br;      uint8_t  br_valid;
    uint8_t  exist;   uint8_t  exist_valid;
    uint8_t  param1;  uint8_t  p1_valid;
    uint8_t  param2;  uint8_t  p2_valid;

    uint8_t  motion_buf[R60_WIN_SIZE];
    uint16_t dist_buf[R60_WIN_SIZE];
    uint8_t  motion_cnt;
    uint8_t  dist_cnt;
    uint8_t  win_idx;
} radar_agg_t;

void R60ABD1_Init(void);

void R60ABD1_BuildCombinedFrame(uint8_t *buf, uint16_t *len);

void R60ABD1_ResetWindow(void);

#endif
