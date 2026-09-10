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
 * Extended rendezvous adopts the accepted caller's priority until reply
 * delivery.
 *
 * S prio 1 accepts A prio 5. S must run at A's priority, even though S's
 * nominal priority is higher. H prio 2 queues while A is active, but H is only
 * a queued waiter and must raise S while it waits. When S replies, the reply
 * must be copied and the timeout disarmed before scheduler decisions can delay
 * A.
 */

#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U

#define S_PRIO 1U
#define H_PRIO 2U
#define A_PRIO 5U

#define POLL_TICKS 1U
#define ACTIVE_START_DELAY_TICKS RK_MS_TO_TICKS(20)
#define HIGH_QUEUE_OBSERVE_TICKS RK_MS_TO_TICKS(60)
#define ACTIVE_CALL_TIMEOUT_TICKS RK_MS_TO_TICKS(160)
#define POST_REPLY_BUSY_TICKS RK_MS_TO_TICKS(220)
#define ABANDON_CALL_TIMEOUT_TICKS RK_MS_TO_TICKS(120)
#define ABANDON_OBSERVE_TICKS RK_MS_TO_TICKS(180)

RK_DECLARE_TASK(sHandle, STask, sStack, STACKSIZE)
RK_DECLARE_TASK(hHandle, HTask, hStack, STACKSIZE)
RK_DECLARE_TASK(aHandle, ATask, aStack, STACKSIZE)

static volatile UINT highCallerGo;
static volatile UINT activeReplyObserved;
static volatile UINT highReplyObserved;
static volatile UINT abandonCallerGo;
static volatile UINT abandonCallerTimedOut;
static volatile UINT serverDone;

static ULONG activeReq = 0xA5A50005UL;
static ULONG activeReply = 0x5A5A0005UL;
static ULONG highReq = 0xA5A50002UL;
static ULONG highReply = 0x5A5A0002UL;
static ULONG abandonReq = 0xA5A5DEADUL;

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
    printf("XR FAIL %s t=%lu\r\n", wherePtr, kTickGetMs());
    K_ASSERT(0);
    TestFaultStop_();
}

static VOID TestCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("XR ERR %s err=%d\r\n", wherePtr, err);
        K_ASSERT(err == RK_ERR_SUCCESS);
        TestFaultStop_();
    }
}

static VOID WaitForFlag_(volatile UINT const *const flagPtr)
{
    while (*flagPtr == 0U)
    {
        kSleep(POLL_TICKS);
    }
}

static VOID BusyWaitTicks_(RK_TICK const ticks)
{
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), ticks);

    while (!K_TICK_EXPIRED(deadline))
    {
        RK_BARRIER
    }
}

static VOID ExpectReplyCopied_(RK_SYNCH_CALL_DATA const *const callPtr,
                               ULONG const expected,
                               CHAR const *const wherePtr)
{
    ULONG const *const replyPtr = (ULONG const *)callPtr->replyPtr;

    if ((replyPtr == NULL) || (*replyPtr != expected))
    {
        printf("XR REPLY %s exp=0x%lx got=0x%lx\r\n", wherePtr, expected,
               (replyPtr == NULL) ? 0UL : *replyPtr);
        TestFail_(wherePtr);
    }

    if (kTimeoutNodeIsArmed(&callPtr->caller->timeoutNode) == RK_TRUE)
    {
        TestFail_("reply left caller timeout armed");
    }
}

static VOID ExpectServerPrio_(RK_PRIO const expected,
                              CHAR const *const wherePtr)
{
    RK_PRIO const actual = RK_TASK_PRIO(sHandle);

    if (actual != expected)
    {
        printf("XR PRIO %s exp=%u got=%u\r\n", wherePtr, (UINT)expected,
               (UINT)actual);
        TestFail_(wherePtr);
    }
}

