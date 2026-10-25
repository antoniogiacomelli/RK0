/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.90.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#include <kapi.h>
#include <qemu_uart.h>


/**
 * @brief Minimal example. Task 1 has higher prio than T2.
 *  :
 *  T1 starts SUSPENDED, waiting for a pure signal (resume). 
 *  Task 2 runs, signals T1, so it is preempted in RUNNING state. 
 *  T1 writes on stdout, spins for 10ms and self-suspend again. 
 *  T2 resumes from where it left off, writes on stdout and signals 1, 
 *  being preepmpted while RUNNING.
 *  :repeat:
 */


RK_DECLARE_TASK(task1Handle, Task1, task1Stack, RK_MIN_STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, task2Stack, RK_MIN_STACKSIZE)

static VOID AppCheck_(RK_ERR const err)
{
    if (err != RK_ERR_SUCCESS)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

int main(void)
{
    kCoreInit();
    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

VOID kApplicationInit(VOID)
{
    AppCheck_(kTaskInit(&task1Handle, Task1, RK_NO_ARGS, "Task1",
                        task1Stack, RK_MIN_STACKSIZE, 1U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task2Handle, Task2, RK_NO_ARGS, "Task2",
                        task2Stack, RK_MIN_STACKSIZE, 2U, RK_PREEMPT));
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSuspend(); /* self-suspend for pure signal */
        kPuts("Task 1\r\n");
        kBusyDelay(RK_TICKS_TO_MS(10)); /* do nothing for 10 ms*/

    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kResume(task1Handle); /* signal task1 */
        kPuts("Task 2\r\n");
 
    }
}
