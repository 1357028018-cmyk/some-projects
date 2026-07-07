/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pressure_sensor.c
  * @brief   16x16薄膜压力传感器阵列扫描驱动（轮询版）
  *
  * 扫描原理：
  *   ADG1606为16:1模拟多路复用器，通过4根选择线S[3:0]在16个输入通道中切换。
  *   STM32以二进制方式驱动S[3:0]（PE7=S0, PE8=S1, PE9=S2, PE10=S3），
  *   每次选通一行传感器（低电平使能EN），然后依次切换ADC1的采集通道，
  *   通过轮询方式逐个采集16列数据，完成一次完整16x16矩阵扫描后，
  *   数据通过USART1阻塞式发送至Edgi-Talk（压力帧后捎带转发R60ABD1雷达帧）。
  *
  * USART1发送帧序列（每次PressureSensor_StartScan触发）：
  *   [0] 压力数据帧（268字节）：
  *       Byte[0]     : 0xAA (帧头)
  *       Byte[1]     : 帧序号 (0x00-0xFF 循环)
  *       Byte[2-3]   : 数据长度 (0x0100 = 256)
  *       Byte[4-259] : 256字节压力数据 (按行展开，每行16字节)
  *       Byte[260]   : CRC8校验 (从Byte[1]到Byte[259]的异或)
  *       Byte[261]   : 0x55 (帧尾)
  *       Byte[262-267]: 6字节全0填充
  *   [1] R60ABD1雷达转发帧（如有待发数据，紧跟在压力帧之后）：
  *       Byte[0-1]   : 0x53 0x59 (帧头)
  *       Byte[2]     : 控制字
  *       Byte[3]     : 命令字
  *       Byte[4-5]   : 数据长度 (大端)
  *       Byte[6~5+数据长度] : 数据内容 (原始二进制，长度由Byte[4-5]指定，最长64字节)
  *       Byte[6+数据长度]   : 校验和 (帧头到数据末尾求和取低8位)
  *       Byte[7+数据长度~8+数据长度]: 0x54 0x43 (帧尾)
  *
  * ADC通道顺序（按PCB布线排列）：
  *   PB1(PC9) PC5(CH15) PA7(CH7) PA5(CH5) PA3(CH3) PA1(CH1)
  *   PC3(CH13) PC1(CH11) PB0(CH8) PC4(CH14) PA6(CH6) PA4(CH4)
  *   PA2(CH2)  PA0(CH0)  PC2(CH12) PC0(CH10)
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

#include "pressure_sensor.h"
#include "adc.h"
#include "usart.h"
#include "tim.h"

#include "gpio.h"
#include "r60abd1.h"
#include "airbag_driver.h"

#define PS_FRAME_SIZE        268
#define PS_FRAME_DATA_OFFSET   4
#define PS_FRAME_CRC_OFFSET    (PS_FRAME_DATA_OFFSET + PS_TOTAL_SIZE)
#define PS_FRAME_TAIL_OFFSET   (PS_FRAME_CRC_OFFSET + 1)
#define PS_FRAME_FILL_OFFSET   (PS_FRAME_TAIL_OFFSET + 1)

#define PS_DISCARD_COUNT      1
#define PS_VALID_SAMPLES      4
#define PS_SAMPLES_PER_POINT  (PS_DISCARD_COUNT + PS_VALID_SAMPLES)
#define PS_VALID_SHIFT        2

#define PS_FRAMES_PER_TX      4
#define PS_FRAMES_SHIFT       2

#define PS_BUSY_TIMEOUT_MS    500

static const uint8_t ps_adc_ch_map[PS_COL_NUM] = {
    14,  6, 0,  12,
    10,  2,  4, 8,
    15,  7, 1,  13,
    11,  3,  5, 9
};

static uint16_t g_adc_buf[PS_TOTAL_SIZE];
static uint32_t g_acc_buf[PS_TOTAL_SIZE];
static uint8_t g_tx_frame[PS_FRAME_SIZE];

static volatile uint8_t  g_busy     = 0;
static volatile uint8_t  g_frame_seq = 0;
static volatile uint8_t  g_frame_cnt = 0;
static volatile uint32_t g_busy_tick = 0;

static ADC_ChannelConfTypeDef s_adc_ch_cfg = {
    .Channel      = 0,
    .Rank         = 1,
    .SamplingTime = ADC_SAMPLETIME_239CYCLES_5
};

static void PS_DelayUs(uint32_t us)
{
    volatile uint32_t count;
    count = us * 9;
    while (count--) {
        __NOP();
    }
}

static uint8_t PS_CalcCRC8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
    }
    return crc;
}

