/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.1
 * Architecture     :   ARMv6m
 *
 * Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>
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
/* ARMv6-M doesn't have explicit DMB, DSB, ISB instructions  */
#define RK_DMB __ASM volatile("NOP");
#define RK_DSB __ASM volatile("NOP");
#define RK_ISB __ASM volatile("NOP");
#define RK_NOP __ASM volatile("NOP");
#define RK_STUP __ASM volatile("SVC #0xAA");
#define RK_WFI __ASM volatile("WFI" ::: "memory");
#define RK_DIS_IRQ   __ASM volatile ("CPSIE I" : : : "memory");

#define RK_EN_IRQ   __ASM volatile ("CPSID I" : : : "memory");


#define K_SET_CR(x)                                              \
    do                                                           \
    {                                                            \
        __ASM volatile("MSR primask, %0" : : "r"(x) : "memory"); \
    } while (0)

#define K_GET_CR(x)                                  \
    do                                               \
    {                                                \
        __ASM volatile("MRS %0, primask" : "=r"(x)); \
    } while (0)

/* Processor Core Management  */

RK_FORCE_INLINE
static inline ULONG kEnterCR(VOID)
{

    volatile ULONG crState = 0xAAAAAAAA;
    K_GET_CR(crState);
    if (crState == 0)
    {
        RK_DIS_IRQ
    }
    return (crState);
}

RK_FORCE_INLINE
static inline VOID kExitCR(volatile ULONG crState)
{
    K_SET_CR(crState);
    if (crState == 0)
    {
        RK_ISB
    }
}

#define RK_CR_AREA volatile ULONG RK_crState = 0xFFFFFFF;
#define RK_CR_ENTER RK_crState = kEnterCR();
#define RK_CR_EXIT kExitCR(RK_crState);

#define RK_PEND_CTXTSWTCH            \
    RK_CORE_SCB->ICSR |= (1 << 28U); \
    RK_DSB

#define K_TRAP(N)                      \
    do                                     \
    {                                      \
        __ASM volatile("svc %0" ::"i"(N)); \
    } while (0)

#define RK_TICK_EN RK_CORE_SYSTICK->CTRL |= 0x00000001;
#define RK_TICK_DIS RK_CORE_SYSTICK->CTRL &= 0xFFFFFFFE;

/* Modified for ARMv6-M (Cortex-M0) */
RK_FORCE_INLINE static inline unsigned kIsISR(void)
{
    unsigned ipsr_value;
    /* ARMv6-M compatible way to read IPSR */
    __ASM("MRS %0, IPSR" : "=r"(ipsr_value));
    RK_NOP
    return (ipsr_value);
}

/* implementing a “find-first-set” (count trailing zeros)
 * using a de bruijn multiply+LUT
 */

/* place table on ram for efficiency */
__attribute__((section(".tableGetReady"), aligned(4)))
const static unsigned table[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20,
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
    unsigned ret = (unsigned)table[idx];
    return (ret);
}

#endif /* RK_DEFS_H */
