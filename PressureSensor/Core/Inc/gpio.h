/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
#define PB12_Pin       GPIO_PIN_12
#define PB12_GPIO_Port GPIOB
#define PB13_Pin       GPIO_PIN_13
#define PB13_GPIO_Port GPIOB
#define PB14_Pin       GPIO_PIN_14
#define PB14_GPIO_Port GPIOB
#define PB15_Pin       GPIO_PIN_15
#define PB15_GPIO_Port GPIOB
#define PD8_Pin        GPIO_PIN_8
#define PD8_GPIO_Port  GPIOD
#define PD9_Pin        GPIO_PIN_9
#define PD9_GPIO_Port  GPIOD
#define PD10_Pin       GPIO_PIN_10
#define PD10_GPIO_Port GPIOD

#define VALVE_LEFT_PIN      PD8_Pin
#define VALVE_LEFT_PORT     PD8_GPIO_Port
#define VALVE_MID_PIN       PD9_Pin
#define VALVE_MID_PORT      PD9_GPIO_Port
#define VALVE_RIGHT_PIN     PD10_Pin
#define VALVE_RIGHT_PORT    PD10_GPIO_Port
/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */
