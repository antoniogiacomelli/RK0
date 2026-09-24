/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.83.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#include <kapi.h>
#include <qemu_uart.h>


/**
 * @brief Minimal example. Task 1 has higher prio than 2
 *  but sleep for 1.5x more. Thus, Task 2 must run
 *  only while Task 1 is sleeping.
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
        AppCheck_(kSleep(RK_MS_TO_TICKS(750UL)));
        kPuts("Task 1\r\n");

    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        AppCheck_(kSleep(RK_MS_TO_TICKS(500UL)));
        kPuts("Task 2\r\n");

    }
}
