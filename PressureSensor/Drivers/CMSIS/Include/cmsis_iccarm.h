/******************************************************************************
 * @file     cmsis_iccarm.h
 * @brief    CMSIS compiler ICCARM (IAR ARM) header file
 * @version  V5.0.8
 * @date     21. July 2018
 ******************************************************************************/
/*
 * Copyright (c) 2017-2018 Arm Limited. All rights reserved.
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

#ifndef __CMSIS_ICCARM_H
#define __CMSIS_ICCARM_H

#include <stdint.h>

/* CMSIS compiler specific defines */
#ifndef   __ASM
  #define __ASM                                  __asm
#endif
#ifndef   __INLINE
  #define __INLINE                               inline
#endif
#ifndef   __STATIC_INLINE
  #define __STATIC_INLINE                        static inline
#endif
#ifndef   __STATIC_FORCEINLINE
  #define __STATIC_FORCEINLINE                   _Pragma("inline=forced") static
#endif
#ifndef   __NO_RETURN
  #define __NO_RETURN                            __attribute__((__noreturn__))
#endif
#ifndef   __USED
  #define __USED                                 __attribute__((used))
#endif
#ifndef   __WEAK
  #define __WEAK                                 __attribute__((weak))
#endif
#ifndef   __PACKED
  #define __PACKED                               __attribute__((packed))
#endif
#ifndef   __PACKED_STRUCT
  #define __PACKED_STRUCT                        __packed struct
#endif
#ifndef   __PACKED_UNION
  #define __PACKED_UNION                         __packed union
#endif
#ifndef   __RESTRICT
  #define __RESTRICT                             __restrict
#endif

__STATIC_INLINE uint32_t __RRX(uint32_t value)
{
  uint32_t result;
  __ASM("RRX      %0, %1" : "=r"(result) : "r" (value));
  return(result);
}

__STATIC_INLINE uint8_t __LDREXB(volatile uint8_t *addr)
{
  uint32_t result;
  __ASM("LDREXB %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint8_t) result);
}

__STATIC_INLINE uint16_t __LDREXH(volatile uint16_t *addr)
{
  uint32_t result;
  __ASM("LDREXH %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint16_t) result);
}

__STATIC_INLINE uint32_t __LDREXW(volatile uint32_t *addr)
{
  uint32_t result;
  __ASM("LDREX %0, [%1]" : "=r" (result) : "r" (addr));
  return(result);
}

__STATIC_INLINE uint32_t __STREXB(uint8_t value, volatile uint8_t *addr)
{
  uint32_t result;
  __ASM("STREXB %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" ((uint32_t)value));
  return(result);
}

__STATIC_INLINE uint32_t __STREXH(uint16_t value, volatile uint16_t *addr)
{
  uint32_t result;
  __ASM("STREXH %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" ((uint32_t)value));
  return(result);
}

__STATIC_INLINE uint32_t __STREXW(uint32_t value, volatile uint32_t *addr)
{
  uint32_t result;
  __ASM("STREX %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" (value));
  return(result);
}

__STATIC_INLINE void __CLREX(void)
{
  __ASM("CLREX");
}

#define __SSAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  __ASM("SSAT %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) ); \
  __RES; \
 })

#define __USAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  __ASM("USAT %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) ); \
  __RES; \
 })

#endif /* __CMSIS_ICCARM_H */
