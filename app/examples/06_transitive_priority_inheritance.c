/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                     */
/* VERSION: V0.80.0                                                           */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * Reproduction bench:
 * Transitive priority inheritance on two mutexes with a changing donor set.
 *
 * Scenario:
 *   TL  prio 3 owns A and B.
 *   TM  prio 2 blocks on A.
 *   TH  prio 1 blocks on B forever.
 *   TH0 prio 0 blocks on B with a timeout.
 *   X   prio 2 runs as interference.
 *
 * Expected TL effective-priority sequence:
 *   3 -> 0 while TH0 and TH wait on B,
 *   0 -> 1 after TH0 times out,
 *   1 while A is unlocked before B,
 *   1 -> 3 after B is unlocked.
 */

#include <kapi.h>
#include <stdarg.h>
#include <stdio.h>

#define STACKSIZE 192U

#define TH0_PRIO 0U
#define TH_PRIO 1U
#define TM_PRIO 2U
#define X_PRIO 2U
#define TL_PRIO 3U

RK_DECLARE_TASK(th0Handle, Th0Task, th0Stack, STACKSIZE)
RK_DECLARE_TASK(thHandle, ThTask, thStack, STACKSIZE)
RK_DECLARE_TASK(tmHandle, TmTask, tmStack, STACKSIZE)
RK_DECLARE_TASK(tlHandle, TlTask, tlStack, STACKSIZE)
RK_DECLARE_TASK(xHandle, XTask, xStack, STACKSIZE)

static RK_MUTEX mutexA;
static RK_MUTEX mutexB;
static volatile UINT cycle;

static VOID BenchLog_(CHAR const *const fmtPtr, ...)
{
    va_list args;

    va_start(args, fmtPtr);
    vprintf(fmtPtr, args);
    va_end(args);
    printf("\r\n");
}

static VOID BenchStop_(VOID)
{
    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

static VOID BenchCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("PI ERR %s err=%d\r\n", wherePtr, err);
        K_ASSERT(err == RK_ERR_SUCCESS);
        BenchStop_();
    }
}

static VOID BenchExpectPrio_(RK_PRIO const expected,
                             CHAR const *const wherePtr)
{
    RK_PRIO const actual = RK_RUNNING_PRIO;

    if (actual != expected)
    {
        printf("PI FAIL %s exp=%u got=%u nom=%u\r\n", wherePtr,
               (UINT)expected, (UINT)actual, (UINT)RK_RUNNING_NOM_PRIO);
        K_ASSERT(actual == expected);
        BenchStop_();
    }
}

int main(void)
{
    kCoreInit();
    kInit();

    BenchStop_();
}

VOID kApplicationInit(VOID)
{
    BenchCheckErr_(kTaskInit(&th0Handle, Th0Task, RK_NO_ARGS, "TH0", th0Stack,
                             STACKSIZE, TH0_PRIO, RK_PREEMPT),
                   "task TH0");
    BenchCheckErr_(kTaskInit(&thHandle, ThTask, RK_NO_ARGS, "TH", thStack,
                             STACKSIZE, TH_PRIO, RK_PREEMPT),
                   "task TH");
    BenchCheckErr_(kTaskInit(&tmHandle, TmTask, RK_NO_ARGS, "TM", tmStack,
                             STACKSIZE, TM_PRIO, RK_PREEMPT),
                   "task TM");
    BenchCheckErr_(kTaskInit(&tlHandle, TlTask, RK_NO_ARGS, "TL", tlStack,
                             STACKSIZE, TL_PRIO, RK_PREEMPT),
                   "task TL");
    BenchCheckErr_(kTaskInit(&xHandle, XTask, RK_NO_ARGS, "X", xStack,
                             STACKSIZE, X_PRIO, RK_PREEMPT),
                   "task X");

    BenchCheckErr_(kMutexInit(&mutexA, RK_PRIO_INHERITANCE), "mutex A");
    BenchCheckErr_(kMutexInit(&mutexB, RK_PRIO_INHERITANCE), "mutex B");

    BenchLog_("PI bench: lower priority number means higher priority");
    BenchLog_("PI bench: TL owns A+B, TM waits A, TH/TH0 wait B");
}

