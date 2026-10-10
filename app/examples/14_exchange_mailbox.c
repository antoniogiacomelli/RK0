/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                    */
/* VERSION: V0.90.1                                                          */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/* Regression coverage for the single-pointer exchange mailbox. */

#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U
#define CONTROL_PRIO 2U
#define WORKER1_PRIO 5U
#define WORKER2_PRIO 6U
#define SHORT_WAIT RK_MS_TO_TICKS(10UL)
#define SYNC_WAIT RK_MS_TO_TICKS(500UL)
#define DIRECT_POST_DONE RK_EVENT_1
#define WORKER1_RECV_DONE RK_EVENT_2
#define WORKER1_BROADCAST_DONE RK_EVENT_3
#define WORKER2_BROADCAST_DONE RK_EVENT_4
#define WORKER2_POST_DONE RK_EVENT_5

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(worker1Handle, Worker1Task, worker1Stack, STACKSIZE)
RK_DECLARE_TASK(worker2Handle, Worker2Task, worker2Stack, STACKSIZE)
RK_DECLARE_EXCHG(exchange)

static ULONG payloadA = 0xA1UL;
static ULONG payloadB = 0xB2UL;
static ULONG payloadC = 0xC3UL;
static ULONG payloadDirect = 0xD4UL;

static volatile UINT stage;
static volatile RK_ERR directPostErr = RK_ERR_ERROR;
static volatile RK_ERR worker1RecvErr = RK_ERR_ERROR;
static volatile RK_ERR worker2PostErr = RK_ERR_ERROR;
static volatile RK_ERR worker1BroadcastErr = RK_ERR_ERROR;
static volatile RK_ERR worker2BroadcastErr = RK_ERR_ERROR;
static VOID *volatile worker1RecvPtr;
static VOID *volatile worker1BroadcastPtr;
static VOID *volatile worker2BroadcastPtr;

static VOID Stop_(VOID)
{
    while (1)
    {
        (void)kSleep(RK_MS_TO_TICKS(1000UL));
    }
}

static VOID Fail_(CHAR const *const wherePtr)
{
    printf("EX FAIL %s\r\n", wherePtr);
    Stop_();
}

static VOID Check_(RK_BOOL const condition, CHAR const *const wherePtr)
{
    if (condition == RK_FALSE)
    {
        Fail_(wherePtr);
    }
}

static VOID InitRequire_(RK_ERR const err)
{
    if (err != RK_ERR_SUCCESS)
    {
        while (1)
        {
        }
    }
}

static VOID WaitForStage_(UINT const wantedStage)
{
    while (stage < wantedStage)
    {
        (void)kSleep(1UL);
    }
}

static VOID WaitForReceivers_(UINT const wantedCount,
                              CHAR const *const wherePtr)
{
    for (RK_TICK waited = 0UL; waited < SYNC_WAIT; waited++)
    {
        VOID *mailPtr = NULL;
        UINT nPend = 0U;
        UINT nSend = 99U;
        Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend, &nSend) ==
                         RK_ERR_SUCCESS),
               "waiter query result");
        Check_((RK_BOOL)(mailPtr == NULL), "waiter mailbox empty");
        Check_((RK_BOOL)(nSend == 0U), "no senders while receivers wait");
        if ((nPend > 0U) && (nPend == wantedCount))
        {
            return;
        }
        (void)kSleep(1UL);
    }
    Fail_(wherePtr);
}

static VOID WaitForSenders_(UINT const wantedCount,
                            CHAR const *const wherePtr)
{
    for (RK_TICK waited = 0UL; waited < SYNC_WAIT; waited++)
    {
        VOID *mailPtr = NULL;
        UINT nRecv = 99U;
        UINT nSend = 0U;
        Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nRecv, &nSend) ==
                         RK_ERR_SUCCESS),
               "sender query result");
        Check_((RK_BOOL)((mailPtr == &payloadA) && (nRecv == 0U)),
               "sender query state");
        if (nSend == wantedCount)
        {
            UINT nSendOnly = 0U;
            Check_((RK_BOOL)((kExchangeQuery(&exchange, NULL, NULL,
                                             &nSendOnly) == RK_ERR_SUCCESS) &&
                             (nSendOnly == wantedCount)),
                   "sender-only query");
            return;
        }
        (void)kSleep(1UL);
    }
    Fail_(wherePtr);
}