static VOID ExpectQueuedCaller_(RK_TASK_HANDLE const expected,
                                CHAR const *const wherePtr)
{
    if (sHandle->synchMesgCallers.size != 1UL)
    {
        printf("XR CALLQ %s size=%lu\r\n", wherePtr,
               sHandle->synchMesgCallers.size);
        TestFail_(wherePtr);
    }

    if (K_GET_TCB_ADDR(sHandle->synchMesgCallers.listDummy.nextPtr) !=
        expected)
    {
        printf("XR CALLQ %s unexpected head\r\n", wherePtr);
        TestFail_(wherePtr);
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
    TestCheckErr_(kTaskInit(&sHandle, STask, RK_NO_ARGS, "S", sStack,
                            STACKSIZE, S_PRIO, RK_PREEMPT),
                  "task S");
    TestCheckErr_(kTaskInit(&hHandle, HTask, RK_NO_ARGS, "H", hStack,
                            STACKSIZE, H_PRIO, RK_PREEMPT),
                  "task H");
    TestCheckErr_(kTaskInit(&aHandle, ATask, RK_NO_ARGS, "A", aStack,
                            STACKSIZE, A_PRIO, RK_PREEMPT),
                  "task A");
    TestCheckErr_(kSynchMesgInit(sHandle, sizeof(ULONG)), "server endpoint");

    printf("XR bench: extended rendezvous reply delivery\r\n");
}

VOID HTask(VOID *args)
{
    ULONG reply = 0UL;
    ULONG replyBytes = 0UL;
    RK_SYNCH_ATTR attr = {&highReq, sizeof(highReq), &reply,
                          sizeof(reply), &replyBytes};
    RK_UNUSEARGS

    WaitForFlag_(&highCallerGo);

    TestCheckErr_(kSynchMesgCall(sHandle, &attr, RK_WAIT_FOREVER),
                  "H call S");

    if ((reply != highReply) || (replyBytes != sizeof(highReply)))
    {
        TestFail_("H reply mismatch");
    }

    highReplyObserved = 1U;
    WaitForFlag_(&serverDone);
    TestPassStop_();
}

VOID ATask(VOID *args)
{
    ULONG reply = 0UL;
    ULONG replyBytes = 0UL;
    RK_SYNCH_ATTR attr = {&activeReq, sizeof(activeReq), &reply,
                          sizeof(reply), &replyBytes};
    RK_UNUSEARGS

    kSleep(ACTIVE_START_DELAY_TICKS);

    TestCheckErr_(kSynchMesgCall(sHandle, &attr, ACTIVE_CALL_TIMEOUT_TICKS),
                  "A call S");

    if ((reply != activeReply) || (replyBytes != sizeof(activeReply)))
    {
        TestFail_("A reply mismatch");
    }

    activeReplyObserved = 1U;
    WaitForFlag_(&abandonCallerGo);

    reply = 0UL;
    replyBytes = 0UL;
    attr.reqPtr = &abandonReq;
    attr.reqBytes = sizeof(abandonReq);
    attr.replyPtr = &reply;
    attr.replyMaxBytes = sizeof(reply);
    attr.replyBytesPtr = &replyBytes;

    RK_ERR const err =
        kSynchMesgCall(sHandle, &attr, ABANDON_CALL_TIMEOUT_TICKS);
    if (err != RK_ERR_TIMEOUT)
    {
        printf("XR ABANDON timeout exp=%d got=%d\r\n", RK_ERR_TIMEOUT, err);
        TestFail_("A abandoned timeout");
    }

    abandonCallerTimedOut = 1U;
    WaitForFlag_(&serverDone);
    TestPassStop_();
}

VOID STask(VOID *args)
{
    ULONG recv = 0UL;
    ULONG reqBytes = 0UL;
    RK_SYNCH_CALL_DATA call = {0};
    RK_UNUSEARGS

    TestCheckErr_(kSynchMesgAccept(&call, &recv, &reqBytes, RK_WAIT_FOREVER),
                  "S accept A");

    if ((recv != activeReq) || (reqBytes != sizeof(activeReq)) ||
        (call.caller != aHandle))
    {
        TestFail_("S accepted wrong active caller");
    }

    ExpectServerPrio_(A_PRIO, "S adopted A");
    printf("XR: S accepted A, prio exp=%u got=%u\r\n", (UINT)A_PRIO,
           (UINT)RK_RUNNING_PRIO);

    highCallerGo = 1U;
    kSleep(HIGH_QUEUE_OBSERVE_TICKS);

    ExpectQueuedCaller_(hHandle, "H queued while A active");
    ExpectServerPrio_(H_PRIO, "queued H raises active server");
    printf("XR: queued H raised active server, prio exp=%u got=%u\r\n",
           (UINT)H_PRIO, (UINT)RK_RUNNING_PRIO);

    TestCheckErr_(kSynchMesgReply(&call, &activeReply, sizeof(activeReply)),
                  "S reply A");

    ExpectReplyCopied_(&call, activeReply, "A reply copied");
    ExpectServerPrio_(S_PRIO, "S restored after A reply");
    printf("XR: S restored after A reply, prio exp=%u got=%u\r\n",
           (UINT)S_PRIO, (UINT)RK_RUNNING_PRIO);
    BusyWaitTicks_(POST_REPLY_BUSY_TICKS);

    TestCheckErr_(kSynchMesgAccept(&call, &recv, &reqBytes, RK_NO_WAIT),
                  "S accept H");

    if ((recv != highReq) || (reqBytes != sizeof(highReq)) ||
        (call.caller != hHandle))
    {
        TestFail_("S accepted wrong high caller");
    }

    ExpectServerPrio_(H_PRIO, "S adopted H");
    printf("XR: S accepted H, prio exp=%u got=%u\r\n", (UINT)H_PRIO,
           (UINT)RK_RUNNING_PRIO);
    TestCheckErr_(kSynchMesgReply(&call, &highReply, sizeof(highReply)),
                  "S reply H");

    ExpectReplyCopied_(&call, highReply, "H reply copied");
    ExpectServerPrio_(S_PRIO, "S restored after H reply");
    printf("XR: S restored after H reply, prio exp=%u got=%u\r\n",
           (UINT)S_PRIO, (UINT)RK_RUNNING_PRIO);

    abandonCallerGo = 1U;
    TestCheckErr_(kSynchMesgAccept(&call, &recv, &reqBytes, RK_WAIT_FOREVER),
                  "S accept abandoned A");

    if ((recv != abandonReq) || (reqBytes != sizeof(abandonReq)) ||
        (call.caller != aHandle))
    {
        TestFail_("S accepted wrong abandoned caller");
    }

    ExpectServerPrio_(A_PRIO, "S adopted abandoned A");
    printf("XR: S accepted abandoned A, prio exp=%u got=%u\r\n",
           (UINT)A_PRIO, (UINT)RK_RUNNING_PRIO);
    kSleep(ABANDON_OBSERVE_TICKS);

    if ((abandonCallerTimedOut == 0U) ||
        (aHandle->synchMesgCallState != RK_SYNCH_CALL_ABANDONED))
    {
        TestFail_("A did not abandon active call");
    }
    ExpectServerPrio_(S_PRIO, "S restored after A timeout");
    printf("XR: S restored after A timeout, prio exp=%u got=%u\r\n",
           (UINT)S_PRIO, (UINT)RK_RUNNING_PRIO);

    TestCheckErr_(kSynchMesgReply(&call, NULL, 0UL), "S closes abandoned A");
    ExpectServerPrio_(S_PRIO, "S restored after abandoned reply");
    printf("XR: S restored after abandoned reply, prio exp=%u got=%u\r\n",
           (UINT)S_PRIO, (UINT)RK_RUNNING_PRIO);

    serverDone = 1U;
    WaitForFlag_(&activeReplyObserved);
    WaitForFlag_(&highReplyObserved);
    printf("XR PASS extended rendezvous priority adoption\r\n");
    TestPassStop_();
}
