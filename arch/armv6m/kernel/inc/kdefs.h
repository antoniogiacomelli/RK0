/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
 * Architecture     :   ARMv6m
 *
 * Copyright (C) 2025 Antonio Giacomelli
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

#include <kservices.h>

/* Assembly Helpers - ARMv6-M (Cortex-M0) compatible versions */
/* ARMv6-M doesn't have explicit DMB, DSB, ISB instructions, use memory barriers */
#define _RK_DMB                          __asm volatile("nop");
#define _RK_DSB                          __asm volatile("nop");
#define _RK_ISB                          __asm volatile("nop");
#define _RK_NOP                          __asm volatile("nop");
#define _RK_STUP                         __asm volatile("svc #0xAA");

/* Processor Core Management */
#define RK_CR_AREA    volatile UINT state;
#define RK_CR_ENTER   do { state = __get_PRIMASK(); __disable_irq(); } while(0);
#define RK_CR_EXIT    do { __set_PRIMASK(state); } while(0);

#define RK_PEND_CTXTSWTCH RK_TRAP_PENDSV
#define RK_TRAP_PENDSV  \
     RK_CORE_SCB->ICSR |= (1<<28U); \

#define K_TRAP_SVC(N)  \
    do { __asm volatile ("svc %0" :: "i" (N)); } while(0)

#define RK_TICK_EN  RK_CORE_SYSTICK->CTRL |= 0x00000001;
#define RK_TICK_DIS RK_CORE_SYSTICK->CTRL &= 0xFFFFFFFE;

/* Modified for ARMv6-M (Cortex-M0) */
__RK_INLINE static inline
unsigned kIsISR( void)
{
	unsigned ipsr_value;
	/* ARMv6-M compatible way to read IPSR */
	__asm ("MRS %0, IPSR" : "=r"(ipsr_value));
	__asm volatile ("" ::: "memory");
	/* Memory barrier */
	return (ipsr_value);
}
__RK_INLINE
static inline unsigned __getReadyPrio(unsigned x)
{
    /* implementing a “find-first-set” (count trailing zeros)
     * using a de bruijn multiply-and-table trick from Hacker’s Delight
     * book
     */

    /* mark “unused” table slots with 0xFF so we can spot any logic errors. */
    #define u (0xFFU)

    /* after the multiply+shift below,
     * we’ll get a 6-bit index in 0...63.  Only 32 of those indices
     * ever occur for a one-hot x. Note that the input x one-hot
     * as it has been already AND'd to its 2's complement
     * Each valid slot holds the bit-position (0–31)
     * that is the highest priority index on the ready queue table
     */
    static const unsigned char table[64] = 
    {
        /*  0– 7 */  32,   0,   1,  12,   2,   6,   u,  13,
        /*  8–15 */   3,   u,   7,   u,   u,   u,   u,  14,
        /* 16–23 */  10,   4,   u,   u,   8,   u,   u,  25,
        /* 24–31 */   u,   u,   u,   u,   u,  21,  27,  15,
        /* 32–39 */  31,  11,   5,   u,   u,   u,   u,   u,
        /* 40–47 */   9,   u,   u,  24,   u,   u,  20,  26,
        /* 48–55 */  30,   u,   u,   u,   u,  23,   u,  19,
        /* 56–63 */  29,   u,  22,  18,  28,  17,  16,   u
    };

    /*
     * for modulo-2^32 arithmetic, this “rotates” the single 1 in x
     * into a unique 6-bit pattern in the 'highest' bits of the result
     */
    x = x * 0x0450FBAFU; 

    /* Shift right the top 6 bits
     */
    unsigned idx = (x >> 26);

    /* LUT */
    unsigned ret = (unsigned)table[idx];
    kassert(ret < 32); /* ret must be within [0,31]*/
    return (ret);

/*Bruijn's algorithm from  Warren, Henry S.. Hacker's Delight (p. 183). Pearson Education. Kindle Edition.  */
}

#endif /* RK_DEFS_H */
