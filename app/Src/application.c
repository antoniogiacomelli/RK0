#include "application.h"

/* in this example, stacks and task handles are extern declared in application.h that is included in main.c */
RK_TASK_HANDLE task1Handle;
RK_TASK_HANDLE task2Handle;
RK_TASK_HANDLE task3Handle;

RK_MBOX mbox;

/* keep stacks double-word aligned for ARMv7M */
INT stack1[STACKSIZE] __attribute__((aligned(8)));
INT stack2[STACKSIZE] __attribute__((aligned(8)));
INT stack3[STACKSIZE] __attribute__((aligned(8)));

/* UART output for QEMU */
#ifndef UART0_BASE
#define UART0_BASE 0x4000C000
#endif
volatile unsigned int * const UART0_DR = (unsigned int *)UART0_BASE;

void kPutc(CHAR c) 
{
    *UART0_DR = c;
}

void kPuts(const CHAR *str) 
{
    while(*str) 
    {
        RK_TICK_DIS

        kPutc(*str++);
        
        RK_TICK_EN
    }
}

VOID kApplicationInit(VOID)
{
  kMboxInit(&mbox, NULL);
}

UINT mail;
VOID Task1(VOID* args)
{
    RK_UNUSEARGS
	UINT* recvPtr;
    while(1)
    {
		kPuts("Task 1 will block\n\r");
        kMboxPend(&mbox, (ADDR*)&recvPtr, RK_WAIT_FOREVER);
		UINT recv = *recvPtr;
		UNUSED(recv);
        kPuts("Task 1 Signalled\n\r");
		kSleepUntil(33);
     }
}



VOID Task2(VOID* args)
{
	UINT mesg = 1;
    RK_UNUSEARGS
    while(1)
    {
        kPuts("Task 2 running\r\n");
		kMboxPost(&mbox, &mesg, RK_WAIT_FOREVER);
		kSleepUntil(17);
     }
}

VOID Task3(VOID* args)
{
    RK_UNUSEARGS
    while(1)
    {
		kPuts("Nobody cares about Task3...\n\r");
    	RK_ERR err = kPend(39);
	   	if (err==RK_ERR_TIMEOUT)
	    	kPuts("Task3 timed-out, alone.\n\r");
	   kSleep(15);
    }
}