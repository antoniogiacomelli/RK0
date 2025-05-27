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

#define STACKSIZE 128

K_DECLARE_TASK(task1Handle, stack1, STACKSIZE)
K_DECLARE_TASK(task2Handle, stack2, STACKSIZE)
K_DECLARE_TASK(task3Handle, stack3, STACKSIZE)

#if (QEMU_MACHINE == lm3s6965evb)
/********** STELLARIS BOARD UART **********/

#ifndef UART0_BASE
#define UART0_BASE 0x4000C000
#define UART0_DR  (*(volatile unsigned *)(UART0_BASE + 0x00)) /* Data register */
#define UART0_FR  (*(volatile unsigned *)(UART0_BASE + 0x18)) /* Fifo register */
#define UART0_FR_TXFF (1U << 5)   /* FIFO Full */
#endif
 
static inline VOID kPutc(CHAR const c) 
{
	while (UART0_FR & UART0_FR_TXFF)
        ;
    UART0_DR = c;
}

VOID kPuts(const CHAR *str) 
{
    while(*str) 
    {
        kPutc(*str++);
    }
}
#else
static inline VOID kPutc(CHAR const c) 
{
	(VOID)c;
	return;
}

VOID kPuts(const CHAR *str) 
{
	(VOID)str;
	return;
}
#endif

/********** APPLICATION: SYNCHRONISATION BARRIER **********/

 
/* Kernel objects */

RK_EVENT syncEvent;  
RK_MUTEX syncMutex;  

/******/

UINT syncCounter; 

 
VOID kApplicationInit(VOID)
{
	
    kassert(!kCreateTask(&task1Handle, Task1, "Task1", stack1, STACKSIZE, RK_NO_ARGS, 1, RK_PREEMPT));
    kassert(!kCreateTask(&task2Handle, Task2, "Task2", stack2, STACKSIZE, RK_NO_ARGS, 1, RK_PREEMPT));
    kassert(!kCreateTask(&task3Handle, Task3, "Task3", stack3, STACKSIZE, RK_NO_ARGS, 1, RK_PREEMPT));
	kassert(!kMutexInit(&syncMutex));
	kassert(!kEventInit(&syncEvent));
	syncCounter = 0;
}

#define SYNC_CONDITION (3) /* needed tasks in the barrier */

static VOID synch(VOID)
{
	kMutexLock(&syncMutex, RK_INHERIT, RK_WAIT_FOREVER);
	syncCounter += 1;
	if (syncCounter < SYNC_CONDITION)
	{
	    /* must be atomic */
	    kDisableIRQ();
		kMutexUnlock(&syncMutex);
		kEventSleep(&syncEvent, RK_WAIT_FOREVER);
		kEnableIRQ();
        kMutexLock(&syncMutex, RK_INHERIT, RK_WAIT_FOREVER);
	}
	else
	{  
        syncCounter = 0;
 		kEventWake(&syncEvent);
		kPuts("All synch'd\n\r");

    }
     kMutexUnlock(&syncMutex);
}



VOID Task1(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
  		kSleep(50);
		kPuts("Task 1 synchs\n\r");
		synch();
        
	}
}
VOID Task2(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
  		kSleep(100);
		kPuts("Task 2 synchs\n\r");
		synch();
	}
}
VOID Task3(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
  		kSleep(150);
		kPuts("Task 3 synchs\n\r");
		synch();
	}
}