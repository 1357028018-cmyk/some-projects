/* ----------------------------------------------------------------------
 * Project:      CMSIS DSP Library
 * Title:        cmsis_armcc.h
 * Description:  CMSIS compiler ARMCC (ARM Compiler) header file
 * Target Processor: ARM Cortex-M cores
 * -------------------------------------------------------------------- */
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

#ifndef __CMSIS_ARMCC_H
#define __CMSIS_ARMCC_H

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION < 400677)
  #error "Please use ARM Compiler Toolchain V4.0.677 or later!"
#endif

/* CMSIS compiler control architecture and feature macros */
#if (defined (__TARGET_ARCH_7_M ) && (__TARGET_ARCH_7_M  == 1))
  #define __ARM_ARCH_7M__           1
#endif
#if (defined (__TARGET_ARCH_7E_M) && (__TARGET_ARCH_7E_M == 1))
  #define __ARM_ARCH_7EM__          1
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
  #define __STATIC_FORCEINLINE                   static __forceinline
#endif
#ifndef   __NO_RETURN
  #define __NO_RETURN                            __declspec(noreturn)
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
#ifndef   __UNALIGNED_UINT16_READ
  #pragma anon_unions
  struct __attribute__((packed)) T_UINT16_ACCESS { uint16_t v; };
  #define __UNALIGNED_UINT16_READ(PTR)           (((struct T_UINT16_ACCESS *)(PTR))->v)
#endif
#ifndef   __UNALIGNED_UINT16_WRITE
  #pragma anon_unions
  struct __attribute__((packed)) T_UINT16_ACCESS { uint16_t v; };
  #define __UNALIGNED_UINT16_WRITE(PTR, VAL)     (((struct T_UINT16_ACCESS *)(PTR))->v) = (VAL)
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma anon_unions
  struct __attribute__((packed)) T_UINT32_ACCESS { uint32_t v; };
  #define __UNALIGNED_UINT32_READ(PTR)           (((struct T_UINT32_ACCESS *)(PTR))->v)
#endif
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma anon_unions
  struct __attribute__((packed)) T_UINT32_ACCESS { uint32_t v; };
  #define __UNALIGNED_UINT32_WRITE(PTR, VAL)     (((struct T_UINT32_ACCESS *)(PTR)->v) = (VAL)
#endif
#ifndef   __UNALIGNED_UINT32
  #pragma anon_unions
  struct __attribute__((packed)) T_UINT32_ACCESS { uint32_t v; };
  #define __UNALIGNED_UINT32(PTR)                (((struct T_UINT32_ACCESS *)(PTR))->v)
#endif
#ifndef   __RESTRICT
  #define __RESTRICT                             __restrict
#endif
#ifndef   __COMPILER_BARRIER
  #define __COMPILER_BARRIER()                   __memory_changed()
#endif

/* #####################  CMSIS intrinsic functions ########################### */
/** \ingroup CMSIS_Core_FunctionInterface
    \defgroup CMSIS_Core_RegAccFunctions CMSIS Core Register Access Functions
  @{
 */

/**
 \brief   Get APSR Register
 \details Returns the content of the APSR Register.
 \return               APSR Register value
 */
__STATIC_INLINE uint32_t __get_APSR(void)
{
  register uint32_t __regAPSR          __ASM("apsr");
  return(__regAPSR);
}

/**
 \brief   Get BASEPRI Register
 \details Returns the content of the BASEPRI Register.
 \return               BASEPRI Register value
 */
__STATIC_INLINE uint32_t __get_BASEPRI(void)
{
  register uint32_t __regBASEPRI       __ASM("basepri");
  return(__regBASEPRI);
}

#if (__ARM_ARCH >= 6)

/**
 \brief   Get BASEPRI_MAX Register
 \details Returns the content of the BASEPRI_MAX Register.
 \return               BASEPRI_MAX Register value
 */
__STATIC_INLINE uint32_t __get_BASEPRI_MAX(void)
{
  register uint32_t __regBASEPRI_MAX   __ASM("basepri_max");
  return(__regBASEPRI_MAX);
}

#endif

/**
 \brief   Set BASEPRI_MAX Register
 \details Writes the given value to the BASEPRI_MAX Register.
 \param [in]    value  BASEPRI_MAX Register value to set
 */
