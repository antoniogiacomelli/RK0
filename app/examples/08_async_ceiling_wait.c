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
 * Async message-pool ceilings apply while tasks wait to allocate from the pool.
 *
 * A high-priority task first proves the admission rule: a task already above the
 * ceiling cannot acquire the pool. A holder then consumes the only pool message.
 * Two lower-priority tasks block in kMesgAlloc(). Both waiters must be raised to
 * the pool ceiling while queued, restore on timeout, and keep the ceiling when a
 * free message is handed off.
 */

#include <kapi.h>
#include <klist.h>
#include <stdio.h>

#define STACKSIZE 192U

#define CEILING_PRIO 3U
#define HI_PRIO 2U
#define HOLDER_PRIO 8U
#define W1_PRIO 10U
#define W2_PRIO 12U

#define OBSERVE_DELAY_TICKS RK_MS_TO_TICKS(60)
#define W1_TIMEOUT_TICKS RK_MS_TO_TICKS(150)
#define W2_TIMEOUT_TICKS RK_MS_TO_TICKS(180)

#define MODE_TIMEOUT_RESTORE 1U
#define MODE_HANDOFF 2U

typedef struct AsyncCeilingPayload
{
    ULONG value;
} AsyncCeilingPayload;

RK_DECLARE_TASK(hiHandle, HiTask, hiStack, STACKSIZE)
RK_DECLARE_TASK(holderHandle, HolderTask, holderStack, STACKSIZE)
RK_DECLARE_TASK(w1Handle, W1Task, w1Stack, STACKSIZE)
RK_DECLARE_TASK(w2Handle, W2Task, w2Stack, STACKSIZE)
RK_DECLARE_MESG_POOL(mesgPool, mesgPoolBuf, AsyncCeilingPayload, 1U)

static RK_TIMER observerTimer;
static volatile UINT testCycle;
static volatile UINT testMode;
static volatile UINT observerCycle;
static volatile UINT observerDoneCycle;
static volatile UINT hiAdmissionStart;
static volatile UINT hiAdmissionDone;
static volatile UINT w1DoneCycle;
static volatile UINT w2DoneCycle;

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
    printf("AC FAIL %s t=%lu c=%u mode=%u\r\n", wherePtr, kTickGetMs(),
           testCycle, testMode);
    K_ASSERT(0);
    TestFaultStop_();
}

static VOID TestCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("AC ERR %s err=%d\r\n", wherePtr, err);
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
        printf("AC PRIO %s exp=%u got=%u\r\n", wherePtr, (UINT)expected,
               (UINT)actual);
        TestFail_(wherePtr);
    }
}

static RK_TASK_HANDLE PoolWaitAt_(UINT const index)
{
    RK_TASK_HANDLE ret = NULL;
    RK_CR_AREA
    RK_CR_ENTER

    if (index < mesgPool.waitingQueue.size)
    {
        RK_NODE *nodePtr = mesgPool.waitingQueue.listDummy.nextPtr;

        for (UINT i = 0U; i < index; i++)
        {
            nodePtr = nodePtr->nextPtr;
        }
        ret = K_GET_TCB_ADDR(nodePtr);
    }

    RK_CR_EXIT
    return (ret);
}

static VOID ExpectPoolWaitOrder_(RK_TASK_HANDLE const first,
                                 RK_TASK_HANDLE const second,
                                 CHAR const *const wherePtr)
{
    RK_TASK_HANDLE const actualFirst = PoolWaitAt_(0U);
    RK_TASK_HANDLE const actualSecond = PoolWaitAt_(1U);

    if ((actualFirst != first) || (actualSecond != second))
    {
        printf("AC ORDER %s exp=%p,%p got=%p,%p size=%lu\r\n", wherePtr,
               (VOID *)first, (VOID *)second, (VOID *)actualFirst,
               (VOID *)actualSecond, mesgPool.waitingQueue.size);
        TestFail_(wherePtr);
    }
}

static VOID ExpectPoolEmpty_(CHAR const *const wherePtr)
{
    if (mesgPool.waitingQueue.size != 0UL)
    {
        printf("AC QUEUE %s size=%lu\r\n", wherePtr,
               mesgPool.waitingQueue.size);
        TestFail_(wherePtr);
    }
}

static VOID ObserverCb_(VOID *args)
{
    RK_UNUSEARGS

    UINT const cycle = observerCycle;

    if (cycle == 0U)
    {
        return;
    }

    ExpectTaskPrio_(w1Handle, CEILING_PRIO, "observer W1 ceiling");
    ExpectTaskPrio_(w2Handle, CEILING_PRIO, "observer W2 ceiling");
    ExpectPoolWaitOrder_(w1Handle, w2Handle, "observer FIFO ceiling");
    observerDoneCycle = cycle;
}

