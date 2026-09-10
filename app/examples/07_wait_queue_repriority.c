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
 * Effective-priority changes must update a task's current wait-queue position.
 *
 * H prio 2 blocks on mutex M owned by L.
 * W prio 5 waits forever on semaphore S.
 * L prio 10 owns M, then waits on S behind W.
 *
 * When H blocks on M, L inherits priority 2 while already queued on S. S is not
 * a priority-inheritance object, but its wait queue must still be ordered by
 * the waiters' effective priorities.
 */

#include <kapi.h>
#include <klist.h>
#include <stdio.h>

#define STACKSIZE 192U

#define H_PRIO 2U
#define W_PRIO 5U
#define L_PRIO 10U

#define SETUP_SLEEP_TICKS RK_MS_TO_TICKS(50)
#define OBSERVE_DELAY_TICKS RK_MS_TO_TICKS(40)
#define H_MUTEX_TIMEOUT_TICKS RK_MS_TO_TICKS(160)
#define L_SEMA_TIMEOUT_TICKS RK_MS_TO_TICKS(260)

#define MODE_TIMEOUT_RESTORE 1U
#define MODE_BOOSTED_POST 2U
#define MODE_L_FINITE_TIMEOUT 3U

RK_DECLARE_TASK(hHandle, HTask, hStack, STACKSIZE)
RK_DECLARE_TASK(wHandle, WTask, wStack, STACKSIZE)
RK_DECLARE_TASK(lHandle, LTask, lStack, STACKSIZE)

static RK_MUTEX mutexM;
static RK_SEMAPHORE semaS;
static RK_TIMER observerTimer;

static volatile UINT testCycle;
static volatile UINT testMode;
static volatile UINT observerCycle;
static volatile UINT observerPostS;
static volatile UINT observerDoneCycle;
static volatile UINT wDoneCycle;
static volatile UINT lDoneCycle;
static volatile UINT lSemaTimeoutCycle;

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
    printf("WQ FAIL %s t=%lu c=%u mode=%u\r\n", wherePtr, kTickGetMs(),
           testCycle, testMode);
    K_ASSERT(0);
    TestFaultStop_();
}

static VOID TestCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("WQ ERR %s err=%d\r\n", wherePtr, err);
        K_ASSERT(err == RK_ERR_SUCCESS);
        TestFaultStop_();
    }
}

static RK_TASK_HANDLE SemaWaitAt_(UINT const index)
{
    RK_TASK_HANDLE ret = NULL;
    RK_CR_AREA
    RK_CR_ENTER

    if (index < semaS.waitingQueue.size)
    {
        RK_NODE *nodePtr = semaS.waitingQueue.listDummy.nextPtr;

        for (UINT i = 0U; i < index; i++)
        {
            nodePtr = nodePtr->nextPtr;
        }
        ret = K_GET_TCB_ADDR(nodePtr);
    }

    RK_CR_EXIT
    return (ret);
}

static VOID ExpectSemaOrder_(RK_TASK_HANDLE const first,
                             RK_TASK_HANDLE const second,
                             CHAR const *const wherePtr)
{
    RK_TASK_HANDLE const actualFirst = SemaWaitAt_(0U);
    RK_TASK_HANDLE const actualSecond = SemaWaitAt_(1U);

    if ((actualFirst != first) || (actualSecond != second))
    {
        printf("WQ ORDER %s first=%s second=%s\r\n", wherePtr,
               (actualFirst != NULL) ? actualFirst->taskName : "NULL",
               (actualSecond != NULL) ? actualSecond->taskName : "NULL");
        TestFail_(wherePtr);
    }
}

static VOID ExpectSemaHead_(RK_TASK_HANDLE const head,
                            CHAR const *const wherePtr)
{
    RK_TASK_HANDLE const actualHead = SemaWaitAt_(0U);

    if (actualHead != head)
    {
        printf("WQ HEAD %s head=%s\r\n", wherePtr,
               (actualHead != NULL) ? actualHead->taskName : "NULL");
        TestFail_(wherePtr);
    }
}

