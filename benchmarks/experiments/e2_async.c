/* SPDX-License-Identifier: Apache-2.0 */
/* Finite pool in BOTH variants. EXP_CEILING=0 disables only its ceiling. */
#include "bench.h"

#ifndef EXP_CAPACITY
#define EXP_CAPACITY 4U
#endif
#ifndef EXP_CEILING
#define EXP_CEILING 3U
#endif
#if (EXP_CAPACITY < 1U) || (EXP_CAPACITY > 8U)
#error "EXP_CAPACITY must be 1..8"
#endif
#if (EXP_CEILING != 0U) && (EXP_CEILING != 3U)
#error "EXP_CEILING must be 0 (disabled) or 3"
#endif
#define FILLED RK_EVENT_1
#define PROD_DONE RK_EVENT_2
#define CONS_DONE RK_EVENT_3
#define MED_DONE RK_EVENT_4

typedef struct { ULONG sequence; } Payload;
RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, 320U)
RK_DECLARE_TASK(producerHandle, ProducerTask, producerStack, EXP_STACK)
RK_DECLARE_TASK(consumerHandle, ConsumerTask, consumerStack, EXP_STACK)
RK_DECLARE_TASK(mediumHandle, MediumTask, mediumStack, EXP_STACK)
RK_DECLARE_MESG_POOL(pool, poolBuffer, Payload, EXP_CAPACITY)
static RK_MESG_ENDPOINT producerEndpoint;
static RK_MESG_ENDPOINT consumerEndpoint;
static volatile UINT timeoutPhase;
static volatile UINT drainCount;
static volatile UINT consumerFinished;
static volatile UINT mediumBeforeDrain;
static volatile UINT workFinished;
static volatile UINT mediumBeforeWork;
static volatile ULONG workElapsed;
static volatile ULONG allocElapsed;
static volatile ULONG drainElapsed;
static volatile ULONG releaseTime;
static volatile RK_ERR allocResult;

static RK_PRIO ProtectedPrio_(RK_PRIO const nominal)
{
    return ((EXP_CEILING == 0U) ? nominal : (RK_PRIO)EXP_CEILING);
}

static VOID Send_(RK_MESG *const message, ULONG const sequence)
{
    Payload *const payload = kMesgPayload(message);
    payload->sequence = sequence;
    BenchOK_(kMesgSend(consumerHandle, message), "send transfers ownership");
    /* The message is no longer accessible to the sender. */
    BenchCheck_((RK_BOOL)(producerHandle->priority == 8U), "sender restored");
}

VOID kApplicationInit(VOID)
{
    BenchOK_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "E2ctrl",
                      controlStack, 320U, 1U, RK_PREEMPT), "control init");
    BenchOK_(kTaskInit(&producerHandle, ProducerTask, RK_NO_ARGS, "E2prod",
                      producerStack, EXP_STACK, 8U, RK_PREEMPT), "producer init");
    BenchOK_(kTaskInit(&consumerHandle, ConsumerTask, RK_NO_ARGS, "E2cons",
                      consumerStack, EXP_STACK, 10U, RK_PREEMPT), "consumer init");
    BenchOK_(kTaskInit(&mediumHandle, MediumTask, RK_NO_ARGS, "E2med",
                      mediumStack, EXP_STACK, 5U, RK_PREEMPT), "medium init");
    BenchOK_(kMesgEndpointInit(&producerEndpoint, producerHandle,
                              RK_MESG_SEND_ONLY), "producer endpoint");
    BenchOK_(kMesgEndpointInit(&consumerEndpoint, consumerHandle,
                              RK_MESG_SEND_RECV), "consumer endpoint");
    BenchOK_(kMesgPoolInit(&pool, poolBuffer, sizeof(Payload), EXP_CAPACITY,
                         (EXP_CEILING == 0U) ? RK_MESG_PRIO_CEILING_NONE
                                            : EXP_CEILING), "pool init");
}

VOID ProducerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        for (ULONG i = 0UL; i < EXP_CAPACITY; i++)
        {
            RK_MESG *message = NULL;
            BenchOK_(kMesgAlloc(&pool, &message, RK_NO_WAIT), "initial allocation");
            BenchCheck_((RK_BOOL)(producerHandle->priority == ProtectedPrio_(8U)),
                        "allocation ceiling");
            Send_(message, i);
        }
        BenchSignal_(controlHandle, FILLED);
        RK_MESG *extra = NULL;
        ULONG const start = BenchNow_();
        allocResult = kMesgAlloc(&pool, &extra,
                                (timeoutPhase != 0U) ? 50UL : EXP_LIMIT);
        allocElapsed = BenchNow_() - start;
        if (timeoutPhase != 0U)
        {
            BenchCheck_((RK_BOOL)((allocResult == RK_ERR_TIMEOUT) &&
                                  (extra == NULL)), "full pool timeout");
        }
        else
        {
            BenchOK_(allocResult, "pool handoff allocation");
            Send_(extra, EXP_CAPACITY);
        }
        BenchSignal_(controlHandle, PROD_DONE);
    }
}

