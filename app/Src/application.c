#include "application.h"

/* make sure stacks have a global visible scope; typically, they are declared 
as extern in application.h  and defined in main.c */

/* keep stacks double-word aligned for ARMv7M */
INT stack1[STACKSIZE] __attribute__((aligned(8))); 
INT stack2[STACKSIZE] __attribute__((aligned(8)));
INT stack3[STACKSIZE] __attribute__((aligned(8)));


VOID kApplicationInit(VOID)
{
	/* kernel objects are initialised here */
}

VOID Task3( VOID* args)
{
    RK_UNUSEARGS
	while(1)
	{
		kSleep(1);
	}
}

VOID Task2( VOID* args)
{
    RK_UNUSEARGS
	while(1)
	{
		kSleep(1);
	}
}

VOID Task1( VOID* args)
{
    RK_UNUSEARGS
	while(1)
	{
		kSleep(1);
	}
}