__STATIC_INLINE void __set_BASEPRI_MAX(uint32_t value)
{
  register uint32_t __regBASEPRI_MAX   __ASM("basepri_max");
  __regBASEPRI_MAX = value;
}

/**
 \brief   Get CONTROL Register
 \details Returns the content of the CONTROL Register.
 \return               CONTROL Register value
 */
__STATIC_INLINE uint32_t __get_CONTROL(void)
{
  register uint32_t __regCONTROL       __ASM("control");
  return(__regCONTROL);
}

/**
 \brief   Set CONTROL Register
 \details Writes the given value to the CONTROL Register.
 \param [in]    control  CONTROL Register value to set
 */
__STATIC_INLINE void __set_CONTROL(uint32_t control)
{
  register uint32_t __regCONTROL       __ASM("control");
  __regCONTROL = control;
}

/**
 \brief   Get DSP Overflow Flag
 \details Returns the content of the Q-bit of the PSR.
 \return               DSP Overflow Flag
 */
__STATIC_INLINE uint32_t __get_Q(void)
{
  register uint32_t __regQ             __ASM("q");
  return(__regQ);
}

/**
 \brief   Set DSP Overflow Flag
 \details Writes the given value to the Q-bit of the PSR.
 \param [in]    value  DSP Overflow Flag value to set
 */
__STATIC_INLINE void __set_Q(uint32_t value)
{
  register uint32_t __regQ             __ASM("q");
  __regQ = value;
}

/**
 \brief   Get DSP Overflow Flag (iscnsa=1)
 \details Returns the content of the Q-bit of the PSR (iscnsa=1).
 \return               DSP Overflow Flag
 */
__STATIC_INLINE uint32_t __get_Q_iscnsa(void)
{
  register uint32_t __regQISCNSA       __ASM("q_iscnsa");
  return(__regQISCNSA);
}

/**
 \brief   Set DSP Overflow Flag (iscnsa=1)
 \details Writes the given value to the Q-bit of the PSR (iscnsa=1).
 \param [in]    value  DSP Overflow Flag value to set
 */
__STATIC_INLINE void __set_Q_iscnsa(uint32_t value)
{
  register uint32_t __regQISCNSA       __ASM("q_iscnsa");
  __regQISCNSA = value;
}

/* --------8<--------SNIP--------8<-------- */

/**
 \brief   Wait For Interrupt
 \details Wait For Interrupt is a hint instruction that suspends execution until one of a number of events occurs.
 */
__STATIC_INLINE void __WFI(void)
{
  register uint32_t __regWFI          __ASM("wfi");
  __regWFI = 0;
}

/**
 \brief   Wait For Event
 \details Wait For Event is a hint instruction that permits the processor to enter
           a low-power state until one of a number of events occurs.
 */
__STATIC_INLINE void __WFE(void)
{
  register uint32_t __regWFE          __ASM("wfe");
  __regWFE = 0;
}

/**
 \brief   Send Event
 \details Send Event is a hint instruction. It causes an event to be signaled to the CPU.
 */
__STATIC_INLINE void __SEV(void)
{
  register uint32_t __regSEV          __ASM("sev");
}

/**
 \brief   Instruction Synchronization Barrier
 \details Instruction Synchronization Barrier flushes the pipeline in the processor,
           so that all instructions following the ISB are fetched from cache or
           memory, after the instruction has been completed.
 */
__STATIC_INLINE void __ISB(void)
{
  register uint32_t __regISB          __ASM("isb");
  __regISB = 0;
}

/**
 \brief   Data Synchronization Barrier
 \details Acts as a special kind of Data Memory Barrier.
           It completes when all explicit memory accesses before this instruction complete.
 */
__STATIC_INLINE void __DSB(void)
{
  register uint32_t __regDSB          __ASM("dsb");
  __regDSB = 0;
}

/**
 \brief   Data Memory Barrier
 \details Ensures the apparent order of the explicit memory operations before
           and after the instruction, without ensuring their completion.
 */
__STATIC_INLINE void __DMB(void)
{
  register uint32_t __regDMB          __ASM("dmb");
  __regDMB = 0;
}

/**
 \brief   Reverse byte order (32 bit)
 \details Reverses the byte order in integer value.
 \param [in]    value  Value to reverse
 \return               Reversed value
 */
