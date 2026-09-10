/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                     */
/* VERSION: V0.80.0                                                           */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * Regression bench:
 * Real transitive priority inheritance over a mutex chain.
 *
 * Chain:
 *   H prio 1 waits on A owned by M.
 *   M prio 5 waits on B owned by L.
 *   L prio 10 owns B.
 *   W prio 4 is another waiter on B.
 *
 * Before H blocks, B wait order is W,M and L inherits W priority. When H
 * blocks on A, M inherits H priority while already waiting on B, so M must move
 * ahead of W and L must inherit H priority through M. When H times out, M and L
 * must drop back to the remaining donors and B wait order must return to W,M.
 */

#include <kapi.h>
#include <klist.h>
#include <stdarg.h>
#include <stdio.h>

#define STACKSIZE 192U

#define H_PRIO 1U
#define W_PRIO 4U
#define M_PRIO 5U
#define L_PRIO 10U

#define W_START_TICKS RK_MS_TO_TICKS(20)
#define M_START_TICKS RK_MS_TO_TICKS(40)
#define H_START_TICKS RK_MS_TO_TICKS(80)
#define H_TIMEOUT_TICKS RK_MS_TO_TICKS(180)

RK_DECLARE_TASK(hHandle, HTask, hStack, STACKSIZE)
RK_DECLARE_TASK(wHandle, WTask, wStack, STACKSIZE)
RK_DECLARE_TASK(mHandle, MTask, mStack, STACKSIZE)
RK_DECLARE_TASK(lHandle, LTask, lStack, STACKSIZE)

static RK_MUTEX mutexA;
static RK_MUTEX mutexB;

static volatile UINT wAttemptB;
static volatile UINT mAttemptB;
static volatile UINT hAttemptA;
static volatile UINT hTimedOut;
static volatile UINT lReleasedB;
static volatile UINT wReleasedB;
static volatile UINT mReleasedA;

static VOID BenchLog_(CHAR const *const fmtPtr, ...)
{
    va_list args;

    va_start(args, fmtPtr);
    vprintf(fmtPtr, args);
    va_end(args);
    printf("\r\n");
}

static VOID BenchStopFault_(VOID)
{
    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

static VOID BenchStopPass_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

static VOID BenchFail_(CHAR const *const wherePtr)
{
    printf("PI FAIL %s t=%lu\r\n", wherePtr, kTickGetMs());
    K_ASSERT(0);
    BenchStopFault_();
}

static VOID BenchCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("PI ERR %s err=%d\r\n", wherePtr, err);
        K_ASSERT(err == RK_ERR_SUCCESS);
        BenchStopFault_();
    }
}

static VOID BenchExpectTaskPrio_(RK_TASK_HANDLE const taskHandle,
                                 RK_PRIO const expected,
                                 CHAR const *const wherePtr)
{
    RK_PRIO const actual = RK_TASK_PRIO(taskHandle);

    if (actual != expected)
    {
        printf("PI PRIO %s exp=%u got=%u task=%s\r\n", wherePtr,
               (UINT)expected, (UINT)actual, taskHandle->taskName);
        BenchFail_(wherePtr);
    }
}

static RK_TASK_HANDLE MutexBWaitAt_(UINT const index)
{
    RK_TASK_HANDLE ret = NULL;
    RK_CR_AREA
    RK_CR_ENTER

    if (index < mutexB.waitingQueue.size)
    {
        RK_NODE *nodePtr = mutexB.waitingQueue.listDummy.nextPtr;

        for (UINT i = 0U; i < index; i++)
        {
            nodePtr = nodePtr->nextPtr;
        }
        ret = K_GET_TCB_ADDR(nodePtr);
    }

    RK_CR_EXIT
    return (ret);
}

static VOID BenchExpectBOrder_(RK_TASK_HANDLE const first,
                               RK_TASK_HANDLE const second,
                               CHAR const *const wherePtr)
{
    RK_TASK_HANDLE const actualFirst = MutexBWaitAt_(0U);
    RK_TASK_HANDLE const actualSecond = MutexBWaitAt_(1U);

    if ((actualFirst != first) || (actualSecond != second))
    {
        printf("PI ORDER %s exp=%s,%s got=%s,%s size=%lu\r\n", wherePtr,
               first->taskName, second->taskName,
               (actualFirst != NULL) ? actualFirst->taskName : "NULL",
               (actualSecond != NULL) ? actualSecond->taskName : "NULL",
               mutexB.waitingQueue.size);
        BenchFail_(wherePtr);
    }
}

int main(void)
{
    kCoreInit();
    kInit();

    BenchStopFault_();
}

VOID kApplicationInit(VOID)
{
    BenchCheckErr_(kTaskInit(&hHandle, HTask, RK_NO_ARGS, "H", hStack,
                             STACKSIZE, H_PRIO, RK_PREEMPT),
                   "task H");
    BenchCheckErr_(kTaskInit(&wHandle, WTask, RK_NO_ARGS, "W", wStack,
                             STACKSIZE, W_PRIO, RK_PREEMPT),
                   "task W");
    BenchCheckErr_(kTaskInit(&mHandle, MTask, RK_NO_ARGS, "M", mStack,
                             STACKSIZE, M_PRIO, RK_PREEMPT),
                   "task M");
    BenchCheckErr_(kTaskInit(&lHandle, LTask, RK_NO_ARGS, "L", lStack,
                             STACKSIZE, L_PRIO, RK_PREEMPT),
                   "task L");

    BenchCheckErr_(kMutexInit(&mutexA, RK_PRIO_INHERITANCE), "mutex A");
    BenchCheckErr_(kMutexInit(&mutexB, RK_PRIO_INHERITANCE), "mutex B");

    BenchLog_("PI bench: lower priority number means higher priority");
    BenchLog_("PI bench: H->M->L chain with W as remaining B waiter");
}