static VOID ArmObserver_(UINT const cycle)
{
    observerCycle = cycle;

    if (cycle > 1U)
    {
        kTimerReload(&observerTimer, OBSERVE_DELAY_TICKS);
    }
}

static VOID WaitForCycleDone_(UINT const cycle)
{
    while ((w1DoneCycle != cycle) || (w2DoneCycle != cycle))
    {
        kSleep(RK_MS_TO_TICKS(10));
    }
}

static VOID WaitForObserver_(UINT const cycle)
{
    while (observerDoneCycle != cycle)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }
}

static VOID RunTimeoutCase_(VOID)
{
    RK_MESG *heldPtr = NULL;

    printf("AC cycle=1 mode=timeout start\r\n");
    TestCheckErr_(kMesgAlloc(&mesgPool, &heldPtr, RK_NO_WAIT),
                  "holder alloc timeout");
    ExpectTaskPrio_(holderHandle, CEILING_PRIO, "holder owns ceiling");

    testMode = MODE_TIMEOUT_RESTORE;
    testCycle = 1U;
    ArmObserver_(1U);

    WaitForObserver_(1U);
    kSleep(RK_MS_TO_TICKS(220));
    WaitForCycleDone_(1U);
    ExpectPoolEmpty_("timeout pool empty");
    ExpectTaskPrio_(w1Handle, W1_PRIO, "W1 timeout restored");
    ExpectTaskPrio_(w2Handle, W2_PRIO, "W2 timeout restored");

    TestCheckErr_(kMesgFree(heldPtr), "holder free timeout");
    ExpectTaskPrio_(holderHandle, HOLDER_PRIO, "holder free restored");
    printf("AC cycle=1 pass\r\n");
}

