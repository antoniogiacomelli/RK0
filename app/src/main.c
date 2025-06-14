/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.0
 * Architecture     :   ARMv6/7m
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

#include <application.h>


#define RK_SYSTEMCORECLOCK (50000000) /*50 MHz Core Clock*/

/* 1ms SysTick*/
#define RK_CORE_INIT() \
do { \
    kCoreSysTickConfig( RK_SYSTEMCORECLOCK/1000); \
    kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x06); \
    kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x07); \
    kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x07); \
} while(0)

int main(void)
{
    
    /* initialise systick and needed core interrupts */
    RK_CORE_INIT();
    
    kInit();

    while(1)
    {   /* not to be here */
        asm volatile("BKPT #0");
        asm volatile ("cpsid i" : : : "memory");
    }
 
}
