/* SPDX-License-Identifier: Apache-2.0 */
/* E2 architectural case study. Observer copies never replace tracked sources. */
#include "bench.h"
#include "e2_observe.h"
#include <ksch.h>
#include <stdint.h>

#ifndef EXP_E2_VARIANT
#define EXP_E2_VARIANT 0
#endif
#ifndef EXP_CAPACITY
#define EXP_CAPACITY 4U
#endif
#if (EXP_CAPACITY < 1U) || (EXP_CAPACITY > 8U)
#error "EXP_CAPACITY must be 1..8"
#endif
#if (EXP_E2_VARIANT == 0)
#define VARIANT_NAME "COMPOSED"
#elif (EXP_E2_VARIANT == 1)
#define VARIANT_NAME "RK0_NOC"
#elif (EXP_E2_VARIANT == 2)
#define VARIANT_NAME "RK0_CEIL"
#elif (EXP_E2_VARIANT == 3)
#define VARIANT_NAME "STATIC_PRIORITY"
#else
#error "Invalid EXP_E2_VARIANT"
#endif
#define PRODUCER_PRIO ((EXP_E2_VARIANT == 3) ? 3U : 8U)
#define RECEIVER_PRIO ((EXP_E2_VARIANT == 3) ? 3U : 10U)
#define FILLED RK_EVENT_1
#define PRODUCER_DONE RK_EVENT_2
#define RECEIVER_DONE RK_EVENT_3
#define MEDIUM_DONE RK_EVENT_4
#define PRIMED RK_EVENT_5
#define BACKGROUND_DONE RK_EVENT_6
#define PRODUCER_BG RK_EVENT_2
#define RECEIVER_BG RK_EVENT_2
#define LOG_CAPACITY 192U
#define NO_BLOCK 65535U
#define FREE 0U
#define PRODUCER 1U
#define RECEIVER 2U
#define RELEASING 3U

enum { REQUEST=1, GRANT, ALLOC_RETURN, SEND, RECEIVE, PRE_RELEASE, RELEASE,
       AVAILABLE, RELEASE_RETURN, MED_DISPATCH, MED_COMPLETE, PRIORITY,
       BG_DISPATCH, BG_COMPLETE, RECV_AVAILABLE, REJECT };
static CHAR const *const eventNames[] = {"none", "request", "grant", "alloc_return",
    "send", "receive", "pre_release", "release", "available", "release_return",
    "medium_dispatch", "medium_complete", "priority", "background_dispatch",
    "background_complete", "receive_available", "reject"};
typedef struct { ULONG sequence; ULONG check; } Payload;
typedef struct {
    ULONG time;
    ULONG value;
    uint16_t block;
    uint8_t kind, actor, subject, priority, contribution, nominal;
} Event;

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, 320U)
RK_DECLARE_TASK(producerHandle, ProducerTask, producerStack, EXP_STACK)
RK_DECLARE_TASK(receiverHandle, ReceiverTask, receiverStack, EXP_STACK)
RK_DECLARE_TASK(mediumHandle, MediumTask, mediumStack, 128U)
RK_DECLARE_MESG_POOL(pool, poolBuffer, Payload, EXP_CAPACITY)
#if (EXP_E2_VARIANT != 0)
static RK_MESG_ENDPOINT producerEndpoint, receiverEndpoint;
#else
static RK_MESG_QUEUE freeQueue, dataQueue;
static ULONG freePointers[EXP_CAPACITY], dataPointers[EXP_CAPACITY];
static UINT appOwner[EXP_CAPACITY];
#endif
static Event events[LOG_CAPACITY];
static UINT owner[EXP_CAPACITY];
static volatile UINT logCount, logging, faults;
static volatile UINT scenario, expectedItems, background;
static volatile ULONG sum;
static ULONG dataRelease, backgroundRelease;
static UINT sampleNumber;
static ULONG grantTimes[EXP_CAPACITY];
static ULONG custodyTimes[EXP_CAPACITY];

static UINT Block_(VOID const *block)
{
    ULONG const address = (ULONG)block;
    ULONG const start = (ULONG)poolBuffer;
    ULONG const bytes = RK_MESG_BLOCK_SIZE_BYTES(Payload);
    if ((address < start) || (address >= start + bytes * EXP_CAPACITY) ||
        (((address - start) % bytes) != 0UL)) { faults |= 1U; return (0U); }
    return ((UINT)((address - start) / bytes));
}