static VOID RunAdmissionCase_(VOID)
{
    RK_MESG *mesgPtr = NULL;

    printf("AC admission start\r\n");
    hiAdmissionStart = 1U;
    while (hiAdmissionDone == 0U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    ExpectPoolEmpty_("high denied free pool");
    ExpectTaskPrio_(hiHandle, HI_PRIO, "high denied prio");

    TestCheckErr_(kMesgAlloc(&mesgPool, &mesgPtr, RK_NO_WAIT),
                  "holder alloc send admission");
    ExpectTaskPrio_(holderHandle, CEILING_PRIO, "holder send owns ceiling");

    RK_ERR const err = kMesgSend(hiHandle, mesgPtr);
    if (err != RK_ERR_INVALID_PRIO)
    {
        printf("AC send admission err=%d\r\n", err);
        TestFail_("send ceiling admission");
    }
    ExpectTaskPrio_(holderHandle, CEILING_PRIO, "holder still owns ceiling");
    TestCheckErr_(kMesgFree(mesgPtr), "holder free send admission");
    ExpectTaskPrio_(holderHandle, HOLDER_PRIO, "holder send restored");

    printf("AC admission pass\r\n");
}

static VOID RunHandoffCase_(VOID)
{
    RK_MESG *heldPtr = NULL;

    printf("AC cycle=2 mode=handoff start\r\n");
    TestCheckErr_(kMesgAlloc(&mesgPool, &heldPtr, RK_NO_WAIT),
                  "holder alloc handoff");
    ExpectTaskPrio_(holderHandle, CEILING_PRIO, "holder owns handoff");

    testMode = MODE_HANDOFF;
    testCycle = 2U;
    ArmObserver_(2U);

    WaitForObserver_(2U);
    TestCheckErr_(kMesgFree(heldPtr), "holder handoff W1");
    WaitForCycleDone_(2U);
    ExpectPoolEmpty_("handoff pool empty");
    ExpectTaskPrio_(holderHandle, HOLDER_PRIO, "holder handoff restored");
    printf("AC cycle=2 pass\r\n");
}

int main(void)
{
    kCoreInit();
    kInit();

    TestFaultStop_();
}

VOID kApplicationInit(VOID)
{
    TestCheckErr_(kTaskInit(&hiHandle, HiTask, RK_NO_ARGS, "AChi", hiStack,
                            STACKSIZE, HI_PRIO, RK_PREEMPT),
                  "task high");
    TestCheckErr_(kTaskInit(&holderHandle, HolderTask, RK_NO_ARGS, "AChold",
                            holderStack, STACKSIZE, HOLDER_PRIO, RK_PREEMPT),
                  "task holder");
    TestCheckErr_(kTaskInit(&w1Handle, W1Task, RK_NO_ARGS, "ACw1", w1Stack,
                            STACKSIZE, W1_PRIO, RK_PREEMPT),
                  "task W1");
    TestCheckErr_(kTaskInit(&w2Handle, W2Task, RK_NO_ARGS, "ACw2", w2Stack,
                            STACKSIZE, W2_PRIO, RK_PREEMPT),
                  "task W2");

    TestCheckErr_(kMesgPoolInit(&mesgPool, mesgPoolBuf,
                                sizeof(AsyncCeilingPayload), 1U,
                                CEILING_PRIO),
                  "message pool");
    TestCheckErr_(kMesgEndpointInit(hiHandle), "high endpoint");
    TestCheckErr_(kTimerInit(&observerTimer, 0U, OBSERVE_DELAY_TICKS,
                             ObserverCb_, RK_NO_ARGS, RK_TIMER_ONESHOT),
                  "observer timer");
}

VOID HolderTask(VOID *args)
{
    RK_UNUSEARGS

    RunAdmissionCase_();
    RunTimeoutCase_();
    RunHandoffCase_();

    printf("AC PASS async ceiling waiters\r\n");
    TestPassStop_();
}

VOID HiTask(VOID *args)
{
    RK_UNUSEARGS

    while (hiAdmissionStart == 0U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    RK_MESG *mesgPtr = NULL;
    RK_ERR const err = kMesgAlloc(&mesgPool, &mesgPtr, RK_WAIT_FOREVER);
    if ((err != RK_ERR_INVALID_PRIO) || (mesgPtr != NULL))
    {
        printf("AC HI admission err=%d msg=%p\r\n", err, (VOID *)mesgPtr);
        TestFail_("high ceiling admission");
    }
    hiAdmissionDone = 1U;

    TestPassStop_();
}

VOID W1Task(VOID *args)
{
    RK_UNUSEARGS

    while (testCycle < 1U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    RK_MESG *mesgPtr = NULL;
    RK_ERR err = kMesgAlloc(&mesgPool, &mesgPtr, W1_TIMEOUT_TICKS);
    if (err != RK_ERR_TIMEOUT)
    {
        printf("AC W1 timeout err=%d msg=%p\r\n", err, (VOID *)mesgPtr);
        TestFail_("W1 expected timeout");
    }
    ExpectTaskPrio_(w1Handle, W1_PRIO, "W1 post-timeout prio");
    w1DoneCycle = 1U;

    while (testCycle < 2U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    mesgPtr = NULL;
    err = kMesgAlloc(&mesgPool, &mesgPtr, RK_WAIT_FOREVER);
    TestCheckErr_(err, "W1 handoff alloc");
    if (mesgPtr == NULL)
    {
        TestFail_("W1 handoff null");
    }
    ExpectTaskPrio_(w1Handle, CEILING_PRIO, "W1 owns handoff ceiling");
    TestCheckErr_(kMesgFree(mesgPtr), "W1 free handoff");
    ExpectTaskPrio_(w1Handle, W1_PRIO, "W1 handoff restored");
    w1DoneCycle = 2U;

    TestPassStop_();
}

VOID W2Task(VOID *args)
{
    RK_UNUSEARGS

    while (testCycle < 1U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    RK_MESG *mesgPtr = NULL;
    RK_ERR err = kMesgAlloc(&mesgPool, &mesgPtr, W2_TIMEOUT_TICKS);
    if (err != RK_ERR_TIMEOUT)
    {
        printf("AC W2 timeout err=%d msg=%p\r\n", err, (VOID *)mesgPtr);
        TestFail_("W2 expected timeout");
    }
    ExpectTaskPrio_(w2Handle, W2_PRIO, "W2 post-timeout prio");
    w2DoneCycle = 1U;

    while (testCycle < 2U)
    {
        kSleep(RK_MS_TO_TICKS(10));
    }

    mesgPtr = NULL;
    err = kMesgAlloc(&mesgPool, &mesgPtr, RK_WAIT_FOREVER);
    TestCheckErr_(err, "W2 handoff alloc");
    if (mesgPtr == NULL)
    {
        TestFail_("W2 handoff null");
    }
    ExpectTaskPrio_(w2Handle, CEILING_PRIO, "W2 owns handoff ceiling");
    TestCheckErr_(kMesgFree(mesgPtr), "W2 free handoff");
    ExpectTaskPrio_(w2Handle, W2_PRIO, "W2 handoff restored");
    w2DoneCycle = 2U;

    TestPassStop_();
}
