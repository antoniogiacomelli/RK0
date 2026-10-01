/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                    */
/* VERSION: V0.85.0                                                          */
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

    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend) ==
                     RK_ERR_SUCCESS),
           "initial query result");
    Check_((RK_BOOL)((mailPtr == NULL) && (nPend == 0U)),
           "initial query state");

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
    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend) ==
                     RK_ERR_SUCCESS),
           "buffered query result");
    Check_((RK_BOOL)((mailPtr == &payloadA) && (nPend == 0U)),
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
    (void)kSleep(1UL);
    Check_((RK_BOOL)(directPostErr == RK_ERR_SUCCESS), "direct post result");

    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadA, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "blocked sender setup");
    stage = 2U;
    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadB, RK_WAIT_FOREVER) ==
                     RK_ERR_SUCCESS),
           "blocked sender result");
    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend) ==
                     RK_ERR_SUCCESS),
           "blocked sender query");
    Check_((RK_BOOL)(mailPtr == &payloadB), "blocked sender value");
    (void)kSleep(1UL);
    Check_((RK_BOOL)((worker1RecvErr == RK_ERR_SUCCESS) &&
                     (worker1RecvPtr == &payloadA)),
           "blocked sender consumed value");
    Check_((RK_BOOL)((kExchangeAccept(&exchange, &mailPtr) == RK_ERR_SUCCESS) &&
                     (mailPtr == &payloadB)),
           "blocked sender accept");

    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadA, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "sender timeout setup");
    Check_((RK_BOOL)(kExchangePost(&exchange, &payloadB, SHORT_WAIT) ==
                     RK_ERR_TIMEOUT),
           "sender timeout result");
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
    (void)kSleep(SHORT_WAIT);
    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend) ==
                     RK_ERR_SUCCESS),
           "broadcast query");
    Check_((RK_BOOL)((mailPtr == NULL) && (nPend == 2U)),
           "broadcast waiters");

    UINT nRecv = 0U;
    Check_((RK_BOOL)(kExchangeBroadcast(&exchange, &payloadC, &nRecv) ==
                     RK_ERR_SUCCESS),
           "broadcast result");
    Check_((RK_BOOL)(nRecv == 2U), "broadcast count");
    (void)kSleep(1UL);
    Check_((RK_BOOL)((worker1BroadcastErr == RK_ERR_SUCCESS) &&
                     (worker2BroadcastErr == RK_ERR_SUCCESS) &&
                     (worker1BroadcastPtr == &payloadC) &&
                     (worker2BroadcastPtr == &payloadC)),
           "broadcast values");
#endif

    Check_((RK_BOOL)(kExchangeQuery(&exchange, &mailPtr, &nPend) ==
                     RK_ERR_SUCCESS),
           "final query");
    Check_((RK_BOOL)((mailPtr == NULL) && (nPend == 0U)), "final state");

    printf("EX PASS exchange pointer mailbox\r\n");
    Stop_();
}

VOID Worker1Task(VOID *args)
{
    RK_UNUSEARGS

    WaitForStage_(1U);
    directPostErr = kExchangePost(&exchange, &payloadDirect, RK_NO_WAIT);

    WaitForStage_(2U);
    VOID *mailPtr = NULL;
    worker1RecvErr = kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER);
    worker1RecvPtr = mailPtr;

#if (RK_CONF_EXCHG_BROADCAST == ON)
    WaitForStage_(3U);
    mailPtr = NULL;
    worker1BroadcastErr =
        kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER);
    worker1BroadcastPtr = mailPtr;
#endif
    Stop_();
}

VOID Worker2Task(VOID *args)
{
    RK_UNUSEARGS

#if (RK_CONF_EXCHG_BROADCAST == ON)
    WaitForStage_(3U);
    VOID *mailPtr = NULL;
    worker2BroadcastErr =
        kExchangePend(&exchange, &mailPtr, RK_WAIT_FOREVER);
    worker2BroadcastPtr = mailPtr;
#endif
    Stop_();
}
