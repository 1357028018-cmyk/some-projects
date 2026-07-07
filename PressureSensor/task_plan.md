# Task Plan: STM32 GPIO 姘旀车/鐢电闃€鎺у埗锛堟帴鏀?PSoC 鍛戒护锛?
## Goal
鍦?STM32 绔€氳繃 USART1 RX 涓柇鎺ユ敹 PSoC 鍙戞潵鐨?4 瀛楄妭鍛戒护甯э紝瑙ｆ瀽鍚庢帶鍒?PB12-15/PD8 浜斾釜 GPIO銆?
## Current Phase
Phase 1 (STM32 Implementation)

## Phase 1: STM32 绔疄鐜?- [x] Task S1: 鏂板缓 `airbag_driver.h` 鈥?鍛戒护鐮佸畯 + API 澹版槑
- [x] Task S2: 鏂板缓 `airbag_driver.c` 鈥?鍛戒护鐘舵€佹満 + GPIO 鍘熻 + 瀹氭椂鍏虫车
- [x] Task S3: `gpio.h` 鍔犲紩鑴氬畯 PB12-15/PD8
- [x] Task S4: `gpio.c` 鍔犲垵濮嬪寲 + 鏀?PB12/13 鍒濆 LOW
- [x] Task S5: `usart.c` USART1 RX 涓柇浣胯兘 + NVIC
- [x] Task S6: `stm32f1xx_it.c` 鍔?USART1_IRQHandler
- [x] Task S7: `r60abd1.c` HAL_UART_RxCpltCallback 杩藉姞 USART1
- [x] Task S8: `main.c` 璋冪敤 AirbagDriver_Init()
- [x] Task S9: `pressure_sensor.c` 鍔?Airbag_CheckTimers() 杞
- **Status:** complete

## Phase 2: 楠岃瘉
- [ ] V1: 涓插彛鍔╂墜鍙?A5 01 32 5A 鈫?涓囩敤琛ㄦ祴 PB12 5s 鑷姩鍏?- [ ] V2: 鍙?A5 05 00 5A 鈫?PD8=HIGH; A5 04 00 5A 鈫?PB15=HIGH
- [ ] V3: 鍙?A5 07 00 5A 鈫?鍏?LOW
- [ ] V4: 鑱旇皟 PSoC 鈫?鐘舵€佹満椹卞姩 GPIO
- **Status:** pending

## Notes
- 鎵€鏈?GPIO 淇敼鍦?USER CODE 鍖哄畬鎴愶紝CubeMX 鍐嶇敓涓嶄細瑕嗙洊
- PB12/13 鐨勫垵濮嬬數骞充慨鏀瑰湪 CubeMX 绠＄悊鍖猴紙gpio.c:60锛夛紝鍐嶇敓鍚庝細鎭㈠
