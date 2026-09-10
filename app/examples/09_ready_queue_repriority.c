/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                     */
/* VERSION: V0.80.1                                                           */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * Regression bench:
 * Effective-priority changes must update a READY task's ready-queue position.
 *
 * H prio 1 blocks on mutex M owned by L.
 * M prio 5 is READY and would run before nominal-priority L.
 * L prio 10 owns the mutex and is READY.
 *
 * When H blocks, L inherits priority 1. If L is not removed from its old ready
 * queue and enqueued under the inherited priority, M runs first and fails the
 * bench.
 *
 * The second phase keeps L READY while H times out. L must then move back to
 * its nominal-priority ready queue before M is released.
 */

#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U

#define H_PRIO 1U
#define B_PRIO 1U
#define M_PRIO 5U
#define L_PRIO 10U

#define M_START_DELAY_TICKS RK_MS_TO_TICKS(40)
#define H_START_DELAY_TICKS RK_MS_TO_TICKS(120)
#define H_MUTEX_TIMEOUT_TICKS RK_MS_TO_TICKS(400)
#define PHASE2_MUTEX_TIMEOUT_TICKS RK_MS_TO_TICKS(120)
#define PHASE2_BLOCKER_HOLD_TICKS RK_MS_TO_TICKS(220)
#define PHASE2_SETUP_DELAY_TICKS RK_MS_TO_TICKS(80)
#define PHASE2_VERIFY_DELAY_TICKS RK_MS_TO_TICKS(60)

RK_DECLARE_TASK(hHandle, HTask, hStack, STACKSIZE)
RK_DECLARE_TASK(bHandle, BTask, bStack, STACKSIZE)
RK_DECLARE_TASK(mHandle, MTask, mStack, STACKSIZE)
RK_DECLARE_TASK(lHandle, LTask, lStack, STACKSIZE)

static RK_MUTEX mutexM;
static RK_SEMAPHORE blockerGate;
static RK_SEMAPHORE mediumGate;
static volatile UINT lOwnsMutex;
static volatile UINT mWasReady;
static volatile UINT hBlocking;
static volatile UINT lSawBoost;
static volatile UINT phase1Done;
static volatile UINT phase2Prepare;
static volatile UINT phase2LOwnsMutex;
static volatile UINT phase2HWaiting;
static volatile UINT phase2BlockerYielded;
static volatile UINT phase2TimedOut;
static volatile UINT phase2ExpectMedium;
static volatile UINT mRanAfterDeboost;
static volatile UINT hDone;

static VOID TestFaultStop_(VOID)
{
    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

static VOID TestPassStop_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

static VOID TestFail_(CHAR const *const wherePtr)
{
    printf("RQ FAIL %s t=%lu\r\n", wherePtr, kTickGetMs());
    K_ASSERT(0);
    TestFaultStop_();
}

static VOID TestCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("RQ ERR %s err=%d\r\n", wherePtr, err);
        K_ASSERT(err == RK_ERR_SUCCESS);
        TestFaultStop_();
    }
}

static VOID ExpectTaskPrio_(RK_TASK_HANDLE const taskHandle,
                            RK_PRIO const expected,
                            CHAR const *const wherePtr)
{
    RK_PRIO const actual = RK_TASK_PRIO(taskHandle);

    if (actual != expected)
    {
        printf("RQ PRIO %s exp=%u got=%u\r\n", wherePtr, (UINT)expected,
               (UINT)actual);
        TestFail_(wherePtr);
    }
}

static VOID ExpectTaskStatus_(RK_TASK_HANDLE const taskHandle,
                              RK_TASK_STATUS const expected,
                              CHAR const *const wherePtr)
{
    RK_TASK_STATUS const actual = taskHandle->status;

    if (actual != expected)
    {
        printf("RQ STATUS %s exp=%u got=%u\r\n", wherePtr, (UINT)expected,
               (UINT)actual);
        TestFail_(wherePtr);
    }
}