static UINT Actor_(RK_TASK_HANDLE task)
{
    if (task == producerHandle) { return (PRODUCER); }
    if (task == receiverHandle) { return (RECEIVER); }
    if (task == mediumHandle) { return (3U); }
    return (0U);
}

static VOID LogAt_(UINT kind, UINT block, RK_TASK_HANDLE subject, ULONG value, ULONG time)
{
    if (logging == 0U) { return; }
    if (logCount >= LOG_CAPACITY) { faults |= 2U; return; }
    Event *const event = &events[logCount++];
    event->time = time;
    event->value = value;
    event->block = (uint16_t)block;
    event->kind = (uint8_t)kind;
    event->actor = (uint8_t)Actor_(kTaskGetRunningHandle());
    event->subject = (uint8_t)Actor_(subject);
    event->priority = (uint8_t)subject->priority;
    event->nominal = (uint8_t)subject->prioNominal;
    event->contribution = 0U;
#if (EXP_E2_VARIANT == 2)
    if ((subject == producerHandle) || (subject == receiverHandle))
    {
        RK_MESG_ENDPOINT *const endpoint = subject->mesgEndpointPtr;
        if ((endpoint != NULL) && ((endpoint->ownedMesgList.size != 0UL) ||
            ((endpoint->allocDestPtr != NULL) && (subject->waitingQueuePtr != NULL))))
        { event->contribution = 3U; }
    }
#endif
}

static VOID Log_(UINT kind, UINT block, RK_TASK_HANDLE subject, ULONG value)
{
    RK_CR_AREA
    RK_CR_ENTER
    LogAt_(kind, block, subject, value, BenchNow_());
    RK_CR_EXIT
}

static VOID ReleaseAt_(VOID *block, ULONG time)
{
    UINT const i = Block_(block);
    if (owner[i] != RECEIVER) { faults |= 4U; }
    owner[i] = RELEASING;
    LogAt_(RELEASE, i, receiverHandle, 0UL, time);
}

static VOID AvailableAt_(VOID *block, ULONG time)
{
    UINT const i = Block_(block);
    if (owner[i] != RELEASING) { faults |= 8U; }
    owner[i] = FREE;
    LogAt_(AVAILABLE, i, receiverHandle, 0UL, time);
}

static VOID GrantAt_(VOID *block, RK_TASK_HANDLE task, ULONG time)
{
    UINT const i = Block_(block);
    if (owner[i] == RECEIVER) { ReleaseAt_(block, time); }
    if ((owner[i] != FREE) && (owner[i] != RELEASING)) { faults |= 16U; }
    if (task != producerHandle) { faults |= 32U; }
    owner[i] = PRODUCER;
    grantTimes[i] = time;
    LogAt_(GRANT, i, task, 0UL, grantTimes[i]);
}

VOID E2ObserveRelease(VOID *block)
{
    if (logging != 0U)
    {
        ULONG const time = BenchNow_();
#if (EXP_E2_VARIANT != 0)
        if (((RK_MESG *)block)->owner != NULL) { faults |= 1024U; }
#endif
        ReleaseAt_(block, time);
    }
}

VOID E2ObserveAvailable(VOID *block)
{
    if (logging != 0U) { AvailableAt_(block, BenchNow_()); }
}

VOID E2ObserveGrant(VOID *block, RK_TASK_HANDLE task)
{
    if (logging != 0U)
    {
        ULONG const time = BenchNow_();
#if (EXP_E2_VARIANT != 0)
        RK_MESG const *const message = block;
        if ((message->owner != task) || (message->poolPtr != &pool) ||
            (message->state != RK_MESG_STATE_ALLOCATED)) { faults |= 2048U; }
#endif
        GrantAt_(block, task, time);
    }
}

VOID E2ObserveSend(VOID *block, RK_TASK_HANDLE receiver)
{
    if (logging == 0U) { return; }
    ULONG const time = BenchNow_();
#if (EXP_E2_VARIANT != 0)
    RK_MESG const *const message = block;
    if ((message->owner != receiver) || (message->poolPtr != &pool)) { faults |= 4096U; }
#endif
    UINT const i = Block_(block);
    if ((owner[i] != PRODUCER) || (receiver != receiverHandle)) { faults |= 64U; }
    owner[i] = RECEIVER;
    custodyTimes[i] = time;
    Payload const *const payload = (Payload const *)((BYTE *)block + sizeof(RK_MESG));
    LogAt_(SEND, i, receiverHandle, payload->sequence, custodyTimes[i]);
}

