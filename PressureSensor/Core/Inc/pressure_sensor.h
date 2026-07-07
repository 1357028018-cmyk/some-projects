/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pressure_sensor.h
  * @brief   16x16薄膜压力传感器阵列扫描驱动（轮询版）
  *          使用ADG1606模拟多路复用器 + ADC1轮询 + USART1阻塞发送
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

#ifndef __PRESSURE_SENSOR_H__
#define __PRESSURE_SENSOR_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN Prototypes */

void PressureSensor_Init(void);

void PressureSensor_StartScan(void);

uint8_t PressureSensor_IsSending(void);

/* USER CODE END Prototypes */

/* USER CODE BEGIN Private defines */

#define PS_S0_PORT  GPIOB
#define PS_S0_PIN   GPIO_PIN_2
#define PS_S1_PORT  GPIOE
#define PS_S1_PIN   GPIO_PIN_7
#define PS_S2_PORT  GPIOE
#define PS_S2_PIN   GPIO_PIN_8
#define PS_S3_PORT  GPIOE
#define PS_S3_PIN   GPIO_PIN_9
#define PS_EN_PORT  GPIOE
#define PS_EN_PIN   GPIO_PIN_10

#define PS_ROW_NUM     16
#define PS_COL_NUM     16
#define PS_TOTAL_SIZE  (PS_ROW_NUM * PS_COL_NUM)

#define PS_SET_ROW(row)  do { \
    HAL_GPIO_WritePin(PS_S0_PORT, PS_S0_PIN, ((row) & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET); \
    HAL_GPIO_WritePin(PS_S1_PORT, PS_S1_PIN, ((row) & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET); \
    HAL_GPIO_WritePin(PS_S2_PORT, PS_S2_PIN, ((row) & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET); \
    HAL_GPIO_WritePin(PS_S3_PORT, PS_S3_PIN, ((row) & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET); \
} while(0)

#define PS_EN_ENABLE()   HAL_GPIO_WritePin(PS_EN_PORT, PS_EN_PIN, GPIO_PIN_SET)
#define PS_EN_DISABLE()  HAL_GPIO_WritePin(PS_EN_PORT, PS_EN_PIN, GPIO_PIN_RESET)

#define PS_SETTLE_US    10

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __PRESSURE_SENSOR_H__ */