VOID HTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(H_START_TICKS);

    while (mAttemptB == 0U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    BenchExpectTaskPrio_(lHandle, W_PRIO, "before H L inherits W");
    BenchExpectBOrder_(wHandle, mHandle, "before H blocks B W,M");
    BenchLog_("PI: before H blocks, B order=W,M; expected L eff=%u actual=%u",
              (UINT)W_PRIO, (UINT)RK_TASK_PRIO(lHandle));

    hAttemptA = 1U;
    RK_ERR const err = kMutexLock(&mutexA, H_TIMEOUT_TICKS);
    if (err != RK_ERR_TIMEOUT)
    {
        printf("PI H expected timeout err=%d\r\n", err);
        BenchFail_("H timeout on A");
    }

    BenchExpectTaskPrio_(mHandle, M_PRIO, "after H timeout M restored");
    BenchExpectTaskPrio_(lHandle, W_PRIO, "after H timeout L inherits W");
    BenchExpectBOrder_(wHandle, mHandle, "after H timeout B W,M");
    BenchLog_("PI: H timed out; expected M eff=%u actual=%u; "
              "expected L eff=%u actual=%u",
              (UINT)M_PRIO, (UINT)RK_TASK_PRIO(mHandle),
              (UINT)W_PRIO, (UINT)RK_TASK_PRIO(lHandle));

    hTimedOut = 1U;

    while ((lReleasedB == 0U) || (wReleasedB == 0U) || (mReleasedA == 0U))
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    BenchExpectTaskPrio_(wHandle, W_PRIO, "final W restored");
    BenchExpectTaskPrio_(mHandle, M_PRIO, "final M restored");
    BenchExpectTaskPrio_(lHandle, L_PRIO, "final L restored");

    printf("PI PASS transitive priority inheritance\r\n");
    BenchStopPass_();
}

VOID WTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(W_START_TICKS);
    wAttemptB = 1U;
    BenchLog_("PI: W waits on B; W eff=%u nom=%u", (UINT)RK_RUNNING_PRIO,
              (UINT)RK_RUNNING_NOM_PRIO);

    BenchCheckErr_(kMutexLock(&mutexB, RK_WAIT_FOREVER), "W lock B");
    BenchExpectTaskPrio_(wHandle, W_PRIO, "W acquired B");
    BenchLog_("PI: W acquired B first after H timeout; expected W eff=%u "
              "actual=%u",
              (UINT)W_PRIO, (UINT)RK_RUNNING_PRIO);

    BenchCheckErr_(kMutexUnlock(&mutexB), "W unlock B");
    wReleasedB = 1U;
    BenchStopPass_();
}

VOID MTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(M_START_TICKS);
    BenchCheckErr_(kMutexLock(&mutexA, RK_WAIT_FOREVER), "M lock A");
    BenchLog_("PI: M owns A; expected M eff=%u actual=%u", (UINT)M_PRIO,
              (UINT)RK_RUNNING_PRIO);

    mAttemptB = 1U;
    BenchLog_("PI: M waits on B while owning A; M eff=%u nom=%u",
              (UINT)RK_RUNNING_PRIO, (UINT)RK_RUNNING_NOM_PRIO);

    BenchCheckErr_(kMutexLock(&mutexB, RK_WAIT_FOREVER), "M lock B");
    BenchExpectTaskPrio_(mHandle, M_PRIO, "M acquired B restored");
    BenchLog_("PI: M acquired B after W; expected M eff=%u actual=%u",
              (UINT)M_PRIO, (UINT)RK_RUNNING_PRIO);

    BenchCheckErr_(kMutexUnlock(&mutexB), "M unlock B");
    BenchCheckErr_(kMutexUnlock(&mutexA), "M unlock A");
    mReleasedA = 1U;
    BenchStopPass_();
}

VOID LTask(VOID *args)
{
    RK_UNUSEARGS

    BenchCheckErr_(kMutexLock(&mutexB, RK_WAIT_FOREVER), "L lock B");
    BenchLog_("PI: L owns B; expected L eff=%u actual=%u", (UINT)L_PRIO,
              (UINT)RK_RUNNING_PRIO);

    while (mAttemptB == 0U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    BenchExpectTaskPrio_(lHandle, W_PRIO, "L inherits W before H");
    BenchExpectBOrder_(wHandle, mHandle, "L sees B W,M");

    while (hAttemptA == 0U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    BenchExpectTaskPrio_(mHandle, H_PRIO, "M inherits H");
    BenchExpectTaskPrio_(lHandle, H_PRIO, "L inherits H through M");
    BenchExpectBOrder_(mHandle, wHandle, "B reordered M,W");
    BenchLog_("PI: H waits on A; expected M eff=%u actual=%u; "
              "expected L eff=%u actual=%u; B order=M,W",
              (UINT)H_PRIO, (UINT)RK_TASK_PRIO(mHandle),
              (UINT)H_PRIO, (UINT)RK_RUNNING_PRIO);

    while (hTimedOut == 0U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    BenchExpectTaskPrio_(lHandle, W_PRIO, "L drops to W after H timeout");
    BenchExpectBOrder_(wHandle, mHandle, "B restored W,M");

    BenchCheckErr_(kMutexUnlock(&mutexB), "L unlock B");
    lReleasedB = 1U;
    BenchStopPass_();
}
