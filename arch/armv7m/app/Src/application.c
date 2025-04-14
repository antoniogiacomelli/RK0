#include "application.h"

/* in this example, stacks and task handles are extern declared in application.h that should be included in main.c */
RK_TASK_HANDLE task1Handle;
RK_TASK_HANDLE task2Handle;
RK_TASK_HANDLE task3Handle;


/* keep stacks double-word aligned for ARMv7M */
INT stack1[STACKSIZE] __attribute__((aligned(8)));
INT stack2[STACKSIZE] __attribute__((aligned(8)));
INT stack3[STACKSIZE] __attribute__((aligned(8)));


/* this is to initialise any kernel object, mandatory even if empty */
VOID kApplicationInit(VOID)
{
	
}


VOID Task1(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(20);    
	}
}

VOID Task2(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(30);
	}
}
VOID Task3(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
		kSleep(40);
	}
}