VOID E2ObserveDirect(VOID *block, RK_TASK_HANDLE receiver)
{
    if (logging != 0U) { Log_(RECV_AVAILABLE, Block_(block), receiver, 1UL); }
}

VOID E2ObserveQueueSend(RK_MESG_QUEUE *queue, VOID const *pointer, RK_TASK_HANDLE receiver)
{
#if (EXP_E2_VARIANT == 0)
    if (logging == 0U) { return; }
    ULONG const time = BenchNow_();
    VOID *const block = *(VOID *const *)pointer;
    if (queue == &freeQueue)
    {
        ReleaseAt_(block, time);
        if (receiver != NULL) { GrantAt_(block, receiver, time); }
        else { AvailableAt_(block, time); }
    }
    else if (queue == &dataQueue)
    {
        E2ObserveSend(block, receiverHandle);
        if (receiver != NULL) { Log_(RECV_AVAILABLE, Block_(block), receiver, 1UL); }
    }
#else
    (VOID)queue; (VOID)pointer; (VOID)receiver;
#endif
}

VOID E2ObserveQueueRecv(RK_MESG_QUEUE *queue, VOID const *pointer)
{
#if (EXP_E2_VARIANT == 0)
    if ((logging != 0U) && (queue == &freeQueue))
    { E2ObserveGrant(*(VOID *const *)pointer, producerHandle); }
#else
    (VOID)queue; (VOID)pointer;
#endif
}

VOID E2ObservePriority(RK_TASK_HANDLE task, RK_PRIO oldPriority, RK_BOOL queuedBefore)
{
    if ((logging != 0U) && ((task == producerHandle) || (task == receiverHandle)))
    {
        UINT const index = logCount;
        Log_(PRIORITY, NO_BLOCK, task, oldPriority);
#if (EXP_E2_VARIANT == 2)
        /* Requeueing temporarily clears waitingQueuePtr. Observe the queue
         * captured by the native update BEFORE removal; on timeout it is NULL.
         */
        RK_MESG_ENDPOINT const *const endpoint = task->mesgEndpointPtr;
        if ((index < LOG_CAPACITY) && (endpoint != NULL))
        {
            events[index].contribution = ((endpoint->ownedMesgList.size != 0UL) ||
                ((endpoint->allocDestPtr != NULL) && (queuedBefore == RK_TRUE))) ? 3U : 0U;
        }
#else
        (VOID)index;
#endif
    }
    (VOID)queuedBefore;
}

static Payload *Payload_(VOID *block)
{
#if (EXP_E2_VARIANT == 0)
    return ((Payload *)((BYTE *)block + sizeof(RK_MESG)));
#else
    return ((Payload *)kMesgPayload(block));
#endif
}

static RK_ERR Allocate_(VOID **block, RK_TICK timeout)
{
    Log_(REQUEST, NO_BLOCK, producerHandle, timeout);
#if (EXP_E2_VARIANT == 0)
    RK_ERR const err = kMesgQueueRecv(&freeQueue, block, timeout);
    if (err == RK_ERR_SUCCESS)
    {
        UINT const i = Block_(*block);
        if (appOwner[i] != FREE) { faults |= 128U; }
        appOwner[i] = PRODUCER;
    }
#else
    RK_ERR const err = kMesgAlloc(&pool, (RK_MESG **)block, timeout);
#endif
    Log_((err == RK_ERR_SUCCESS) ? ALLOC_RETURN : REJECT,
         (err == RK_ERR_SUCCESS) ? Block_(*block) : NO_BLOCK, producerHandle, (ULONG)err);
    return (err);
}

static VOID Send_(VOID *block, ULONG sequence)
{
    Payload *const payload = Payload_(block);
    payload->sequence = sequence;
    payload->check = sequence ^ 0x5a5a1234UL;
    BenchWork_(EXP_WORK);
#if (EXP_E2_VARIANT == 0)
    RK_CR_AREA
    RK_CR_ENTER
    UINT const i = Block_(block);
    if (appOwner[i] != PRODUCER) { faults |= 256U; }
    appOwner[i] = RECEIVER;
    RK_ERR const err = kMesgQueueSend(&dataQueue, &block, RK_NO_WAIT);
    RK_CR_EXIT
#else
    RK_ERR const err = kMesgSend(receiverHandle, block);
#endif
    BenchOK_(err, "custody send");
}

