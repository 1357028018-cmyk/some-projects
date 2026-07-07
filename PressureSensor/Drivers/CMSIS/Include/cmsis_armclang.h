/******************************************************************************
 * @file     cmsis_armclang.h
 * @brief    CMSIS compiler ARMCLANG (Arm Compiler 6) header file
 * @version  V5.0.4
 * @date     10. April 2018
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

#ifndef __CMSIS_ARMCLANG_H
#define __CMSIS_ARMCLANG_H

#pragma clang system_header   /* treat file as system include file */

/* CMSIS compiler control architecture and feature macros */
#if (defined (__ARM_ARCH_7M__) && (__ARM_ARCH_7M__ == 1))
  #define __ARM_ARCH_7M__           1
#endif
#if (defined (__ARM_ARCH_7EM__) && (__ARM_ARCH_7EM__ == 1))
  #define __ARM_ARCH_7EM__          1
#endif
#if (defined (__ARM_ARCH_8M_MAIN__) && (__ARM_ARCH_8M_MAIN__ == 1))
  #define __ARM_ARCH_8M_MAIN__      1
#endif
#if (defined (__ARM_ARCH_8M_BASE__) && (__ARM_ARCH_8M_BASE__ == 1))
  #define __ARM_ARCH_8M_BASE__      1
#endif

/* CMSIS compiler specific defines */
#ifndef   __ASM
  #define __ASM                                  __asm
#endif
#ifndef   __INLINE
  #define __INLINE                               __inline
#endif
#ifndef   __STATIC_INLINE
  #define __STATIC_INLINE                        static __inline
#endif
#ifndef   __STATIC_FORCEINLINE
  #define __STATIC_FORCEINLINE                   __attribute__((always_inline)) static __inline
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
#ifndef   __RESTRICT
  #define __RESTRICT                             __restrict
#endif
#ifndef   __COMPILER_BARRIER
  #define __COMPILER_BARRIER()                   __ASM volatile("":::"memory")
#endif

/* ###########################  Core Function Access  ########################### */
/** \ingroup CMSIS_Core_FunctionInterface
    \defgroup CMSIS_Core_RegAccFunctions CMSIS Core Register Access Functions
  @{
 */

#if (defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3))
/**
 \brief   Get CONTROL Register (non-secure)
 \details Returns the content of the non-secure CONTROL Register when in secure mode.
 \return               non-secure CONTROL Register value
 */
__STATIC_FORCEINLINE uint32_t __TZ_get_CONTROL_NS(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, control_ns" : "=r" (result) );
  return(result);
}

/**
 \brief   Set CONTROL Register (non-secure)
 \details Writes the given value to the non-secure CONTROL Register when in secure state.
 \param [in]    control  CONTROL Register value to set
 */
__STATIC_FORCEINLINE void __TZ_set_CONTROL_NS(uint32_t control)
{
  __ASM volatile ("MSR control_ns, %0" : : "r" (control) : "memory");
}
#endif

