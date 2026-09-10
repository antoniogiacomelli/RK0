/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                     */
/* VERSION: V0.80.1                                                           */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * Profiling example 05: ThreadX-style preemptive scheduling counter test.
 *
 * RK0 has a single address-space application model, so this measures a plain
 * RK0 same-space task chain.
 */

#include <kapi.h>
#include <qemu_uart.h>
#include <stdio.h>

#define TM_TEST_DURATION_MS (30000UL)
#define TM_FLAG RK_EVENT_1
#define STACKSIZE (128U)
#define PROFILE_CLASS_NAME "same-space"

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(task4Handle, Task4, stack4, STACKSIZE)
RK_DECLARE_TASK(task5Handle, Task5, stack5, STACKSIZE)
RK_DECLARE_TASK(task6Handle, Task6, stack6, STACKSIZE)

static volatile ULONG counter1;
static volatile ULONG counter2;
static volatile ULONG counter3;
static volatile ULONG counter4;
static volatile ULONG counter5;
static volatile ULONG error;
static volatile ULONG roundn;

static VOID AppCheck_(RK_ERR const err)
{
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
    {
        while (1)
        {
            kErrHandler((RK_FAULT)err);
        }
    }
}

static ULONG ProfileAbsDelta_(ULONG const value, ULONG const average)
{
    return ((value >= average) ? (value - average) : (average - value));
}

static VOID ProfileReport_(RK_TICK const time0, RK_TICK const time1)
{
    ULONG const c1 = counter1;
    ULONG const c2 = counter2;
    ULONG const c3 = counter3;
    ULONG const c4 = counter4;
    ULONG const c5 = counter5;
    ULONG const total = c1 + c2 + c3 + c4 + c5;
    ULONG const average = total / 5UL;
    ULONG maxDelta = ProfileAbsDelta_(c1, average);
    ULONG delta = ProfileAbsDelta_(c2, average);

    if (delta > maxDelta)
    {
        maxDelta = delta;
    }
    delta = ProfileAbsDelta_(c3, average);
    if (delta > maxDelta)
    {
        maxDelta = delta;
    }
    delta = ProfileAbsDelta_(c4, average);
    if (delta > maxDelta)
    {
        maxDelta = delta;
    }
    delta = ProfileAbsDelta_(c5, average);
    if (delta > maxDelta)
    {
        maxDelta = delta;
    }

    if ((c1 + 1UL < average) || (c1 > average + 1UL) ||
        (c2 + 1UL < average) || (c2 > average + 1UL) ||
        (c3 + 1UL < average) || (c3 > average + 1UL) ||
        (c4 + 1UL < average) || (c4 > average + 1UL) ||
        (c5 + 1UL < average) || (c5 > average + 1UL))
    {
        error++;
    }

    printf("\r\nRK0 PROFILE PREEMPT class=%s round=%lu elapsed_ms=%lu "
           "tick_ms=%lu c1=%lu c2=%lu c3=%lu c4=%lu c5=%lu total=%lu "
           "average=%lu max_delta=%lu errors=%lu\r\n",
           PROFILE_CLASS_NAME, roundn, time1 - time0, RK_TICK_INTERVAL_MS, c1,
           c2, c3, c4, c5, total, average, maxDelta, error);
}

#define kSuspendSelf(timeout)                                                 \
    do                                                                        \
    {                                                                         \
        AppCheck_(kEventGet(TM_FLAG, RK_OPT_EVENT_ANY, NULL, (timeout)));     \
    } while (0)

#define kResumeTask(taskHandle)                                               \
    do                                                                        \
    {                                                                         \
        AppCheck_(kEventSet((taskHandle), TM_FLAG));                          \
    } while (0)

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
    AppCheck_(kTaskInit(&task1Handle, Task1, RK_NO_ARGS, "T0", stack1,
                        STACKSIZE, 6U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task2Handle, Task2, RK_NO_ARGS, "T1", stack2,
                        STACKSIZE, 5U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task3Handle, Task3, RK_NO_ARGS, "T2", stack3,
                        STACKSIZE, 4U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task4Handle, Task4, RK_NO_ARGS, "T3", stack4,
                        STACKSIZE, 3U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task5Handle, Task5, RK_NO_ARGS, "T4", stack5,
                        STACKSIZE, 2U, RK_PREEMPT));
    AppCheck_(kTaskInit(&task6Handle, Task6, RK_NO_ARGS, "REP", stack6,
                        STACKSIZE, 1U, RK_PREEMPT));
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kResumeTask(task2Handle);
        counter1++;
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS

    kSuspendSelf(RK_WAIT_FOREVER);

    while (1)
    {
        kResumeTask(task3Handle);
        counter2++;
        kSuspendSelf(RK_WAIT_FOREVER);
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS

    kSuspendSelf(RK_WAIT_FOREVER);

    while (1)
    {
        kResumeTask(task4Handle);
        counter3++;
        kSuspendSelf(RK_WAIT_FOREVER);
    }
}

VOID Task4(VOID *args)
{
    RK_UNUSEARGS

    kSuspendSelf(RK_WAIT_FOREVER);

    while (1)
    {
        kResumeTask(task5Handle);
        counter4++;
        kSuspendSelf(RK_WAIT_FOREVER);
    }
}

VOID Task5(VOID *args)
{
    RK_UNUSEARGS

    kSuspendSelf(RK_WAIT_FOREVER);

    while (1)
    {
        counter5++;
        kSuspendSelf(RK_WAIT_FOREVER);
    }
}

VOID Task6(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_TICK const time0 = kTickGetMs();

        roundn++;
        kResumeTask(task1Handle);
        kSleep(RK_MS_TO_TICKS(TM_TEST_DURATION_MS));
        ProfileReport_(time0, kTickGetMs());
    }
}