static VOID HoldCurrentTaskForTicks_(RK_TICK const ticks)
{
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), ticks);

    while (K_TICK_EXPIRED(deadline) == 0U)
    {
    }
}

int main(void)
{
    kCoreInit();
    kInit();

    TestFaultStop_();
}

VOID kApplicationInit(VOID)
{
    TestCheckErr_(kTaskInit(&hHandle, HTask, RK_NO_ARGS, "H", hStack,
                            STACKSIZE, H_PRIO, RK_PREEMPT),
                  "task H");
    TestCheckErr_(kTaskInit(&bHandle, BTask, RK_NO_ARGS, "B", bStack,
                            STACKSIZE, B_PRIO, RK_PREEMPT),
                  "task B");
    TestCheckErr_(kTaskInit(&mHandle, MTask, RK_NO_ARGS, "M", mStack,
                            STACKSIZE, M_PRIO, RK_PREEMPT),
                  "task M");
    TestCheckErr_(kTaskInit(&lHandle, LTask, RK_NO_ARGS, "L", lStack,
                            STACKSIZE, L_PRIO, RK_PREEMPT),
                  "task L");

    TestCheckErr_(kMutexInit(&mutexM, RK_PRIO_INHERITANCE), "mutex M");
    TestCheckErr_(kSemaBinInit(&blockerGate, 0U), "blocker gate");
    TestCheckErr_(kSemaBinInit(&mediumGate, 0U), "medium gate");

    printf("RQ bench: lower priority number means higher priority\r\n");
    printf("RQ bench: READY owner must move on boost and timeout restore\r\n");
}

VOID HTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(H_START_DELAY_TICKS);

    if ((lOwnsMutex == 0U) || (mWasReady == 0U))
    {
        TestFail_("setup incomplete");
    }

    ExpectTaskStatus_(lHandle, RK_READY, "L ready before H blocks");
    ExpectTaskStatus_(mHandle, RK_READY, "M ready before H blocks");
    ExpectTaskPrio_(lHandle, L_PRIO, "L nominal before H blocks");

    printf("RQ: before H blocks, M prio=%u is ahead of L prio=%u\r\n",
           (UINT)RK_TASK_PRIO(mHandle), (UINT)RK_TASK_PRIO(lHandle));

    hBlocking = 1U;
    RK_ERR const err = kMutexLock(&mutexM, H_MUTEX_TIMEOUT_TICKS);
    if (err != RK_ERR_SUCCESS)
    {
        printf("RQ MUTEX expected success err=%d\r\n", err);
        TestFail_("H lock M");
    }

    if (lSawBoost == 0U)
    {
        TestFail_("L boost not observed");
    }

    ExpectTaskPrio_(lHandle, L_PRIO, "L restored after handoff");
    printf("RQ: H acquired M after READY owner ran first\r\n");
    TestCheckErr_(kMutexUnlock(&mutexM), "H unlock M");
    phase1Done = 1U;

    phase2Prepare = 1U;
    kSleep(PHASE2_SETUP_DELAY_TICKS);

    if (phase2LOwnsMutex == 0U)
    {
        TestFail_("phase2 setup incomplete");
    }

    ExpectTaskStatus_(lHandle, RK_READY, "L ready before H timeout wait");
    ExpectTaskPrio_(lHandle, L_PRIO, "L nominal before H timeout wait");

    printf("RQ: H starts timeout while B keeps boosted L READY\r\n");
    TestCheckErr_(kSemaphorePost(&blockerGate), "release blocker");
    phase2HWaiting = 1U;

    RK_ERR const phase2Err = kMutexLock(&mutexM, PHASE2_MUTEX_TIMEOUT_TICKS);
    if (phase2Err != RK_ERR_TIMEOUT)
    {
        printf("RQ MUTEX timeout exp=%d got=%d\r\n", RK_ERR_TIMEOUT,
               phase2Err);
        TestFail_("H timeout M");
    }

    phase2TimedOut = 1U;
    ExpectTaskStatus_(lHandle, RK_READY, "L ready after H timeout");
    ExpectTaskPrio_(lHandle, L_PRIO, "L deboosted while READY");

    phase2ExpectMedium = 1U;
    TestCheckErr_(kSemaphorePost(&mediumGate), "release medium");
    kSleep(PHASE2_VERIFY_DELAY_TICKS);

    if (mRanAfterDeboost == 0U)
    {
        TestFail_("M did not run after L deboost");
    }

    hDone = 1U;
    printf("RQ PASS ready queue repriority\r\n");
    TestPassStop_();
}