__STATIC_FORCEINLINE uint32_t __get_CONTROL(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, control" : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE void __set_CONTROL(uint32_t control)
{
  __ASM volatile ("MSR control, %0" : : "r" (control) : "memory");
}

__STATIC_FORCEINLINE uint32_t __get_IPSR(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, ipsr" : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __get_APSR(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, apsr" : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __get_xPSR(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, xpsr" : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE uint32_t __get_PSP(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, psp"  : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE void __set_PSP(uint32_t topOfProcStack)
{
  __ASM volatile ("MSR psp, %0" : : "r" (topOfProcStack) : );
}

__STATIC_FORCEINLINE uint32_t __get_MSP(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, msp" : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE void __set_MSP(uint32_t topOfMainStack)
{
  __ASM volatile ("MSR msp, %0" : : "r" (topOfMainStack) : );
}

__STATIC_FORCEINLINE uint32_t __get_PRIMASK(void)
{
  uint32_t result;
  __ASM volatile ("MRS %0, primask" : "=r" (result) );
  return(result);
}

__STATIC_FORCEINLINE void __set_PRIMASK(uint32_t priMask)
{
  __ASM volatile ("MSR primask, %0" : : "r" (priMask) : "memory");
}

__STATIC_FORCEINLINE void __enable_irq(void)
{
  __ASM volatile ("cpsie i" : : : "memory");
}

__STATIC_FORCEINLINE void __disable_irq(void)
{
  __ASM volatile ("cpsid i" : : : "memory");
}

__STATIC_FORCEINLINE void __enable_fault_irq(void)
{
  __ASM volatile ("cpsie f" : : : "memory");
}

__STATIC_FORCEINLINE void __disable_fault_irq(void)
{
  __ASM volatile ("cpsid f" : : : "memory");
}

__STATIC_FORCEINLINE void __NOP(void)
{
  __ASM volatile ("nop");
}

__STATIC_FORCEINLINE void __WFI(void)
{
  __ASM volatile ("wfi");
}

__STATIC_FORCEINLINE void __WFE(void)
{
  __ASM volatile ("wfe");
}

__STATIC_FORCEINLINE void __SEV(void)
{
  __ASM volatile ("sev");
}

__STATIC_FORCEINLINE void __ISB(void)
{
  __ASM volatile ("isb 0xF":::"memory");
}

__STATIC_FORCEINLINE void __DSB(void)
{
  __ASM volatile ("dsb 0xF":::"memory");
}

__STATIC_FORCEINLINE void __DMB(void)
{
  __ASM volatile ("dmb 0xF":::"memory");
}

__STATIC_FORCEINLINE uint32_t __REV(uint32_t value)
{
  uint32_t result;
  __ASM volatile ("rev %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __REV16(uint32_t value)
{
  uint32_t result;
  __ASM volatile ("rev16 %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

__STATIC_FORCEINLINE int16_t __REVSH(int16_t value)
{
  int16_t result;
  __ASM volatile ("revsh %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __ROR(uint32_t op1, uint32_t op2)
{
  op2 %= 32U;
  if (op2 == 0U) { return op1; }
  uint32_t result;
  __ASM volatile ("ror %0, %1, %2" : "=r" (result) : "r" (op1), "I" (op2));
  return(result);
}

#define __BKPT(value)                       __ASM volatile ("bkpt "#value)

__STATIC_FORCEINLINE uint32_t __RBIT(uint32_t value)
{
  uint32_t result;
  __ASM volatile ("rbit %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

__STATIC_FORCEINLINE uint8_t __LDREXB(volatile uint8_t *addr)
{
  uint32_t result;
  __ASM volatile ("ldrexb %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint8_t) result);
}

__STATIC_FORCEINLINE uint16_t __LDREXH(volatile uint16_t *addr)
{
  uint32_t result;
  __ASM volatile ("ldrexh %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint16_t) result);
}

__STATIC_FORCEINLINE uint32_t __LDREXW(volatile uint32_t *addr)
{
  uint32_t result;
  __ASM volatile ("ldrex %0, [%1]" : "=r" (result) : "r" (addr));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __STREXB(uint8_t value, volatile uint8_t *addr)
{
  uint32_t result;
  __ASM volatile ("strexb %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" ((uint32_t)value));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __STREXH(uint16_t value, volatile uint16_t *addr)
{
  uint32_t result;
  __ASM volatile ("strexh %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" ((uint32_t)value));
  return(result);
}

__STATIC_FORCEINLINE uint32_t __STREXW(uint32_t value, volatile uint32_t *addr)
{
  uint32_t result;
  __ASM volatile ("strex %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" (value));
  return(result);
}

__STATIC_FORCEINLINE void __CLREX(void)
{
  __ASM volatile ("clrex");
}

#define __SSAT(ARG1,ARG2) \
({                          \
  int32_t __RES, __ARG1 = (ARG1); \
  __ASM volatile ("ssat %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) : );
  __RES; \
 })

#define __USAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  __ASM volatile ("usat %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) : );
  __RES; \
 })

__STATIC_FORCEINLINE uint32_t __RRX(uint32_t value)
{
  uint32_t result;
  __ASM volatile ("rrx %0, %1" : "=r" (result) : "r" (value) );
  return(result);
}

#endif /* __CMSIS_ARMCLANG_H */
