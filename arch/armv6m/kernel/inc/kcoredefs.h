/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/

/** ARMv6-M core definitions and minimal core-HAL. */
#ifndef RK_COREDEFS_H
#define RK_COREDEFS_H

#include <kconfig.h>
#include <kcommondefs.h>
#include <kexecutive.h>

RK_BEGIN_DECLS


/* Values */

RK_BEGIN_DECLS

#ifndef UINT8_MAX
#define UINT8_MAX (0xFF) /* 255 */
#endif
#ifndef INT8_MAX
#define INT8_MAX (0x7F) /* 127 */
#endif
#ifndef UINT16_MAX
#define UINT16_MAX (0xFFFF)
#endif
#ifndef INT16_MAX
#define INT16_MAX (0x7FFF)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX (0xFFFFFFFF) /* 4,294,976,295 */
#endif
#ifndef INT32_MAX
#define INT32_MAX (0x7FFFFFFF) /* 2,147,483,547 */
#endif

#define RK_PRIO_TYPE_MAX UINT8_MAX
#define RK_INT_MAX INT32_MAX
#define RK_UINT_MAX UINT32_MAX
#define RK_ULONG_MAX UINT32_MAX
#define RK_LONG_MAX INT32_MAX
#define RK_TICK_TYPE_MAX RK_ULONG_MAX

/* we do not use std _Bool on kernel objects */
#define RK_FALSE (RK_BOOL)0U
#define RK_TRUE  (RK_BOOL)1U

/* Null pointer */
#ifndef NULL
#define NULL ((void *)(0))
#endif

/*** Stackframe Registers offset ***/

#define PSR_OFFSET 1
#define PC_OFFSET 2
#define LR_OFFSET 3
#define R12_OFFSET 4
#define R3_OFFSET 5
#define R2_OFFSET 6
#define R1_OFFSET 7
#define R0_OFFSET 8
#define R11_OFFSET 9
#define R10_OFFSET 10
#define R9_OFFSET 11
#define R8_OFFSET 12
#define R7_OFFSET 13
#define R6_OFFSET 14
#define R5_OFFSET 15
#define R4_OFFSET 16

extern unsigned long RK_gSyTickDiv;
extern unsigned long RK_gSysCoreClock;
extern unsigned long RK_gSysTickInterval;

#define RK_CORE_SCB_BASE            (0xE000ED00UL)
#define RK_CORE_SYSTICK_BASE        (0xE000E010UL)
#define RK_CORE_NVIC_BASE           (0xE000E100UL)


#define RK_CORE_SVC_IRQN                 ((int)-5)
#define RK_CORE_PENDSV_IRQN              ((int)-2)
#define RK_CORE_SYSTICK_IRQN             ((int)-1)

void kCoreInit(void);

/* Assembly Helpers - ARMv6-M (Cortex-M0) compatible versions */
#define RK_DMB RK_ASM volatile("DMB" ::: "memory");
#define RK_DSB RK_ASM volatile("DSB" ::: "memory");
#define RK_ISB RK_ASM volatile("ISB" ::: "memory");
#define RK_NOP RK_ASM volatile("NOP");
#define RK_STUP RK_ASM volatile("SVC #0xAA" ::: "memory");
#define RK_WFI RK_ASM volatile("WFI" ::: "memory");
#define RK_DIS_IRQ RK_ASM volatile("CPSID I" : : : "memory");
#define RK_EN_IRQ RK_ASM volatile("CPSIE I" : : : "memory");

RK_FORCE_INLINE
static inline unsigned kEnterCR(void)
{
    unsigned state;
    RK_ASM volatile("MRS %0, PRIMASK " : "=r"(state));
    RK_DIS_IRQ
    return (state);
}

RK_FORCE_INLINE
static inline void kExitCR(unsigned state)
{
    RK_ASM volatile("MSR PRIMASK, %0" : : "r"(state) : "memory");
}

#define RK_CR_AREA  unsigned RK_crState;
#define RK_CR_ENTER RK_crState = kEnterCR();
#define RK_CR_EXIT kExitCR(RK_crState);