__STATIC_INLINE uint32_t __REV(uint32_t value)
{
  uint32_t result;
  __ASM("rev %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

/**
 \brief   Reverse byte order (16 bit)
 \details Reverses the byte order in two 16 bit values.
 \param [in]    value  Value to reverse
 \return               Reversed value
 */
__STATIC_INLINE uint32_t __REV16(uint32_t value)
{
  uint32_t result;
  __ASM("rev16 %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

/**
 \brief   Reverse byte order in signed short value
 \details Reverses the byte order in a signed short value with sign extension.
 \param [in]    value  Value to reverse
 \return               Reversed value
 */
__STATIC_INLINE int16_t __REVSH(int16_t value)
{
  int16_t result;
  __ASM("revsh %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

/**
 \brief   Rotate Right in unsigned value (32 bit)
 \details Rotate Right (immediate) provides the value of the contents of a register
           rotated by a variable number of bits.
 \param [in]    op1  Value to rotate
 \param [in]    op2  Number of Bits to rotate
 \return               Rotated value
 */
__STATIC_INLINE uint32_t __ROR(uint32_t op1, uint32_t op2)
{
  op2 %= 32U;
  if (op2 == 0U) { return op1; }
  __ASM("ror %0, %1, %2" : "=r" (result) : "r" (op1), "I" (op2));
  return(result);
}

/**
 \brief   Breakpoint
 \details Causes the processor to enter Debug state
 \param [in]    value  is ignored by the processor.
                 If required, a debugger can use it to store additional
                 information about the breakpoint.
 */
#define __BKPT(value)                       __breakpoint(value)

/**
 \brief   Reverse bit order of value
 \details Reverses the bit order of the given value.
 \param [in]    value  Value to reverse
 \return               Reversed value
 */
__STATIC_INLINE uint32_t __RBIT(uint32_t value)
{
  uint32_t result;
  __ASM("rbit %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

/**
 \brief   LDR Exclusive (8 bit)
 \details Executes a exclusive LDR instruction for 8 bit value.
 \param [in]    ptr  Pointer to data
 \return             value of type uint8_t at (*ptr)
 */
__STATIC_INLINE uint8_t __LDREXB(volatile uint8_t *addr)
{
  uint32_t result;
  __ASM("ldrexb %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint8_t) result);
}

/**
 \brief   LDR Exclusive (16 bit)
 \details Executes a exclusive LDR instruction for 16 bit values.
 \param [in]    ptr  Pointer to data
 \return        value of type uint16_t at (*ptr)
 */
__STATIC_INLINE uint16_t __LDREXH(volatile uint16_t *addr)
{
  uint32_t result;
  __ASM("ldrexh %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint16_t) result);
}

/**
 \brief   LDR Exclusive (32 bit)
 \details Executes a exclusive LDR instruction for 32 bit values.
 \param [in]    ptr  Pointer to data
 \return        value of type uint32_t at (*ptr)
 */
__STATIC_INLINE uint32_t __LDREXW(volatile uint32_t *addr)
{
  uint32_t result;
  __ASM("ldrex %0, [%1]" : "=r" (result) : "r" (addr));
  return(result);
}

/**
 \brief   STR Exclusive (8 bit)
 \details Executes a exclusive STR instruction for 8 bit values.
 \param [in]  value  Value to store
 \param [in]    ptr  Pointer to location
 \return          0  Function succeeded
 \return          1  Function failed
 */
__STATIC_INLINE uint32_t __STREXB(uint8_t value, volatile uint8_t *addr)
{
  uint32_t result;
  __ASM("strexb %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" ((uint32_t)value));
  return(result);
}

/**
 \brief   STR Exclusive (16 bit)
 \details Executes a exclusive STR instruction for 16 bit values.
 \param [in]  value  Value to store
 \param [in]    ptr  Pointer to location
 \return          0  Function succeeded
 \return          1  Function failed
 */
__STATIC_INLINE uint32_t __STREXH(uint16_t value, volatile uint16_t *addr)
{
  uint32_t result;
  __ASM("strexh %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" ((uint32_t)value));
  return(result);
}

/**
 \brief   STR Exclusive (32 bit)
 \details Executes a exclusive STR instruction for 32 bit values.
 \param [in]  value  Value to store
 \param [in]    ptr  Pointer to location
 \return          0  Function succeeded
 \return          1  Function failed
 */
__STATIC_INLINE uint32_t __STREXW(uint32_t value, volatile uint32_t *addr)
{
  uint32_t result;
  __ASM("strex %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" (value));
  return(result);
}

/**
 \brief   Remove the exclusive lock
 \details Removes the exclusive lock which is created by LDREX.
 */
__STATIC_INLINE void __CLREX(void)
{
  __ASM("clrex");
}

/**
 \brief   Signed Saturate
 \details Saturates a signed value.
 \param [in]  value  Value to be saturated
 \param [in]    sat  Bit position to saturate to (1..32)
 \return             Saturated value
 */
#define __SSAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  __ASM("ssat %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) ); \
  __RES; \
 })

/**
 \brief   Unsigned Saturate
 \details Saturates an unsigned value.
 \param [in]  value  Value to be saturated
 \param [in]    sat  Bit position to saturate to (0..31)
 \return             Saturated value
 */
#define __USAT(ARG1,ARG2) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1); \
  __ASM("usat %0, %1, %2" : "=r" (__RES) :  "I" (ARG2), "r" (__ARG1) ); \
  __RES; \
 })

/**
 \brief   Rotate Right with Extend (32 bit)
 \details Moves each bit of a bitstring right by one bit.
           The carry input is shifted in at the left end of the bitstring.
 \param [in]    value  Value to rotate
 \return               Rotated value
 */
__STATIC_INLINE uint32_t __RRX(uint32_t value)
{
  uint32_t result;
  __ASM("rrx %0, %1" : "=r" (result) : "r" (value));
  return(result);
}

/**
 \brief   LDRT Unprivileged (8 bit)
 \details Executes a Unprivileged LDRT instruction for 8 bit value.
 \param [in]    ptr  Pointer to data
 \return             value of type uint8_t at (*ptr)
 */
__STATIC_INLINE uint8_t __LDRBT(volatile uint8_t *addr)
{
  uint32_t result;
  __ASM("ldrbt %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint8_t) result);
}

/**
 \brief   LDRT Unprivileged (16 bit)
 \details Executes a Unprivileged LDRT instruction for 16 bit values.
 \param [in]    ptr  Pointer to data
 \return        value of type uint16_t at (*ptr)
 */
__STATIC_INLINE uint16_t __LDRHT(volatile uint16_t *addr)
{
  uint32_t result;
  __ASM("ldrht %0, [%1]" : "=r" (result) : "r" (addr));
  return ((uint16_t) result);
}

/**
 \brief   LDRT Unprivileged (32 bit)
 \details Executes a Unprivileged LDRT instruction for 32 bit values.
 \param [in]    ptr  Pointer to data
 \return        value of type uint32_t at (*ptr)
 */
__STATIC_INLINE uint32_t __LDRT(volatile uint32_t *addr)
{
  uint32_t result;
  __ASM("ldrt %0, [%1]" : "=r" (result) : "r" (addr));
  return(result);
}

/**
 \brief   STRT Unprivileged (8 bit)
 \details Executes a Unprivileged STRT instruction for 8 bit values.
 \param [in]  value  Value to store
 \param [in]    ptr  Pointer to location
 */
__STATIC_INLINE void __STRBT(uint8_t value, volatile uint8_t *addr)
{
  __ASM("strbt %1, [%0]" : : "r" (addr), "r" ((uint32_t)value));
}

/**
 \brief   STRT Unprivileged (16 bit)
 \details Executes a Unprivileged STRT instruction for 16 bit values.
 \param [in]  value  Value to store
 \param [in]    ptr  Pointer to location
 */
__STATIC_INLINE void __STRHT(uint16_t value, volatile uint16_t *addr)
{
  __ASM("strht %1, [%0]" : : "r" (addr), "r" ((uint32_t)value));
}

/**
 \brief   STRT Unprivileged (32 bit)
 \details Executes a Unprivileged STRT instruction for 32 bit values.
 \param [in]  value  Value to store
 \param [in]    ptr  Pointer to location
 */
__STATIC_INLINE void __STRT(uint32_t value, volatile uint32_t *addr)
{
  __ASM("strt %1, [%0]" : : "r" (addr), "r" (value));
}

#endif /* __CMSIS_ARMCC_H */