static void PS_ScanMatrix(void)
{
    uint8_t  row, col, s;
    uint32_t sum;
    HAL_StatusTypeDef status;

    for (row = 0; row < PS_ROW_NUM; row++) {
        PS_EN_DISABLE();
        PS_SET_ROW(row);
        PS_EN_ENABLE();

        PS_DelayUs(PS_SETTLE_US);

        for (col = 0; col < PS_COL_NUM; col++) {
            s_adc_ch_cfg.Channel = ps_adc_ch_map[col];
            status = HAL_ADC_ConfigChannel(&hadc1, &s_adc_ch_cfg);
            if (status != HAL_OK) {
                g_adc_buf[(uint16_t)row * PS_COL_NUM + col] = 0;
                continue;
            }

            for (s = 0; s < PS_DISCARD_COUNT; s++) {
                HAL_ADC_Start(&hadc1);
                (void)HAL_ADC_PollForConversion(&hadc1, 10);
            }

            sum = 0;
            for (s = 0; s < PS_VALID_SAMPLES; s++) {
                HAL_ADC_Start(&hadc1);
                status = HAL_ADC_PollForConversion(&hadc1, 10);
                if (status == HAL_OK) {
                    sum += HAL_ADC_GetValue(&hadc1);
                }
            }

            g_adc_buf[(uint16_t)row * PS_COL_NUM + col] = (uint16_t)(sum >> PS_VALID_SHIFT);
        }
    }

    PS_EN_DISABLE();
}

static void PS_BuildAndSendFrame(void)
{
    uint16_t i;

    g_tx_frame[0] = 0xAA;
    g_tx_frame[1] = g_frame_seq++;
    g_tx_frame[2] = (PS_TOTAL_SIZE >> 8) & 0xFF;
    g_tx_frame[3] = PS_TOTAL_SIZE & 0xFF;

    for (i = 0; i < PS_TOTAL_SIZE; i++) {
        g_tx_frame[PS_FRAME_DATA_OFFSET + i] = (uint8_t)(g_adc_buf[i] & 0xFF);
    }

    g_tx_frame[PS_FRAME_CRC_OFFSET] = PS_CalcCRC8(
        &g_tx_frame[1],
        PS_FRAME_DATA_OFFSET + PS_TOTAL_SIZE - 1
    );

    g_tx_frame[PS_FRAME_TAIL_OFFSET] = 0x55;

    Airbag_UpdateFeedback();
    g_tx_frame[PS_FRAME_FILL_OFFSET + 0] = g_airbag_fb_state;
    g_tx_frame[PS_FRAME_FILL_OFFSET + 1] = g_airbag_fb_left_sec;
    g_tx_frame[PS_FRAME_FILL_OFFSET + 2] = g_airbag_fb_mid_sec;
    g_tx_frame[PS_FRAME_FILL_OFFSET + 3] = g_airbag_fb_right_sec;
    g_tx_frame[PS_FRAME_FILL_OFFSET + 4] = g_airbag_fb_flags;
    g_tx_frame[PS_FRAME_FILL_OFFSET + 5] = 0x00;

    HAL_UART_Transmit(&huart1, g_tx_frame, PS_FRAME_SIZE, 100);
}

void PressureSensor_Init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1);
    PS_EN_DISABLE();
    g_busy       = 0;
    g_frame_seq  = 0;
    g_frame_cnt  = 0;
    g_busy_tick  = 0;
}

void PressureSensor_StartScan(void)
{
    if (g_busy) {
        if ((uint32_t)(HAL_GetTick() - g_busy_tick) > PS_BUSY_TIMEOUT_MS) {
            g_busy = 0;
        } else {
            return;
        }
    }
    g_busy = 1;
    g_busy_tick = HAL_GetTick();

    PS_ScanMatrix();
    PS_BuildAndSendFrame();

    {
        static uint8_t radar_tick = 0;
        if (++radar_tick >= RADAR_TX_INTERVAL) {
            radar_tick = 0;

            uint8_t radar_buf[R60_COMBINED_MAX];
            uint16_t radar_len = 0;

            HAL_NVIC_DisableIRQ(USART3_IRQn);
            R60ABD1_BuildCombinedFrame(radar_buf, &radar_len);
            R60ABD1_ResetWindow();
            HAL_NVIC_EnableIRQ(USART3_IRQn);

            if (radar_len > 0)
                HAL_UART_Transmit(&huart1, radar_buf, radar_len, 100);
        }
    }

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    Airbag_CheckTimers();

    g_busy = 0;
}

uint8_t PressureSensor_IsSending(void)
{
    return g_busy;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        PressureSensor_StartScan();
    }
}
