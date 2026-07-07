/******************************************************************************
 * @file     cmsis_gcc.h
 * @brief    CMSIS compiler GCC header file
 * @version  V5.0.4
 * @date     09. April 2018
 ******************************************************************************/
/*
 * Copyright (c) 2009-2018 Arm Limited. All rights reserved.
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

#ifndef __CMSIS_GCC_H
#define __CMSIS_GCC_H

/* ignore some GCC warnings */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"

/* Fallback for __has_builtin */
#if !defined(__has_builtin)
  #define __has_builtin(x) (0)
#endif

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
  #define __STATIC_FORCEINLINE                   __attribute__((always_inline)) static inline
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
  #define __PACKED                               __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_STRUCT
  #define __PACKED_STRUCT                        struct __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_UNION
  #define __PACKED_UNION                         union __attribute__((packed, aligned(1)))
#endif
#ifndef   __UNALIGNED_UINT16_READ
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  __attribute__((aligned(1))) struct { uint16_t v; } __unaligned_uint16;
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT16_READ(PTR)           (((struct __attribute__((packed)) { uint16_t v; } *)(PTR))->v)
#endif
#ifndef   __UNALIGNED_UINT16_WRITE
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  __attribute__((aligned(1))) struct { uint16_t v; } __unaligned_uint16;
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT16_WRITE(PTR, VAL)     (((struct __attribute__((packed)) { uint16_t v; } *)(PTR))->v) = (VAL)
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  __attribute__((aligned(1))) struct { uint32_t v; } __unaligned_uint32;
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32_READ(PTR)           (((struct __attribute__((packed)) { uint32_t v; } *)(PTR))->v)
#endif
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  __attribute__((aligned(1))) struct { uint32_t v; } __unaligned_uint32;
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32_WRITE(PTR, VAL)     (((struct __attribute__((packed)) { uint32_t v; } *)(PTR))->v) = (VAL)
#endif
#ifndef   __UNALIGNED_UINT32
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  __attribute__((aligned(1))) struct { uint32_t v; } __unaligned_uint32;
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32(PTR)                (((struct __attribute__((packed)) { uint32_t v; } *)(PTR))->v)
#endif
#ifndef   __RESTRICT
  #define __RESTRICT                             __restrict
#endif
#ifndef   __COMPILER_BARRIER
  #define __COMPILER_BARRIER()                   __ASM volatile("":::"memory")
#endif

/* #####################  CMSIS intrinsic functions ########################### */
/** \ingroup CMSIS_Core_FunctionInterface
    \defgroup CMSIS_Core_RegAccFunctions CMSIS Core Register Access Functions
  @{
 */

/**
 \brief   Enable IRQ Interrupts
 \details Enables IRQ interrupts by clearing the I-bit in the CPSR.
           Can only be executed in Privileged modes.
 */
__STATIC_FORCEINLINE void __enable_irq(void)
{
  __ASM volatile ("cpsie i" : : : "memory");
}

/**
 \brief   Disable IRQ Interrupts
 \details Disables IRQ interrupts by setting the I-bit in the CPSR.
           Can only be executed in Privileged modes.
 */
__STATIC_FORCEINLINE void __disable_irq(void)
{
  __ASM volatile ("cpsid i" : : : "memory");
}

/**
 \brief   Get Control Register
 \details Returns the content of the Control Register.
 \return               Control Register value
 */
__STATIC_FORCEINLINE uint32_t __get_CONTROL(void)
{
  uint32_t result;

  __ASM volatile ("MRS %0, control" : "=r" (result) );
  return(result);
}

/* ... remaining implementation ... */

#pragma GCC diagnostic pop

#endif /* __CMSIS_GCC_H */