#define RK_HW_REG(addr) *((volatile unsigned long *)(addr))
#define RK_REG_SCB_ICSR RK_HW_REG(0xE000ED04)
#define RK_REG_SYSTICK_CTRL RK_HW_REG(0xE000E010)
#define RK_REG_SYSTICK_LOAD RK_HW_REG(0xE000E014)
#define RK_REG_SYSTICK_VAL RK_HW_REG(0xE000E018)
#define RK_REG_NVIC RK_HW_REG(0xE000E100)
#define RK_PEND_CTXTSWTCH\
    do\
    {\
        RK_REG_SCB_ICSR |= (1U << 28);\
    } while (0);

RK_FORCE_INLINE static inline unsigned kIsISR(void)
{
#if defined(RK_QEMU_UNIT_TEST)
    extern volatile unsigned RK_gQemuTestForceIsr __attribute__((weak));
    if ((&RK_gQemuTestForceIsr != (volatile unsigned *)0) &&
        (RK_gQemuTestForceIsr != 0U))
    {
        return (1U);
    }
#endif
    unsigned ipsr_value;
    RK_ASM("MRS %0, IPSR" : "=r"(ipsr_value));
    return (ipsr_value);
}

/* implementing a “find-first-set” (count trailing zeros)
 * using a de bruijn multiply+LUT
 */
static const unsigned RK_getReadyTbl[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20,
                                           15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19,
                                           16, 7, 26, 12, 18, 6, 11, 5, 10, 9};

RK_FORCE_INLINE
static inline unsigned __getReadyPrio(unsigned readyQBitmask)
{
    RK_DSB
    readyQBitmask = readyQBitmask * 0x077CB531U;

    /* Shift right the top 5 bits */
    unsigned idx = (readyQBitmask >> 27);

    /* LUT */
    unsigned ret = (unsigned)RK_getReadyTbl[idx];
    return (ret);
}

RK_FORCE_INLINE
static inline RK_ERR kInitStack_(UINT *const stackBufPtr, UINT const stackSize,
                                 RK_TASKENTRY const taskFunc, VOID *argsPtr)
{
    if (stackBufPtr == NULL || stackSize < RK_MIN_STACKSIZE || taskFunc == NULL)
    {
        return (RK_ERR_ERROR);
    }
    stackBufPtr[stackSize - PSR_OFFSET] = 0x01000000U;
    stackBufPtr[stackSize - PC_OFFSET] = (UINT)taskFunc;
    stackBufPtr[stackSize - LR_OFFSET] = 0x14141414U;
    stackBufPtr[stackSize - R12_OFFSET] = 0x12121212U;
    stackBufPtr[stackSize - R3_OFFSET] = 0x03030303U;
    stackBufPtr[stackSize - R2_OFFSET] = 0x02020202U;
    stackBufPtr[stackSize - R1_OFFSET] = 0x01010101U;
    stackBufPtr[stackSize - R0_OFFSET] = (UINT)(argsPtr);

    /* armv6m sequence expects R8–R11 first, then R4–R7. */
    stackBufPtr[stackSize - R4_OFFSET] = 0x08080808U; /* R8 */
    stackBufPtr[stackSize - R5_OFFSET] = 0x09090909U; /* R9 */
    stackBufPtr[stackSize - R6_OFFSET] = 0x10101010U; /* R10 */
    stackBufPtr[stackSize - R7_OFFSET] = 0x11111111U; /* R11 */
    stackBufPtr[stackSize - R8_OFFSET] = 0x04040404U; /* R4 */
    stackBufPtr[stackSize - R9_OFFSET] = 0x05050505U; /* R5 */
    stackBufPtr[stackSize - R10_OFFSET] = 0x06060606U; /* R6 */
    stackBufPtr[stackSize - R11_OFFSET] = 0x07070707U; /* R7 */

    /* stack painting */
    for (ULONG j = 17U; j < stackSize; j++)
    {
        stackBufPtr[stackSize - j] = RK_STACK_PATTERN;
    }
    stackBufPtr[0] = RK_STACK_GUARD;
    return (RK_ERR_SUCCESS);
}

RK_END_DECLS

#endif /* RK_COREDEFS_H */
