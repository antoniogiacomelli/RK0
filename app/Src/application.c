#include "application.h"

INT stack1[STACKSIZE];
INT stack2[STACKSIZE];
INT stack3[STACKSIZE];


VOID kApplicationInit(VOID)
{
}

VOID Task3(VOID)
{
	while(1)
	{
		kSleep(1);

	}
}


VOID Task2(VOID)
{
	while(1)
	{
		kSleep(1);

	}

}


VOID Task1(VOID)
{
	while(1)
	{
		kSleep(1);
	}
}
