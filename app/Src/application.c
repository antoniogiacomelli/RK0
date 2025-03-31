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

void uart_putchar(char c) 
{
    *UART0_DR = c;
}

void uart_print(const char *str) 
{
    while(*str) 
    {
        RK_TICK_DIS

        uart_putchar(*str++);
        
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
		uart_print("Task 1 will block\n\r");
        kMboxPend(&mbox, (ADDR*)&recvPtr, RK_WAIT_FOREVER);
		UINT recv = *recvPtr;
        uart_print("Task 1 Signalled\n\r");
		kSleepUntil(10);
    }
}



VOID Task2(VOID* args)
{
	UINT mesg = 1;
    RK_UNUSEARGS
    while(1)
    {
        uart_print("Task 2 running\r\n");
		kMboxPost(&mbox, &mesg, RK_WAIT_FOREVER);
		kSleep(20);
    }
}

VOID Task3(VOID* args)
{
    RK_UNUSEARGS
    while(1)
    {
        uart_print("Task 3 running\r\n");
        kSleep(5);
    }
}