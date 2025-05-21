/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
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

/* this is for printf */
int __io_putchar(int ch) 
{
	kPutc((CHAR const)ch); 
    return (0);
} 

/* weakly declared on syscalls.c */
int _write(int file, char *ptr, int len)
{
  (void)file;
  int idx;

  for (idx = 0; idx < len; idx++)
  {
	__io_putchar(*ptr++);
  }
  return (len);
}

/********** APPLICATION: SYNCHRONISATION BARRIER **********/

/* in this example, stacks and task handles are extern declared in application.h that is included in main.c */
RK_TASK_HANDLE task1Handle;
RK_TASK_HANDLE task2Handle;
RK_TASK_HANDLE task3Handle;

/* keep stacks double-word aligned for ARMv6/7M */
INT stack1[STACKSIZE] __K_ALIGN(8);
INT stack2[STACKSIZE] __K_ALIGN(8);
INT stack3[STACKSIZE] __K_ALIGN(8);


/* Kernel objects */

RK_EVENT syncEvent;  
RK_MUTEX syncMutex;  

UINT syncCounter; 

/* Initialise kernel objects - this function definition is mandatory, even if empty */
VOID kApplicationInit(VOID)
{
	kMutexInit(&syncMutex);
	kEventInit(&syncEvent);
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
		printf("%s wakes all tasks...\n\r", RK_RUNNING_NAME);
		kEventWake(&syncEvent);
	
    }
	printf("%s synch'd \n\r", RK_RUNNING_NAME);
    kMutexUnlock(&syncMutex);
}



VOID Task1(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(4);
        printf("Task 1 is synching...\n\r");
		kBusyWait(100);
		synch();
        
	}
}
VOID Task2(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(8);
        printf("Task 2 is synching...\n\r");
		kBusyWait(100);
		synch();
	}
}
VOID Task3(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(4);
        printf("Task 3 is synching...\n\r");
		kBusyWait(100);
		synch();
	}
}