static VOID Release_(VOID *block)
{
    UINT const i = Block_(block);
    Log_(PRE_RELEASE, i, receiverHandle, 0UL);
#if (EXP_E2_VARIANT == 0)
    RK_CR_AREA
    RK_CR_ENTER
    if (appOwner[i] != RECEIVER) { faults |= 512U; }
    appOwner[i] = FREE;
    RK_ERR const err = kMesgQueueSend(&freeQueue, &block, RK_NO_WAIT);
    RK_CR_EXIT
#else
    RK_ERR const err = kMesgFree(block);
#endif
    Log_(RELEASE_RETURN, i, receiverHandle, (ULONG)err);
    BenchOK_(err, "custody release");
}

VOID kApplicationInit(VOID)
{
    BenchOK_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "E2ctrl",
                      controlStack, 320U, 1U, RK_PREEMPT), "control init");
    BenchOK_(kTaskInit(&producerHandle, ProducerTask, RK_NO_ARGS, "E2prod",
                      producerStack, EXP_STACK, PRODUCER_PRIO, RK_PREEMPT), "producer init");
    BenchOK_(kTaskInit(&receiverHandle, ReceiverTask, RK_NO_ARGS, "E2recv",
                      receiverStack, EXP_STACK, RECEIVER_PRIO, RK_PREEMPT), "receiver init");
    BenchOK_(kTaskInit(&mediumHandle, MediumTask, RK_NO_ARGS, "E2med",
                      mediumStack, 128U, 5U, RK_PREEMPT), "medium init");
#if (EXP_E2_VARIANT == 0)
    BenchOK_(kMemPartitionInit(&pool, poolBuffer, RK_MESG_BLOCK_SIZE_BYTES(Payload),
                              EXP_CAPACITY), "generic partition");
    BenchOK_(kMesgQueueInit(&freeQueue, freePointers, 1UL, EXP_CAPACITY), "free pointer queue");
    BenchOK_(kMesgQueueInit(&dataQueue, dataPointers, 1UL, EXP_CAPACITY), "data pointer queue");
    for (UINT i = 0U; i < EXP_CAPACITY; i++)
    {
        VOID *block = kMemPartitionAlloc(&pool);
        BenchCheck_((RK_BOOL)(block != NULL), "reserve fixed block");
        BenchOK_(kMesgQueueSend(&freeQueue, &block, RK_NO_WAIT), "initial free pointer");
    }
#else
    BenchOK_(kMesgEndpointInit(&producerEndpoint, producerHandle, RK_MESG_SEND_ONLY), "producer endpoint");
    BenchOK_(kMesgEndpointInit(&receiverEndpoint, receiverHandle, RK_MESG_SEND_RECV), "receiver endpoint");
    BenchOK_(kMesgPoolInit(&pool, poolBuffer, sizeof(Payload), EXP_CAPACITY,
                         (EXP_E2_VARIANT == 2) ? 3U : RK_MESG_PRIO_CEILING_NONE), "native pool");
#endif
}

VOID ProducerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        RK_TASK_EVENT flags = 0UL;
        BenchOK_(kEventGet(EXP_GO | PRODUCER_BG, RK_EVENT_ANY, &flags, RK_WAIT_FOREVER), "producer start");
        if ((flags & PRODUCER_BG) != 0UL)
        {
            Log_(BG_DISPATCH, NO_BLOCK, producerHandle, 0UL);
            BenchWork_(EXP_WORK);
            Log_(BG_COMPLETE, NO_BLOCK, producerHandle, 0UL);
            BenchSignal_(controlHandle, BACKGROUND_DONE);
            continue;
        }
        if (scenario == 2U)
        {
            for (ULONG sequence = 0UL; sequence < expectedItems; sequence++)
            {
                VOID *block = NULL;
                BenchOK_(Allocate_(&block, EXP_LIMIT), "direct allocation");
                if (sequence == 0UL) { BenchSignal_(controlHandle, PRIMED); }
                Send_(block, sequence);
            }
        }
        else
        {
            for (ULONG sequence = 0UL; sequence < EXP_CAPACITY; sequence++)
            {
                VOID *block = NULL;
                BenchOK_(Allocate_(&block, RK_NO_WAIT), "initial allocation");
                Send_(block, sequence);
            }
            VOID *extra = NULL;
            BenchCheck_((RK_BOOL)(Allocate_(&extra, RK_NO_WAIT) == RK_ERR_BUFFER_EMPTY), "nonblocking exhaustion");
            BenchSignal_(controlHandle, FILLED);
            RK_ERR const err = Allocate_(&extra, (scenario == 1U) ? 50UL : EXP_LIMIT);
            if (scenario == 1U)
            { BenchCheck_((RK_BOOL)((err == RK_ERR_TIMEOUT) && (extra == NULL)), "exhaustion timeout"); }
            else { BenchOK_(err, "blocked allocator handoff"); Send_(extra, EXP_CAPACITY); }
        }
        BenchSignal_(controlHandle, PRODUCER_DONE);
    }
}

