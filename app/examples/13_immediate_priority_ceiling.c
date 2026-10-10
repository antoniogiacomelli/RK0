/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                    */
/* VERSION: V0.90.1                                                           */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * Immediate Priority Ceiling Protocol regression.
 *
 * L locks A and then B. H uses the opposite lock order, B then A, and becomes
 * ready while L owns A. A and B both have H's priority as their ceiling, so L
 * continues at that priority until both shared mutexes are released. H cannot
 * interleave the opposite acquisition order and form a circular wait.
 *
 * An outer mutex with a lower ceiling also verifies nested priority restoration:
 * L runs at 5 while owning outer, at 2 while owning A/B, then at 5 again after
 * releasing A/B, and finally at its nominal priority 10.
 */

#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U

#define H_PRIO 2U
#define OUTER_CEILING_PRIO 5U
#define L_PRIO 10U

#define H_START_TICKS RK_MS_TO_TICKS(20)
#define HOLD_TICKS RK_MS_TO_TICKS(60)

RK_DECLARE_TASK(hHandle, HTask, hStack, STACKSIZE)
RK_DECLARE_TASK(lHandle, LTask, lStack, STACKSIZE)

static RK_MUTEX mutexA;
static RK_MUTEX mutexB;
static RK_MUTEX outerMutex;

static volatile UINT lowCompletedPair;
static volatile UINT highStarted;
static volatile UINT highCompleted;

static VOID Stop_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

static VOID Fail_(CHAR const *const wherePtr)
{
    printf("PC FAIL %s t=%lu\r\n", wherePtr, kTickGetMs());
    K_ASSERT(0);
    Stop_();
}

static VOID Check_(RK_BOOL const condition, CHAR const *const wherePtr)
{
    if (condition == RK_FALSE)
    {
        Fail_(wherePtr);
    }
}

static VOID CheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("PC ERR %s err=%d\r\n", wherePtr, err);
        Fail_(wherePtr);
    }
}

static VOID CheckRunningPrio_(RK_PRIO const expected,
                              CHAR const *const wherePtr)
{
    if (RK_RUNNING_PRIO != expected)
    {
        printf("PC PRIO %s exp=%u got=%u\r\n", wherePtr,
               (UINT)expected, (UINT)RK_RUNNING_PRIO);
        Fail_(wherePtr);
    }
}

int main(void)
{
    kCoreInit();
    kInit();

    Stop_();
}

VOID kApplicationInit(VOID)
{
    CheckErr_(kTaskInit(&hHandle, HTask, RK_NO_ARGS, "PCH", hStack,
                        STACKSIZE, H_PRIO, RK_PREEMPT),
              "task H");
    CheckErr_(kTaskInit(&lHandle, LTask, RK_NO_ARGS, "PCL", lStack,
                        STACKSIZE, L_PRIO, RK_PREEMPT),
              "task L");

    CheckErr_(kMutexInit(&mutexA, RK_PRIO_CEILING, H_PRIO), "mutex A");
    CheckErr_(kMutexInit(&mutexB, RK_PRIO_CEILING, H_PRIO), "mutex B");
    CheckErr_(kMutexInit(&outerMutex, RK_PRIO_CEILING,
                         OUTER_CEILING_PRIO),
              "outer mutex");
}

VOID HTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(H_START_TICKS);
    highStarted = 1U;

    Check_(lowCompletedPair != 0U, "H ran inside L shared section");

    CheckErr_(kMutexLock(&mutexB, RK_WAIT_FOREVER), "H lock B");
    CheckErr_(kMutexLock(&mutexA, RK_WAIT_FOREVER), "H lock A");
    CheckRunningPrio_(H_PRIO, "H owns A and B");
    CheckErr_(kMutexUnlock(&mutexA), "H unlock A");
    CheckErr_(kMutexUnlock(&mutexB), "H unlock B");

    highCompleted = 1U;
    Stop_();
}

VOID LTask(VOID *args)
{
    RK_UNUSEARGS

    CheckRunningPrio_(L_PRIO, "L nominal");
    CheckErr_(kMutexLock(&outerMutex, RK_WAIT_FOREVER), "L lock outer");
    CheckRunningPrio_(OUTER_CEILING_PRIO, "L outer ceiling");

    CheckErr_(kMutexLock(&mutexA, RK_WAIT_FOREVER), "L lock A");
    CheckRunningPrio_(H_PRIO, "L shared ceiling");

    RK_TICK const deadline = K_TICK_ADD(kTickGet(), HOLD_TICKS);
    while (K_TICK_EXPIRED(deadline) == 0U)
    {
        RK_COMPILER_BARRIER
    }

    Check_(highStarted == 0U, "H preempted ceiling owner");
    CheckErr_(kMutexLock(&mutexB, RK_WAIT_FOREVER), "L lock B");
    CheckRunningPrio_(H_PRIO, "L nested shared ceilings");
    CheckErr_(kMutexUnlock(&mutexB), "L unlock B");
    CheckRunningPrio_(H_PRIO, "L retains A ceiling");

    lowCompletedPair = 1U;
    CheckErr_(kMutexUnlock(&mutexA), "L unlock A");

    Check_(highCompleted != 0U, "H did not complete opposite order");
    CheckRunningPrio_(OUTER_CEILING_PRIO, "L restored outer ceiling");
    CheckErr_(kMutexUnlock(&outerMutex), "L unlock outer");
    CheckRunningPrio_(L_PRIO, "L restored nominal");

    printf("PC PASS immediate priority ceiling\r\n");
    Stop_();
}