VOID ConsumerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        for (ULONG i = 0UL; i < drainCount; i++)
        {
            RK_MESG *message = NULL;
            BenchOK_(kMesgWait(RK_ANY_TASK, &message, EXP_LIMIT), "receive");
            Payload const *const payload = kMesgPayloadConst(message);
            BenchCheck_((RK_BOOL)((payload->sequence == i) &&
                                  (message->owner == consumerHandle)),
                        "FIFO payload and receiver ownership");
            BenchCheck_((RK_BOOL)(consumerHandle->priority == ProtectedPrio_(10U)),
                        "receiver ceiling");
            BenchWork_(EXP_WORK);
            if (i + 1UL == drainCount)
            {
                workElapsed = BenchNow_() - releaseTime;
                workFinished = 1U;
            }
            BenchOK_(kMesgFree(message), "free returns block");
        }
        drainElapsed = BenchNow_() - releaseTime;
        consumerFinished = 1U;
        BenchSignal_(controlHandle, CONS_DONE);
    }
}

VOID MediumTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        BenchWork_(EXP_INTERFERENCE);
        mediumBeforeDrain = (consumerFinished == 0U) ? 1U : 0U;
        mediumBeforeWork = (workFinished == 0U) ? 1U : 0U;
        BenchSignal_(controlHandle, MED_DONE);
    }
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    BenchHeader_("E2");
    for (UINT phase = 0U; phase < 2U; phase++)
    {
        for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
        {
            timeoutPhase = phase;
            drainCount = EXP_CAPACITY + ((phase == 0U) ? 1U : 0U);
            consumerFinished = 0U;
            workFinished = 0U;
            BenchSignal_(producerHandle, EXP_GO);
            BenchWait_(FILLED);
            RK_TICK const deadline = K_TICK_ADD(kTickGet(), EXP_LIMIT);
            while (pool.waitingQueue.size != 1UL)
            {
                BenchCheck_((RK_BOOL)(K_TICK_EXPIRED(deadline) == 0U),
                            "producer allocation dependency");
                kSleep(1UL);
            }
            BenchCheck_((RK_BOOL)((pool.nFreeBlocks == 0UL) &&
                                  (consumerEndpoint.ownedMesgList.size == EXP_CAPACITY) &&
                                  (consumerEndpoint.mesgQueue.size == EXP_CAPACITY) &&
                                  (producerHandle->status == RK_WAITING_ALLOC) &&
                                  (producerHandle->waitingQueuePtr == &pool.waitingQueue) &&
                                  (producerEndpoint.allocDestPtr != NULL)),
                        "bounded pool saturation");
            BenchCheck_((RK_BOOL)(producerHandle->priority == ProtectedPrio_(8U)),
                        "blocked allocator ceiling");
            BenchCheck_((RK_BOOL)(consumerHandle->priority == ProtectedPrio_(10U)),
                        "queued messages already owned by receiver");
            RK_PRIO const blockedPrio = producerHandle->priority;
            RK_PRIO const queuedOwnerPrio = consumerHandle->priority;
            if (phase != 0U)
            {
                BenchWait_(PROD_DONE);
                BenchCheck_((RK_BOOL)((pool.waitingQueue.size == 0UL) &&
                                      (producerHandle->priority == 8U)),
                            "timeout removes allocation protection");
            }
            kSchLock();
            releaseTime = BenchNow_();
            BenchSignal_(mediumHandle, EXP_GO);
            BenchSignal_(consumerHandle, EXP_GO);
            kSchUnlock();
            BenchWait_(CONS_DONE | MED_DONE | ((phase == 0U) ? PROD_DONE : 0UL));
            BenchCheck_((RK_BOOL)((pool.nFreeBlocks == EXP_CAPACITY) &&
                                  (pool.waitingQueue.size == 0UL) &&
                                  (producerEndpoint.ownedMesgList.size == 0UL) &&
                                  (consumerEndpoint.ownedMesgList.size == 0UL) &&
                                  (consumerEndpoint.mesgQueue.size == 0UL) &&
                                  (producerHandle->priority == 8U) &&
                                  (consumerHandle->priority == 10U)), "pool conservation");
            if (EXP_CEILING != 0U)
            {
                BenchCheck_((RK_BOOL)(mediumBeforeWork == 0U),
                            "ceiling protects work until final free");
            }
            if (sample != 0U)
            {
                printf("EXP RESULT experiment=E2 ceiling=%u capacity=%u "
                       "path=%s sample=%u alloc_counts=%lu drain_counts=%lu "
                       "work_counts=%lu messages=%u medium_before_drain=%u "
                       "medium_before_work=%u blocked_prio=%u "
                       "queued_owner_prio=%u free_blocks=%lu\r\n",
                       (UINT)EXP_CEILING, (UINT)EXP_CAPACITY,
                       (phase == 0U) ? "handoff" : "timeout", sample,
                       allocElapsed, drainElapsed, workElapsed, drainCount,
                       mediumBeforeDrain, mediumBeforeWork,
                       (UINT)blockedPrio, (UINT)queuedOwnerPrio,
                       pool.nFreeBlocks);
            }
        }
    }
    printf("EXP PASS E2\r\n");
    BenchStop_();
}
