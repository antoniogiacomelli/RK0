/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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

K_DECLARE_TASK(task1Handle, stack1, STACKSIZE)
K_DECLARE_TASK(task2Handle, stack2, STACKSIZE)
K_DECLARE_TASK(task3Handle, stack3, STACKSIZE)

#define RK_SYSTEMCORECLOCK (50000000) /*50 MHz Core Clock*/
/*
SYSCLK/100 sets tick to 10ms
*/
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
    
    /* Create tasks */
    /* task handles and stacks are extern declared in application.h and defined in application.c */
    /* params: task handle, task name, stack addr, stack size, input args, priority, run-to-completion */

    kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, RK_NO_ARGS, 1, RK_PREEMPT);
    kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, RK_NO_ARGS, 1, RK_PREEMPT);
    kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, RK_NO_ARGS, 1, RK_PREEMPT);
    kInit();
    while(1)
    {   /* not to be here */
        asm volatile("BKPT");
        asm volatile ("cpsid i" : : : "memory");
    }
 
}