VOID TlTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_ERR err = kMutexLock(&mutexA, RK_WAIT_FOREVER);
        BenchCheckErr_(err, "TL lock A");
        err = kMutexLock(&mutexB, RK_WAIT_FOREVER);
        BenchCheckErr_(err, "TL lock B");

        UINT const thisCycle = ++cycle;
        BenchLog_("PI c=%u: TL owns mutex A and B; expected TL eff=%u "
                  "actual=%u nom=%u", thisCycle, (UINT)TL_PRIO,
                  (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

        kSleep(RK_MS_TO_TICKS(100));
        BenchExpectPrio_(TH0_PRIO, "TL waiters TH0+TH");
        BenchLog_("PI c=%u: TL inherits TH0 priority through mutex B; "
                  "expected TL eff=%u actual=%u nom=%u", thisCycle,
                  (UINT)TH0_PRIO,
                  (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

        kSleep(RK_MS_TO_TICKS(300));
        BenchExpectPrio_(TH_PRIO, "TL after TH0 timeout");
        BenchLog_("PI c=%u: TH0 timed out; TL now inherits TH priority; "
                  "expected TL eff=%u actual=%u nom=%u", thisCycle,
                  (UINT)TH_PRIO,
                  (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

        err = kMutexUnlock(&mutexA);
        BenchCheckErr_(err, "TL unlock A");
        BenchExpectPrio_(TH_PRIO, "TL unlock A before B");
        BenchLog_("PI c=%u: TL unlocked mutex A; TL still inherits through B; "
                  "expected TL eff=%u actual=%u", thisCycle, (UINT)TH_PRIO,
                  (UINT)RK_RUNNING_PRIO);

        kSleep(RK_MS_TO_TICKS(100));
        BenchLog_("PI c=%u: TL unlocks mutex B", thisCycle);
        err = kMutexUnlock(&mutexB);
        BenchCheckErr_(err, "TL unlock B");
        BenchExpectPrio_(TL_PRIO, "TL restored");
        BenchLog_("PI c=%u: TL restored to nominal priority; expected TL "
                  "eff=%u actual=%u nom=%u", thisCycle, (UINT)TL_PRIO,
                  (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

        kSleep(RK_MS_TO_TICKS(500));
    }
}

VOID TmTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSleep(RK_MS_TO_TICKS(30));
        BenchLog_("PI: TM waits forever on mutex A; TM eff=%u nom=%u",
                  (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

        RK_ERR err = kMutexLock(&mutexA, RK_WAIT_FOREVER);
        BenchCheckErr_(err, "TM lock A");
        BenchLog_("PI: TM acquired mutex A; TM eff=%u", (UINT)RK_RUNNING_PRIO);

        err = kMutexUnlock(&mutexA);
        BenchCheckErr_(err, "TM unlock A");
        BenchLog_("PI: TM released mutex A; TM eff=%u", (UINT)RK_RUNNING_PRIO);

        kSleep(RK_MS_TO_TICKS(600));
    }
}

VOID ThTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSleep(RK_MS_TO_TICKS(40));
        BenchLog_("PI: TH waits forever on mutex B; TH eff=%u nom=%u",
                  (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

        RK_ERR err = kMutexLock(&mutexB, RK_WAIT_FOREVER);
        BenchCheckErr_(err, "TH lock B");
        BenchLog_("PI: TH acquired mutex B; TH eff=%u", (UINT)RK_RUNNING_PRIO);

        err = kMutexUnlock(&mutexB);
        BenchCheckErr_(err, "TH unlock B");
        BenchLog_("PI: TH released mutex B; TH eff=%u", (UINT)RK_RUNNING_PRIO);

        kSleep(RK_MS_TO_TICKS(500));
    }
}

VOID Th0Task(VOID *args)
{
    RK_UNUSEARGS

    UINT seen = 0U;

    while (1)
    {
        while (cycle == seen)
        {
            kSleep(RK_MS_TO_TICKS(10));
        }

        seen = cycle;
        BenchLog_("PI c=%u: TH0 waits on mutex B with timeout; TH0 eff=%u",
                  seen,
                  (UINT)RK_RUNNING_PRIO);

        RK_ERR const err = kMutexLock(&mutexB, RK_MS_TO_TICKS(150));
        if (err == RK_ERR_TIMEOUT)
        {
            BenchLog_("PI c=%u: TH0 timed out waiting for mutex B", seen);
        }
        else if (err == RK_ERR_SUCCESS)
        {
            (VOID)kMutexUnlock(&mutexB);
            printf("PI FAIL TH0 locked B in c=%u\r\n", seen);
            K_ASSERT(err == RK_ERR_TIMEOUT);
            BenchStop_();
        }
        else
        {
            BenchCheckErr_(err, "TH0 lock B");
        }
    }
}

VOID XTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSleep(RK_MS_TO_TICKS(50));
        kBusyDelay(RK_MS_TO_TICKS(30));
        kSleep(RK_MS_TO_TICKS(20));
        BenchLog_("PI: background task X ran");
    }
}
