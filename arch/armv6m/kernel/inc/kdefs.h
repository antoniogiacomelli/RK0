/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.5
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
/* ARMv6-M doesn't have explicit DMB, DSB, ISB instructions  */
#define __RK_DMB                          __asm volatile("nop");
#define __RK_DSB                          __asm volatile("nop");
#define __RK_ISB                          __asm volatile("nop");
#define __RK_NOP                          __asm volatile("nop");
#define __RK_STUP                         __asm volatile("svc #0xAA");
 
__RK_INLINE
static inline UINT kEnterCR( VOID)
{

 	UINT crState;
	crState = __get_PRIMASK();
	if (crState == 0)
	{
         asm volatile("CPSID I");
  		return (crState);
	}
    return (crState);
}
__RK_INLINE
static inline VOID kExitCR( UINT crState)
{
     __set_PRIMASK( crState);
     if (crState == 0)
     {
        __RK_ISB
     }
 }


/* Processor Core Management */

#define RK_CR_AREA  unsigned crState_;
#define RK_CR_ENTER crState_ = kEnterCR();
#define RK_CR_EXIT  kExitCR(crState_);

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
    __RK_NOP
    return (ipsr_value);
}

/* implementing a “find-first-set” (count trailing zeros)
* using a de bruijn multiply+LUT
*/

/* place table on ram for efficiency */
__attribute__((section(".tableGetReady"), aligned(4)))
const static unsigned table[32] = 
{
 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

__RK_INLINE
static inline 
unsigned __getReadyPrio(unsigned readyQBitmask)
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
