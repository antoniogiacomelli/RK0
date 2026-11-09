/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.90.3                                                          */
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
RK_EXCHANGE binarySema;

static ULONG dummyToken = 1UL;
static ULONG *recvPtr = NULL;

#define P(sema) do { kExchangePend(&sema, (VOID*)&recvPtr, RK_WAIT_FOREVER); } while(0)
#define V(sema) do { kExchangePost(&sema, &dummyToken, RK_NO_WAIT); } while(0)

VOID kApplicationInit(VOID)
{
    AppCheck_(kTaskInit(&task1Handle, Task1, RK_NO_ARGS, "Task1",
                        task1Stack, RK_MIN_STACKSIZE, 1U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task2Handle, Task2, RK_NO_ARGS, "Task2",
                        task2Stack, RK_MIN_STACKSIZE, 1U, RK_PREEMPT));

    /* on application init */
    kExchangeInit(&binarySema, &dummyToken);
}





/* usage */
/* Task1 and 2 have the same priority */
VOID Task1(VOID *args)
{
    RK_UNUSEARGS

    static ULONG *recvPtr = NULL;
    while (1)
    {
        P(binarySema);

        kPuts("Task1 got token\r\n");
        kSleep(5); /*suspend */

        V(binarySema);
        kPuts("Task1 post token\r\n");

        kSleep(1);
     }
}


VOID Task2(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {

        kSleep(5); /* will wake up and find semaphore taken */
        kPuts("Task2 Wakes\r\n");
        P(binarySema);
        kPuts("Task2 got token\r\n");
        V(binarySema);
        kPuts("Task2 post token\r\n");

     }
}