VOID ReceiverTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        RK_TASK_EVENT flags = 0UL;
        BenchOK_(kEventGet(EXP_GO | RECEIVER_BG, RK_EVENT_ANY, &flags, RK_WAIT_FOREVER), "receiver start");
        if ((flags & RECEIVER_BG) != 0UL)
        {
            Log_(BG_DISPATCH, NO_BLOCK, receiverHandle, 0UL);
            BenchWork_(EXP_WORK);
            Log_(BG_COMPLETE, NO_BLOCK, receiverHandle, 0UL);
            BenchSignal_(controlHandle, RECEIVER_DONE);
            continue;
        }
        for (ULONG sequence = 0UL; sequence < expectedItems; sequence++)
        {
            VOID *block = NULL;
#if (EXP_E2_VARIANT == 0)
            BenchOK_(kMesgQueueRecv(&dataQueue, &block, EXP_LIMIT), "pointer receive");
#else
            BenchOK_(kMesgWait(RK_ANY_TASK, (RK_MESG **)&block, EXP_LIMIT), "native receive");
#endif
            UINT const i = Block_(block);
            Log_(RECEIVE, i, receiverHandle, sequence);
            Payload *const payload = Payload_(block);
            BenchCheck_((RK_BOOL)((owner[i] == RECEIVER) && (payload->sequence == sequence) &&
                                  (payload->check == (sequence ^ 0x5a5a1234UL))), "order payload and unique custody");
            BenchWork_(EXP_WORK);
            sum += payload->sequence + 1UL;
            Release_(block);
        }
        BenchSignal_(controlHandle, RECEIVER_DONE);
    }
}

VOID MediumTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        Log_(MED_DISPATCH, NO_BLOCK, mediumHandle, background);
        BenchWork_(EXP_INTERFERENCE);
        Log_(MED_COMPLETE, NO_BLOCK, mediumHandle, background);
        BenchSignal_(controlHandle, MEDIUM_DONE);
    }
}

static VOID WaitBlocked_(UINT direct)
{
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), EXP_LIMIT);
    for (;;)
    {
#if (EXP_E2_VARIANT == 0)
        RK_BOOL const ready = (direct != 0U)
            ? (RK_BOOL)(dataQueue.waitingReceivers.size == 1UL)
            : (RK_BOOL)(freeQueue.waitingReceivers.size == 1UL);
#else
        RK_BOOL const ready = (direct != 0U)
            ? (RK_BOOL)(receiverEndpoint.waitingReceivers.size == 1UL)
            : (RK_BOOL)(pool.waitingQueue.size == 1UL);
#endif
        if (ready == RK_TRUE) { break; }
        BenchCheck_((RK_BOOL)(K_TICK_EXPIRED(deadline) == 0U), "actual blocking condition");
        kSleep(1UL);
    }
}

static VOID Conservation_(VOID)
{
    for (UINT i = 0U; i < EXP_CAPACITY; i++)
    {
        BenchCheck_((RK_BOOL)(owner[i] == FREE), "logical pool conservation");
#if (EXP_E2_VARIANT == 0)
        BenchCheck_((RK_BOOL)(appOwner[i] == FREE), "application ownership accounting");
#endif
    }
#if (EXP_E2_VARIANT == 0)
    BenchCheck_((RK_BOOL)((freeQueue.ringBuf.nFull == EXP_CAPACITY) &&
                          (dataQueue.ringBuf.nFull == 0UL) &&
                          (freeQueue.waitingReceivers.size == 0UL) &&
                          (dataQueue.waitingReceivers.size == 0UL)), "generic queue conservation");
#else
    BenchCheck_((RK_BOOL)((pool.nFreeBlocks == EXP_CAPACITY) && (pool.waitingQueue.size == 0UL) &&
                          (producerEndpoint.ownedMesgList.size == 0UL) &&
                          (receiverEndpoint.ownedMesgList.size == 0UL) &&
                          (receiverEndpoint.mesgQueue.size == 0UL)), "native pool conservation");
#endif
    BenchCheck_((RK_BOOL)((producerHandle->priority == PRODUCER_PRIO) &&
                          (receiverHandle->priority == RECEIVER_PRIO)), "custody priority restoration");
}

