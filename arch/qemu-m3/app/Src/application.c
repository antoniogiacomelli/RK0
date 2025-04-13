#include "application.h"

/* in this example, stacks and task handles are extern declared in application.h that is included in main.c */
RK_TASK_HANDLE task1Handle;
RK_TASK_HANDLE task2Handle;
RK_TASK_HANDLE task3Handle;


/* keep stacks double-word aligned for ARMv7M */
INT stack1[STACKSIZE] __attribute__((aligned(8)));
INT stack2[STACKSIZE] __attribute__((aligned(8)));
INT stack3[STACKSIZE] __attribute__((aligned(8)));

/* UART output for QEMU */
#ifndef UART0_BASE
#define UART0_BASE 0x4000C000
#endif
volatile unsigned * const UART0_DR = (unsigned *)UART0_BASE;

VOID kPutc(CHAR const c) 
{
    *UART0_DR = c;
}

VOID kPuts(const CHAR *str) 
{
    while(*str) 
    {
        RK_TICK_DIS

        kPutc(*str++);
        
        RK_TICK_EN
    }
}

RK_EVENT syncEvent;  
UINT syncCounter; 
RK_MUTEX syncMutex;  
#define SYNC_CONDITION (syncCounter>=3) /* needed tasks in the barrier */
VOID kApplicationInit(VOID)
{
	kMutexInit(&syncMutex);
	kEventInit(&syncEvent);
	syncCounter = 0;
}
/* only one task can be active within a monitor
 they are enqueued either on the mutex or on the event
 */
static VOID synch(VOID)
{
	kMutexLock(&syncMutex, RK_NO_INHERIT, RK_WAIT_FOREVER);
	syncCounter += 1;
	if (!(SYNC_CONDITION))
	{
	    /* must be atomic */
	    kDisableIRQ();
		kMutexUnlock(&syncMutex);
		kEventSleep(&syncEvent, RK_WAIT_FOREVER);
		kEnableIRQ();
        kMutexLock(&syncMutex, RK_NO_INHERIT, RK_WAIT_FOREVER);
	}
	else
	{
        kPuts("All task synch'd.\n\r");
        syncCounter = 0;
		kEventWake(&syncEvent);
	
    }
    kMutexUnlock(&syncMutex);
}

VOID Task1(VOID* args)
{
       RK_UNUSEARGS
	while (1)
	{
		kSleep(4);
        kPuts("Task 1 is synching...\n\r");
        synch();
        
	}
}
VOID Task2(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(8);
        kPuts("Task 2 is synching...\n\r");
        synch();
	}
}
VOID Task3(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(4);
        kPuts("Task 3 is synching...\n\r");
        synch();
	}
}