static VOID ExpectTaskPrio_(RK_TASK_HANDLE const taskHandle,
                            RK_PRIO const expected,
                            CHAR const *const wherePtr)
{
    RK_PRIO const actual = RK_TASK_PRIO(taskHandle);

    if (actual != expected)
    {
        printf("WQ PRIO %s exp=%u got=%u\r\n", wherePtr, (UINT)expected,
               (UINT)actual);
        TestFail_(wherePtr);
    }
}

static VOID PostExpectHead_(RK_TASK_HANDLE const expectedHead,
                            CHAR const *const wherePtr)
{
    ExpectSemaHead_(expectedHead, wherePtr);
    TestCheckErr_(kSemaphorePost(&semaS), wherePtr);
}

static VOID ObserverCb_(VOID *args)
{
    RK_UNUSEARGS

    UINT const cycle = observerCycle;

    if (cycle == 0U)
    {
        return;
    }

    ExpectTaskPrio_(lHandle, H_PRIO, "observer L boosted");
    ExpectSemaOrder_(lHandle, wHandle, "observer L,W");

    if (observerPostS != 0U)
    {
        PostExpectHead_(lHandle, "observer post boosted");
        ExpectSemaHead_(wHandle, "observer after post");
    }

    observerDoneCycle = cycle;
}

static VOID ArmObserver_(UINT const cycle, UINT const postS)
{
    observerCycle = cycle;
    observerPostS = postS;

    if (cycle > 1U)
    {
        kTimerReload(&observerTimer, OBSERVE_DELAY_TICKS);
    }
}

static VOID WaitForTwoSemaWaiters_(VOID)
{
    while (semaS.waitingQueue.size < 2UL)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }
}

static VOID WaitForCycleDone_(UINT const cycle)
{
    while ((wDoneCycle != cycle) || (lDoneCycle != cycle))
    {
        kSleep(RK_MS_TO_TICKS(10));
    }
}

static VOID RunTimeoutRestoreCase_(UINT const cycle)
{
    RK_ERR const err = kMutexLock(&mutexM, H_MUTEX_TIMEOUT_TICKS);

    if (err != RK_ERR_TIMEOUT)
    {
        printf("WQ MUTEX expected timeout err=%d\r\n", err);
        TestFail_("H mutex timeout");
    }

    if (observerDoneCycle != cycle)
    {
        TestFail_("observer did not run");
    }

    ExpectTaskPrio_(lHandle, L_PRIO, "H timeout restored L");
    ExpectSemaOrder_(wHandle, lHandle, "H timeout W,L");

    PostExpectHead_(wHandle, "post restored W");
    ExpectSemaHead_(lHandle, "after W post");
    PostExpectHead_(lHandle, "post restored L");
    WaitForCycleDone_(cycle);
}

static VOID RunBoostedPostCase_(UINT const cycle)
{
    RK_ERR const err = kMutexLock(&mutexM, H_MUTEX_TIMEOUT_TICKS);

    if (err != RK_ERR_SUCCESS)
    {
        printf("WQ MUTEX expected success err=%d\r\n", err);
        TestFail_("H mutex success");
    }

    TestCheckErr_(kMutexUnlock(&mutexM), "H unlock M");

    if (observerDoneCycle != cycle)
    {
        TestFail_("observer post did not run");
    }

    ExpectTaskPrio_(lHandle, L_PRIO, "boost post restored L");
    PostExpectHead_(wHandle, "post remaining W");
    WaitForCycleDone_(cycle);
}

