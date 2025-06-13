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

K_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
K_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
K_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)

#if (QEMU_MACHINE == lm3s6965evb)
/********** STELLARIS BOARD UART **********/

#include <stdio.h>

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
 int _write(int file, char const *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    kPutc(*ptr++);
  }
  return len;
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
/* Synchronisation Barrier */

typedef struct
{
    RK_MUTEX lock;
    RK_EVENT event;
    UINT count;        /* number of tasks in the barrier */
    UINT round;        /* increased every time all tasks synch     */
} Barrier_t;

VOID BarrierInit(Barrier_t *const barPtr)
{
    kMutexInit(&barPtr->lock, RK_INHERIT);
    kEventInit(&barPtr->event);
    barPtr->count = 0;
    barPtr->round = 0;
}

VOID BarrierWait(Barrier_t *const barPtr, UINT const nTasks)
{
    UINT myRound = 0;
    kMutexLock(&barPtr->lock, RK_WAIT_FOREVER);

    printf("%s entered the barrier \n", RK_RUNNING_NAME);

    /* save round number */
    myRound = barPtr->round;
    /* increase count on this round */
    barPtr->count++;

    if (barPtr->count == nTasks)
    {
        /* reset counter, inc round, broadcast to sleeping tasks */
        barPtr->round++;
        barPtr->count = 0;
        kCondVarBroadcast(&barPtr->event);
    }
    else
    {
        /* a proper wake signal might happen after inc round */
        while ((UINT)(barPtr->round - myRound) == 0U)
        {
            kCondVarWait(&barPtr->event, &barPtr->lock, RK_WAIT_FOREVER);
        }
    }

    kMutexUnlock(&barPtr->lock);

}


#define N_BARR_TASKS 3

Barrier_t syncBarrier;

VOID kApplicationInit(VOID)
{

    kassert(!kCreateTask(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1, STACKSIZE, 2, RK_PREEMPT));
    kassert(!kCreateTask(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2, STACKSIZE, 3, RK_PREEMPT));
    kassert(!kCreateTask(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3, STACKSIZE, 1, RK_PREEMPT));
	BarrierInit(&syncBarrier);
}
VOID Task1(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        kPuts("Task1 passed the barrier!\n\r");
		kSleep(800);

    }
}

VOID Task2(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        kPuts("Task2 passed the barrier!\n\r");
		kSleep(500);
	}
}

VOID Task3(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        kPuts("Task3 passed the barrier!\n\r");
        kSleep(300);
	}
}