static VOID WaitForCompletion_(RK_TASK_EVENT const events,
                               CHAR const *const wherePtr)
{
    Check_((RK_BOOL)(kEventGet(events, RK_EVENT_ALL, NULL, SYNC_WAIT) ==
                     RK_ERR_SUCCESS),
           wherePtr);
}

int main(void)
{
    kCoreInit();
    kInit();

    while (1)
    {
    }
}

VOID kApplicationInit(VOID)
{
    InitRequire_(kExchangeInit(&exchange, NULL));
    InitRequire_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "EXctrl",
                           controlStack, STACKSIZE, CONTROL_PRIO, RK_PREEMPT));
    InitRequire_(kTaskInit(&worker1Handle, Worker1Task, RK_NO_ARGS, "EXwrk1",
                           worker1Stack, STACKSIZE, WORKER1_PRIO, RK_PREEMPT));
    InitRequire_(kTaskInit(&worker2Handle, Worker2Task, RK_NO_ARGS, "EXwrk2",
                           worker2Stack, STACKSIZE, WORKER2_PRIO, RK_PREEMPT));
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS

    VOID *mailPtr = &payloadA;
    UINT nPend = 99U;
    UINT nSend = 99U;

    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend, &nSend) ==
                     RK_ERR_SUCCESS),
           "initial query result");
    Check_((RK_BOOL)((mailPtr == NULL) && (nPend == 0U) && (nSend == 0U)),
           "initial query state");
    Check_((RK_BOOL)((kExchangeQuery(&exchange, &mailPtr, NULL, NULL) ==
                      RK_ERR_SUCCESS) && (mailPtr == NULL)),
           "pointer-only query");
    Check_((RK_BOOL)((kExchangeQuery(&exchange, NULL, &nPend, NULL) ==
                      RK_ERR_SUCCESS) && (nPend == 0U)),
           "receiver-only query");

    Check_((RK_BOOL)(kExchangeAccept(&exchange, &mailPtr) ==
                     RK_ERR_BUFFER_EMPTY),
           "empty accept result");
    Check_((RK_BOOL)(mailPtr == NULL), "empty accept output");
    mailPtr = &payloadA;
    Check_((RK_BOOL)(kExchangePeek(&exchange, &mailPtr) ==
                     RK_ERR_BUFFER_EMPTY),
           "empty peek result");
    Check_((RK_BOOL)(mailPtr == NULL), "empty peek output");

    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadA, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "buffered post");
    Check_((RK_BOOL)((kExchangePeek(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadA)),
           "buffered peek");
    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend, &nSend) ==
                     RK_ERR_SUCCESS),
           "buffered query result");
    Check_((RK_BOOL)((mailPtr == &payloadA) && (nPend == 0U) && (nSend == 0U)),
           "buffered query state");
    Check_((RK_BOOL)((kExchangeAccept(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadA)),
           "buffered accept");

    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadA, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "overwrite setup");
    Check_((RK_BOOL)(kExchangePostOvw(&exchange, &payloadB) == RK_ERR_SUCCESS),
           "overwrite result");
    Check_((RK_BOOL)((kExchangeAccept(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadB)),
           "overwrite value");

    stage = 1U;
    Check_((RK_BOOL)(kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER) ==
                     RK_ERR_SUCCESS),
           "direct pend result");
    Check_((RK_BOOL)(mailPtr == &payloadDirect), "direct pend value");
    WaitForCompletion_(DIRECT_POST_DONE, "direct post completion");
    Check_((RK_BOOL)(directPostErr == RK_ERR_SUCCESS), "direct post result");

    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadA, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "blocked sender setup");
    stage = 2U;
    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadB, RK_WAIT_FOREVER) ==
                     RK_ERR_SUCCESS),
           "blocked sender result");
    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend, &nSend) ==
                     RK_ERR_SUCCESS),
           "blocked sender query");
    Check_((RK_BOOL)((mailPtr == &payloadB) && (nPend == 0U) && (nSend == 1U)),
           "blocked sender state");
    WaitForCompletion_(WORKER1_RECV_DONE, "blocked sender receiver completion");
    Check_((RK_BOOL)((worker1RecvErr == RK_ERR_SUCCESS) &&
                     (worker1RecvPtr == &payloadA)),
           "blocked sender consumed value");
    Check_((RK_BOOL)((kExchangeAccept(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadB)),
           "blocked sender accept");
    WaitForCompletion_(WORKER2_POST_DONE, "second sender completion");
    Check_((RK_BOOL)(worker2PostErr == RK_ERR_SUCCESS), "second sender result");
    Check_((RK_BOOL)((kExchangeQuery(&exchange, NULL, &nPend, &nSend) ==
                      RK_ERR_SUCCESS) && (nPend == 0U) && (nSend == 0U)),
           "completed senders removed");
    Check_((RK_BOOL)((kExchangeAccept(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadC)),
           "second sender value");

    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadA, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "sender timeout setup");
    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadB, SHORT_WAIT) ==
                     RK_ERR_TIMEOUT),
           "sender timeout result");
    nSend = 99U;
    Check_((RK_BOOL)((kExchangeQuery(&exchange, NULL, NULL, &nSend) ==
                      RK_ERR_SUCCESS) && (nSend == 0U)),
           "timed-out sender removed");
    Check_((RK_BOOL)((kExchangeAccept(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadA)),
           "sender timeout preserves value");

    mailPtr = &payloadA;
    Check_((RK_BOOL)(kExchangePend(&exchange, &mailPtr, SHORT_WAIT) ==
                     RK_ERR_TIMEOUT),
           "receiver timeout result");
    Check_((RK_BOOL)(mailPtr == NULL), "receiver timeout output");

#if (RK_CONF_EXCHG_BROADCAST == ON)
    stage = 3U;
    WaitForReceivers_(2U, "broadcast waiters");

    UINT nRecv = 0U;
    Check_((RK_BOOL)(kExchangeBroadcast(&exchange, &payloadC, &nRecv) ==
                     RK_ERR_SUCCESS),
           "broadcast result");
    Check_((RK_BOOL)(nRecv == 2U), "broadcast count");
    WaitForCompletion_(WORKER1_BROADCAST_DONE | WORKER2_BROADCAST_DONE,
                       "broadcast receiver completion");
    Check_((RK_BOOL)((worker1BroadcastErr == RK_ERR_SUCCESS) &&
                     (worker2BroadcastErr == RK_ERR_SUCCESS) &&
                     (worker1BroadcastPtr == &payloadC) &&
                     (worker2BroadcastPtr == &payloadC)),
           "broadcast values");
#endif

    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend, &nSend) ==
                     RK_ERR_SUCCESS),
           "final query");
    Check_((RK_BOOL)((mailPtr == NULL) && (nPend == 0U) && (nSend == 0U)),
           "final state");

    printf("EX PASS exchange pointer mailbox\r\n");
    Stop_();
}