VOID BTask(VOID *args)
{
    RK_UNUSEARGS

    TestCheckErr_(kSemaphorePend(&blockerGate, RK_WAIT_FOREVER),
                  "B wait gate");

    if (phase2HWaiting == 0U)
    {
        TestFail_("B released before H waits");
    }

    printf("RQ: B runs while H timeout restores READY L\r\n");
    HoldCurrentTaskForTicks_(PHASE2_BLOCKER_HOLD_TICKS);
    phase2BlockerYielded = 1U;
    kYield();

    TestPassStop_();
}

VOID MTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(M_START_DELAY_TICKS);
    mWasReady = 1U;
    printf("RQ: M is READY interference at prio=%u\r\n",
           (UINT)RK_RUNNING_PRIO);

    while (phase1Done == 0U)
    {
        if ((hBlocking != 0U) && (lSawBoost == 0U))
        {
            TestFail_("M ran before boosted READY owner");
        }

        kYield();
    }

    TestCheckErr_(kSemaphorePend(&mediumGate, RK_WAIT_FOREVER),
                  "M wait phase2 gate");

    if ((phase2TimedOut == 0U) || (phase2ExpectMedium == 0U))
    {
        TestFail_("M released before phase2 timeout");
    }

    mRanAfterDeboost = 1U;
    printf("RQ: M ran before deboosted L at prio=%u\r\n",
           (UINT)RK_RUNNING_PRIO);

    TestPassStop_();
}

VOID LTask(VOID *args)
{
    RK_UNUSEARGS

    TestCheckErr_(kMutexLock(&mutexM, RK_WAIT_FOREVER), "L lock M");
    lOwnsMutex = 1U;
    printf("RQ: L owns M at nominal priority %u\r\n", (UINT)RK_RUNNING_PRIO);

    while (hBlocking == 0U)
    {
        kYield();
    }

    if (RK_RUNNING_PRIO != H_PRIO)
    {
        printf("RQ PRIO L boosted exp=%u got=%u nom=%u\r\n", (UINT)H_PRIO,
               (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);
        TestFail_("L boosted");
    }

    lSawBoost = 1U;
    printf("RQ: L ran from READY queue at inherited priority %u\r\n",
           (UINT)RK_RUNNING_PRIO);
    TestCheckErr_(kMutexUnlock(&mutexM), "L unlock M");

    while (phase2Prepare == 0U)
    {
        kYield();
    }

    TestCheckErr_(kMutexLock(&mutexM, RK_WAIT_FOREVER), "L relock M");
    phase2LOwnsMutex = 1U;
    printf("RQ: L owns M again and waits READY at nominal priority %u\r\n",
           (UINT)RK_RUNNING_PRIO);

    while (phase2TimedOut == 0U)
    {
        if (phase2BlockerYielded != 0U)
        {
            TestFail_("L ran before H after READY deboost");
        }

        kYield();
    }

    while (mRanAfterDeboost == 0U)
    {
        if (phase2ExpectMedium != 0U)
        {
            TestFail_("L ran before M after READY deboost");
        }

        kYield();
    }

    TestCheckErr_(kMutexUnlock(&mutexM), "L phase2 unlock M");

    TestPassStop_();
}
