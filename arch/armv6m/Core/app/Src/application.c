/******************************************************************************
 *
 * This RK0 v0.4.0 - ARMv6M port deployed on a STM32 NUCLEO-F030R8
 * The IDE used was STM32CubeIDE 1.17.0
 * Files for peripheral configuration and CRT were all automatically generated.
 * The only step needed was to remove the ISR Handlers for SysTick, PendSV and
 * SVC that for some reason were created not only as __weak references.
 * C Newlib is within the toolchain, and to create kprintf() was as a matter 
 * of defining the  _write backend syscall using the UART HAL.
 */

#include <application.h>

#ifdef RK_NUF030R8

#include <stm32f0xx.h>

extern UART_HandleTypeDef huart2;

int _write( int file, char *ptr, int len)
{
	(VOID) file;
	int ret = len;
	while (len)
	{
		huart2.Instance->TDR = (char) (*ptr) & 0xFF;
		while (!(huart2.Instance->ISR & UART_FLAG_TXE))
			;

		len--;
		ptr++;
	}
	return (ret);
}
extern UART_HandleTypeDef huart2;
#define kprintf(...) \
            do \
            { \
                __disable_irq();\
                printf(__VA_ARGS__); \
                __enable_irq(); \
            } while(0U)

#else
#define kprintf(...) (void)0
#endif

INT stack1[STACKSIZE]__attribute__((aligned(8)));
INT stack2[STACKSIZE]__attribute__((aligned(8)));
INT stack3[STACKSIZE]__attribute__((aligned(8)));
INT stack4[STACKSIZE]__attribute__((aligned(8)));

RK_TASK_HANDLE task1Handle;
RK_TASK_HANDLE task2Handle;
RK_TASK_HANDLE task3Handle;
RK_TASK_HANDLE task4Handle;

RK_EVENT syncEvent;
UINT syncCounter;
RK_MUTEX syncMutex;
#define SYNC_CONDITION (syncCounter>=3) /* needed tasks in the barrier */
VOID kApplicationInit( VOID)
{
	kMutexInit( &syncMutex);
	kEventInit( &syncEvent);
	syncCounter = 0;
}

static VOID synch( VOID)
{
	kMutexLock( &syncMutex, RK_NO_INHERIT, RK_WAIT_FOREVER);
	syncCounter += 1;
	if (!(SYNC_CONDITION))
	{
		/* must be atomic */
		kDisableIRQ();
		kMutexUnlock( &syncMutex);
		kEventSleep( &syncEvent, RK_WAIT_FOREVER);
		kEnableIRQ();
		kMutexLock( &syncMutex, RK_NO_INHERIT, RK_WAIT_FOREVER);
	}
	else
	{
		kprintf( "All task synch'd.\n\r");
		syncCounter = 0;
		kEventWake( &syncEvent);

	}
	kMutexUnlock( &syncMutex);
}

VOID Task1( VOID *args)
{
	RK_UNUSEARGS
	while (1)
	{
		kSleep( 4);
		kprintf( "Task 1 is synching...\n\r");
		synch();

	}
}
VOID Task2( VOID *args)
{
	RK_UNUSEARGS
	while (1)
	{
		kSleep( 8);
		kprintf( "Task 2 is synching...\n\r");
		synch();
	}
}
VOID Task3( VOID *args)
{
	RK_UNUSEARGS
	while (1)
	{
		kSleep( 4);
		kprintf( "Task 3 is synching...\n\r");
		synch();
	}
}

VOID Task4( VOID *args)
{
	RK_UNUSEARGS
	while (1)
	{
		kSleep( 400000);
	}
}

