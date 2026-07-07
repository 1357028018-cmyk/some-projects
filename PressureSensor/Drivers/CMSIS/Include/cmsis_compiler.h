/******************************************************************************
 * @file     cmsis_compiler.h
 * @brief    CMSIS compiler specific macros, functions, instructions
 * @version  V5.0.2
 * @date     14. November 2017
 ******************************************************************************/
/*
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if   defined ( __ICCARM__ )
  #pragma system_include         /* treat file as system include file for MISRA check */
#elif defined (__clang__)
  #pragma clang system_header   /* treat file as system include file */
#endif

#ifndef __CMSIS_COMPILER_H
#define __CMSIS_COMPILER_H

#include <stdint.h>

/*
 * \brief  CMSIS compiler specific macros, functions, instructions
 */

/* Fallback for __has_include */
#if !defined(__has_include)
  #define __has_include(name) (0)
#endif


/*
 * Define compiler specific macros
 */
#if   defined ( __CC_ARM )
  #include "cmsis_armcc.h"
#elif defined ( __ARMCC_VERSION ) && ( __ARMCC_VERSION >= 6010050 )
  #include "cmsis_armclang.h"
#elif defined ( __GNUC__ )
  #include "cmsis_gcc.h"
#elif defined ( __ICCARM__ )
  #include "cmsis_iccarm.h"
#else
  #error Unknown compiler
#endif

#endif /* __CMSIS_COMPILER_H */
