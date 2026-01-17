/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.5
 * Architecture     :   ARMv6m
 *
 * Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#ifndef RK_DEFS_H
#define RK_DEFS_H

#include <kexecutive.h>

/* Assembly Helpers - ARMv6-M (Cortex-M0) compatible versions */
#define RK_DMB __ASM volatile("DMB" ::: "memory");
#define RK_DSB __ASM volatile("DSB" ::: "memory");
#define RK_ISB __ASM volatile("ISB" ::: "memory");
#define RK_NOP __ASM volatile("NOP");
#define RK_STUP __ASM volatile("SVC #0xAA" ::: "memory");
#define RK_WFI __ASM volatile("WFI" ::: "memory");
#define RK_DIS_IRQ   __ASM volatile ("CPSID I" : : : "memory");
#define RK_EN_IRQ   __ASM volatile ("CPSIE I" : : : "memory");

RK_FORCE_INLINE
static inline unsigned kEnterCR(void)
{
    unsigned state;
    __ASM volatile ("MRS %0, PRIMASK ": "=r" (state));
    __ASM volatile("":::"memory");
    RK_DIS_IRQ
    return (state);
}


RK_FORCE_INLINE
static inline void kExitCR(unsigned state)
{
    __ASM volatile ("MSR PRIMASK, %0": : "r" (state) : "memory");
}

#define RK_CR_AREA  unsigned RK_crState;
#define RK_CR_ENTER RK_crState = kEnterCR();
#define RK_CR_EXIT kExitCR(RK_crState);

#define RK_PEND_CTXTSWTCH  \
    RK_CORE_SCB->ICSR |= (1 << 28U); 

#define K_TRAP(N)                      \
    do                                     \
    {                                      \
        __ASM volatile("svc %0" ::"i"(N)); \
    } while (0)



#define RK_HW_REG(addr)         *((volatile unsigned long*)(addr))
#define RK_REG_SCB_ICSR         RK_HW_REG(0xE000ED04)
#define RK_REG_SYSTICK_CTRL     RK_HW_REG(0xE000E010) 
#define RK_REG_SYSTICK_LOAD     RK_HW_REG(0xE000E014) 
#define RK_REG_SYSTICK_VAL      RK_HW_REG(0xE000E018) 
#define RK_REG_NVIC             RK_HW_REG(0xE000E100)   
#define RK_PEND_CTXTSWTCH do { RK_REG_SCB_ICSR |= (1<<28); } while(0);

/* Modified for ARMv6-M (Cortex-M0) */
RK_FORCE_INLINE static inline unsigned kIsISR(void)
{
    unsigned ipsr_value;
    __ASM("MRS %0, IPSR" : "=r"(ipsr_value));
    return (ipsr_value);
}

/* implementing a “find-first-set” (count trailing zeros)
 * using a de bruijn multiply+LUT
 */

/* place table on ram for efficiency */
//__attribute__((section(".tableGetReady"), aligned(4)))
static const unsigned RK_getReadyTbl[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20,
                                   15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19,
                                   16, 7, 26, 12, 18, 6, 11, 5, 10, 9};

RK_FORCE_INLINE
static inline unsigned __getReadyPrio(unsigned readyQBitmask)
{
    readyQBitmask = readyQBitmask * 0x077CB531U;

    /* Shift right the top 5 bits
     */
    unsigned idx = (readyQBitmask >> 27);

    /* LUT */
    unsigned ret = (unsigned)RK_getReadyTbl[idx];
    return (ret);
}


RK_FORCE_INLINE
static inline RK_ERR kInitStack_(UINT *const stackBufPtr, UINT const stackSize,
        RK_TASKENTRY const taskFunc, VOID *argsPtr)
{

    if (stackBufPtr == NULL || stackSize == 0 || taskFunc == NULL)
    {
        return (RK_ERR_ERROR);
    }
    stackBufPtr[stackSize - PSR_OFFSET] = 0x01000000;
    stackBufPtr[stackSize - PC_OFFSET] = (UINT)taskFunc;
    stackBufPtr[stackSize - LR_OFFSET] = 0x14141414;
    stackBufPtr[stackSize - R12_OFFSET] = 0x12121212;
    stackBufPtr[stackSize - R3_OFFSET] = 0x03030303;
    stackBufPtr[stackSize - R2_OFFSET] = 0x02020202;
    stackBufPtr[stackSize - R1_OFFSET] = 0x01010101;
    stackBufPtr[stackSize - R0_OFFSET] = (UINT)(argsPtr);

    /* armv6m sequence expects R8–R11 first, then R4–R7. */
    stackBufPtr[stackSize - R4_OFFSET] = 0x08080808; /* R8 */
    stackBufPtr[stackSize - R5_OFFSET] = 0x09090909; /* R9 */
    stackBufPtr[stackSize - R6_OFFSET] = 0x10101010; /* R10 */
    stackBufPtr[stackSize - R7_OFFSET] = 0x11111111; /* R11 */
    stackBufPtr[stackSize - R8_OFFSET] = 0x04040404; /* R4 */
    stackBufPtr[stackSize - R9_OFFSET] = 0x05050505; /* R5 */
    stackBufPtr[stackSize - R10_OFFSET] = 0x06060606; /* R6 */
    stackBufPtr[stackSize - R11_OFFSET] = 0x07070707; /* R7 */
    /*stack painting*/
    for (ULONG j = 17; j < stackSize; j++)
    {
        stackBufPtr[stackSize - j] = RK_STACK_PATTERN;
    }
    stackBufPtr[0] = RK_STACK_GUARD;
    return (RK_ERR_SUCCESS);
}



#endif /* RK_DEFS_H */