static VOID RunFiniteSemaTimeoutCase_(UINT const cycle)
{
    RK_ERR const err = kMutexLock(&mutexM, H_MUTEX_TIMEOUT_TICKS);

    if (err != RK_ERR_TIMEOUT)
    {
        printf("WQ MUTEX finite expected timeout err=%d\r\n", err);
        TestFail_("H finite mutex timeout");
    }

    if (observerDoneCycle != cycle)
    {
        TestFail_("finite observer did not run");
    }

    ExpectTaskPrio_(lHandle, L_PRIO, "finite H timeout restored L");
    ExpectSemaOrder_(wHandle, lHandle, "finite restored W,L");

    while (lSemaTimeoutCycle != cycle)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    ExpectSemaHead_(wHandle, "finite leaves W");
    PostExpectHead_(wHandle, "finite post W");
    WaitForCycleDone_(cycle);
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
    TestCheckErr_(kTaskInit(&wHandle, WTask, RK_NO_ARGS, "W", wStack,
                            STACKSIZE, W_PRIO, RK_PREEMPT),
                  "task W");
    TestCheckErr_(kTaskInit(&lHandle, LTask, RK_NO_ARGS, "L", lStack,
                            STACKSIZE, L_PRIO, RK_PREEMPT),
                  "task L");

    TestCheckErr_(kMutexInit(&mutexM, RK_PRIO_INHERITANCE), "mutex M");
    TestCheckErr_(kSemaBinInit(&semaS, 0U), "sema S");
    TestCheckErr_(kTimerInit(&observerTimer, 0U, RK_MS_TO_TICKS(90),
                             ObserverCb_, RK_NO_ARGS, RK_TIMER_ONESHOT),
                  "observer timer");
}

VOID HTask(VOID *args)
{
    RK_UNUSEARGS

    static UINT const modes[] = {
        MODE_TIMEOUT_RESTORE,
        MODE_BOOSTED_POST,
        MODE_L_FINITE_TIMEOUT,
    };

    for (UINT i = 0U; i < (sizeof(modes) / sizeof(modes[0])); i++)
    {
        UINT const cycle = i + 1U;

        testMode = modes[i];
        testCycle = cycle;
        printf("WQ cycle=%u mode=%u start\r\n", cycle, testMode);

        kSleep(SETUP_SLEEP_TICKS);
        WaitForTwoSemaWaiters_();
        ExpectSemaOrder_(wHandle, lHandle, "before H blocks W,L");
        ArmObserver_(cycle,
                     (testMode == MODE_BOOSTED_POST) ? 1U : 0U);

        if (testMode == MODE_TIMEOUT_RESTORE)
        {
            RunTimeoutRestoreCase_(cycle);
        }
        else if (testMode == MODE_BOOSTED_POST)
        {
            RunBoostedPostCase_(cycle);
        }
        else
        {
            RunFiniteSemaTimeoutCase_(cycle);
        }

        printf("WQ cycle=%u pass\r\n", cycle);
        kSleep(RK_MS_TO_TICKS(60));
    }

    printf("WQ PASS wait queue repriority\r\n");
    TestPassStop_();
}

VOID WTask(VOID *args)
{
    RK_UNUSEARGS

    UINT seen = 0U;

    while (1)
    {
        while (testCycle == seen)
        {
            kSleep(RK_MS_TO_TICKS(10));
        }
        seen = testCycle;

        TestCheckErr_(kSemaphorePend(&semaS, RK_WAIT_FOREVER), "W wait S");
        wDoneCycle = seen;
    }
}

VOID LTask(VOID *args)
{
    RK_UNUSEARGS

    UINT seen = 0U;

    while (1)
    {
        while (testCycle == seen)
        {
            kSleep(RK_MS_TO_TICKS(10));
        }
        seen = testCycle;

        TestCheckErr_(kMutexLock(&mutexM, RK_WAIT_FOREVER), "L lock M");

        RK_TICK const semaTimeout =
            (testMode == MODE_L_FINITE_TIMEOUT)
                ? L_SEMA_TIMEOUT_TICKS
                : RK_WAIT_FOREVER;
        RK_ERR const semaErr = kSemaphorePend(&semaS, semaTimeout);

        if (testMode == MODE_L_FINITE_TIMEOUT)
        {
            if (semaErr != RK_ERR_TIMEOUT)
            {
                printf("WQ L expected S timeout err=%d\r\n", semaErr);
                TestFail_("L finite S timeout");
            }
            lSemaTimeoutCycle = seen;
        }
        else
        {
            TestCheckErr_(semaErr, "L wait S");
        }

        TestCheckErr_(kMutexUnlock(&mutexM), "L unlock M");
        lDoneCycle = seen;
    }
}
