/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.1
 * Architecture     :   ARMv7m
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

/* Assembly Helpers */
#define RK_DMB __ASM volatile("dmb 0xF" :: : "memory");
#define RK_DSB __ASM volatile("dsb 0xF" :: : "memory");
#define RK_ISB __ASM volatile("isb 0xF" :: : "memory");
#define RK_NOP __ASM volatile("nop");
#define RK_STUP __ASM volatile("svc #0xAA");
#define RK_WFI __ASM volatile("wfi" :: : "memory");
#define RK_DIS_IRQ __ASM volatile("CPSID I");
#define RK_EN_IRQ __ASM volatile("CPSIE I");

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

#define K_TRAP_SVC(N)                      \
    do                                     \
    {                                      \
        __ASM volatile("svc %0" ::"i"(N)); \
    } while (0)

#define RK_TICK_EN RK_CORE_SYSTICK->CTRL |= 0xFFFFFFFF;
#define RK_TICK_DIS RK_CORE_SYSTICK->CTRL &= 0xFFFFFFFE;
#define RK_TICK_MASK RK_CORE_SYSTICK->CTRL &= ~(1UL << 1U);
#define RK_TICK_UNMASK RK_CORE_SYSTICK->CTRL |= (1UL << 1U);

RK_FORCE_INLINE static inline unsigned kIsISR(void)
{
    unsigned ipsr_value;
    __ASM("MRS %0, IPSR" : "=r"(ipsr_value));
    __ASM volatile("dmb 0xF" :: : "memory");
    return (ipsr_value);
}
RK_FORCE_INLINE
static inline unsigned __getReadyPrio(unsigned mask)
{
    unsigned result;
    __ASM volatile(
        "clz   %[out], %[in]      \n"
        "neg   %[out], %[out]     \n"
        "add   %[out], %[out], #31\n" : [out] "=&r"(result) : [in] "r"(mask) :);
    return result;
}

#endif