static VOID Report_(VOID)
{
    CHAR const *const path = (scenario == 0U) ? "handoff" : ((scenario == 1U) ? "timeout" : "direct");
    printf("EXP RESULT experiment=E2 case_study=1 variant=%s capacity=%u path=%s "
           "sample=%u items=%u checksum=%lu events=%u faults=%u payload_bytes=%u "
           "block_bytes=%lu work=%lu interference=%lu release=%lu background_release=%lu "
           "full_code=%u timeout_code=%u\r\n",
           VARIANT_NAME, (UINT)EXP_CAPACITY, path, sampleNumber, expectedItems, sum,
           logCount, faults, (UINT)sizeof(Payload), RK_MESG_BLOCK_SIZE_BYTES(Payload),
           (ULONG)EXP_WORK, (ULONG)EXP_INTERFERENCE, dataRelease, backgroundRelease,
           (UINT)RK_ERR_BUFFER_EMPTY, (UINT)RK_ERR_TIMEOUT);
    for (UINT i = 0U; i < logCount; i++)
    {
        Event const *const e = &events[i];
        printf("EXP EVENT experiment=E2 variant=%s capacity=%u path=%s sample=%u "
               "index=%u time=%lu kind=%s block=%u actor=%u subject=%u priority=%u "
               "nominal=%u contribution=%u value=%lu\r\n", VARIANT_NAME,
               (UINT)EXP_CAPACITY, path, sampleNumber, i, e->time, eventNames[e->kind],
               (UINT)e->block, (UINT)e->actor, (UINT)e->subject, (UINT)e->priority,
               (UINT)e->nominal, (UINT)e->contribution, e->value);
    }
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    BenchHeader_("E2");
    for (UINT path = 0U; path < 3U; path++)
    {
        scenario = path;
        expectedItems = EXP_CAPACITY + ((path == 1U) ? 0U : 1U);
        for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
        {
            sampleNumber = sample;
            Conservation_();
            logCount = 0U; faults = 0U; sum = 0UL; background = 0U; logging = 1U;
            if (path == 2U)
            {
                BenchSignal_(receiverHandle, EXP_GO);
                WaitBlocked_(1U);
                BenchSignal_(producerHandle, EXP_GO);
                BenchWait_(PRIMED);
            }
            else
            {
                BenchSignal_(producerHandle, EXP_GO);
                BenchWait_(FILLED);
                WaitBlocked_(0U);
                if (path == 1U) { BenchWait_(PRODUCER_DONE); }
            }
            kSchLock();
            dataRelease = BenchNow_();
            BenchSignal_(mediumHandle, EXP_GO);
            if (path != 2U) { BenchSignal_(receiverHandle, EXP_GO); }
            kSchUnlock();
            BenchWait_(RECEIVER_DONE | MEDIUM_DONE | ((path == 1U) ? 0UL : PRODUCER_DONE));
            Conservation_();
            background = 1U;
            kSchLock();
            backgroundRelease = BenchNow_();
            BenchSignal_(producerHandle, PRODUCER_BG);
            BenchSignal_(receiverHandle, RECEIVER_BG);
            BenchSignal_(mediumHandle, EXP_GO);
            kSchUnlock();
            BenchWait_(BACKGROUND_DONE | RECEIVER_DONE | MEDIUM_DONE);
            logging = 0U;
            Conservation_();
            BenchCheck_((RK_BOOL)((faults == 0U) &&
                (sum == (ULONG)expectedItems * ((ULONG)expectedItems + 1UL) / 2UL)), "logged custody and functional result");
            if (sample != 0U) { Report_(); }
        }
    }
#if (EXP_E2_VARIANT == 0)
    for (UINT i = 0U; i < EXP_CAPACITY; i++)
    {
        VOID *block = NULL;
        BenchOK_(kMesgQueueRecv(&freeQueue, &block, RK_NO_WAIT), "return reserved generic block");
        BenchOK_(kMemPartitionFree(&pool, block), "return generic backing block");
    }
    BenchCheck_((RK_BOOL)(pool.nFreeBlocks == EXP_CAPACITY), "physical partition conservation");
#endif
    printf("EXP PASS E2\r\n");
    BenchStop_();
}
