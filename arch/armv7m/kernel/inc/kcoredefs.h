/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.18                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/** ARMv7-M core definitions and minimal core-HAL. */
#ifndef RK_COREDEFS_H
#define RK_COREDEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kexecutive.h>

extern unsigned long RK_gSyTickDiv;
extern unsigned long RK_gSysCoreClock;
extern unsigned long RK_gSysTickInterval;


/* Platform-specific memory map for core peripherals */
#define RK_CORE_SCB_BASE            (0xE000ED00UL)
#define RK_CORE_SYSTICK_BASE        (0xE000E010UL)
#define RK_CORE_NVIC_BASE           (0xE000E100UL)

#define RK_CORE_SVC_IRQN                 ((int)-5)
#define RK_CORE_DEBUGMON_IRQN            ((int)-4)
#define RK_CORE_PENDSV_IRQN              ((int)-2)
#define RK_CORE_SYSTICK_IRQN             ((int)-1)


void kCoreInit(void);

/* Assembly Helpers */
#define RK_DMB RK_ASM volatile("DMB" ::: "memory");
#define RK_DSB RK_ASM volatile("DSB" ::: "memory");
#define RK_ISB RK_ASM volatile("ISB" ::: "memory");
#define RK_NOP RK_ASM volatile("NOP");
#define RK_WFI RK_ASM volatile("WFI" ::: "memory");
#define RK_DIS_IRQ RK_ASM volatile("CPSID I" ::: "memory");
#define RK_EN_IRQ RK_ASM volatile("CPSIE I" ::: "memory");

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

#define RK_PEND_CTXTSWTCH \
    do                   \
    {                    \
        RK_REG_SCB_ICSR |= (1U << 28); \
    } while (0);

#define RK_STUP RK_ASM volatile("SVC #0xAA");

RK_FORCE_INLINE static inline unsigned kIsISR(void)
{
    unsigned ipsr_value;
    RK_ASM("MRS %0, IPSR" : "=r"(ipsr_value));
    return (ipsr_value);
}

RK_FORCE_INLINE
static inline unsigned __getReadyPrio(unsigned mask)
{
    unsigned result;
    RK_ASM volatile(
        "clz   %[out], %[in]      \n"
        "neg   %[out], %[out]     \n"
        "add   %[out], %[out], #31\n"
        : [out] "=&r"(result)
        : [in] "r"(mask)
        :);
    return result;
}

RK_FORCE_INLINE
static inline RK_ERR kInitStack_(UINT *const stackBufPtr, UINT const stackSize,
                                 RK_TASKENTRY const taskFunc, VOID *argsPtr)
{
    if (stackBufPtr == NULL || stackSize == 0U || taskFunc == NULL)
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
    stackBufPtr[stackSize - R11_OFFSET] = 0x11111111U;
    stackBufPtr[stackSize - R10_OFFSET] = 0x10101010U;
    stackBufPtr[stackSize - R9_OFFSET] = 0x09090909U;
    stackBufPtr[stackSize - R8_OFFSET] = 0x08080808U;
    stackBufPtr[stackSize - R7_OFFSET] = 0x07070707U;
    stackBufPtr[stackSize - R6_OFFSET] = 0x06060606U;
    stackBufPtr[stackSize - R5_OFFSET] = 0x05050505U;
    stackBufPtr[stackSize - R4_OFFSET] = 0x04040404U;

    /* stack painting */
    for (ULONG j = 17U; j < stackSize; j++)
    {
        stackBufPtr[stackSize - j] = RK_STACK_PATTERN;
    }
    stackBufPtr[0] = RK_STACK_GUARD;
    return (RK_ERR_SUCCESS);
}

#ifdef __cplusplus
}
#endif

#endif /* RK_COREDEFS_H */