VOID Worker1Task(VOID *args)
{
    RK_UNUSEARGS

    WaitForStage_(1U);
    WaitForReceivers_(1U, "direct receiver waiting");
    directPostErr = kExchangePost(&exchange, &payloadDirect, RK_NO_WAIT);
    Check_((RK_BOOL)(kEventSet(controlHandle, DIRECT_POST_DONE) ==
                     RK_ERR_SUCCESS),
           "direct post signal");

    WaitForStage_(2U);
    WaitForSenders_(2U, "two blocked senders");
    VOID *mailPtr = NULL;
    worker1RecvErr = kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER);
    worker1RecvPtr = mailPtr;
    Check_((RK_BOOL)(kEventSet(controlHandle, WORKER1_RECV_DONE) ==
                     RK_ERR_SUCCESS),
           "receiver completion signal");

#if (RK_CONF_EXCHG_BROADCAST == ON)
    WaitForStage_(3U);
    mailPtr = NULL;
    worker1BroadcastErr =
        kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER);
    worker1BroadcastPtr = mailPtr;
    Check_((RK_BOOL)(kEventSet(controlHandle, WORKER1_BROADCAST_DONE) ==
                     RK_ERR_SUCCESS),
           "worker1 broadcast signal");
#endif
    Stop_();
}

VOID Worker2Task(VOID *args)
{
    RK_UNUSEARGS

    WaitForStage_(2U);
    WaitForSenders_(1U, "first blocked sender");
    worker2PostErr = kExchangePost(&exchange, &payloadC, RK_WAIT_FOREVER);
    Check_((RK_BOOL)(kEventSet(controlHandle, WORKER2_POST_DONE) ==
                     RK_ERR_SUCCESS),
           "second sender completion signal");

#if (RK_CONF_EXCHG_BROADCAST == ON)
    WaitForStage_(3U);
    VOID *mailPtr = NULL;
    worker2BroadcastErr =
        kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER);
    worker2BroadcastPtr = mailPtr;
    Check_((RK_BOOL)(kEventSet(controlHandle, WORKER2_BROADCAST_DONE) ==
                     RK_ERR_SUCCESS),
           "worker2 broadcast signal");
#endif
    Stop_();
}
