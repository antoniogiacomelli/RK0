/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E2: bounded asynchronous buffer transfer, with/without an ownership ceiling.
 *
 * The producer fills a finite pool while the receiver waits on an event, then
 * blocks allocating one extra buffer. Ownership transfers at send, not receive.
 * The controller observes that blocked allocation before releasing receiver and
 * independent medium-priority work. Handoff drains N+1 messages using N buffers;
 * timeout drains N after the extra allocation has timed out without a buffer.
 *
 * T0 is sampled immediately before the controller posts the two release events.
 * As priority 1, the controller cannot be preempted by either released task. It
 * then waits for all completion bits: no completion notification wakes it in
 * the measured interval. Release API and context-switch costs are included.
 * All normal serial output occurs after every worker has parked again.
 */
#include "bench_common.h"

#define E2_CONTROL_PRIO 1U
#define E2_CEILING_PRIO 3U
#define E2_MEDIUM_PRIO 5U
#define E2_PRODUCER_PRIO 8U
#define E2_RECEIVER_PRIO 10U
#define E2_START RK_EVENT_1
#define E2_PRODUCER_DONE RK_EVENT_2
#define E2_RECEIVER_DONE RK_EVENT_3
#define E2_MEDIUM_DONE RK_EVENT_4

typedef struct E2Payload
{
    uint32_t sequence;
} E2Payload;

typedef struct E2Config
{
    RK_MEM_PARTITION *pool;
    unsigned capacity;
    unsigned ceiling;
} E2Config;

typedef struct E2Times
{
    uint32_t release;
    uint32_t receiverStart;
    uint32_t workEnd;
    uint32_t drainEnd;
    uint32_t mediumStart;
    uint32_t mediumEnd;
    uint32_t allocStart;
    uint32_t allocEnd;
    unsigned received;
    RK_ERR allocResult;
} E2Times;

RK_DECLARE_TASK(e2Control, E2ControlTask, e2ControlStack, RS_STACK)
RK_DECLARE_TASK(e2Producer, E2ProducerTask, e2ProducerStack, RS_STACK)
RK_DECLARE_TASK(e2Receiver, E2ReceiverTask, e2ReceiverStack, RS_STACK)
RK_DECLARE_TASK(e2Medium, E2MediumTask, e2MediumStack, RS_STACK)
RK_DECLARE_MESG_POOL(e2Pool1Off, e2Pool1OffBuf, E2Payload, 1U)
RK_DECLARE_MESG_POOL(e2Pool1On, e2Pool1OnBuf, E2Payload, 1U)
RK_DECLARE_MESG_POOL(e2Pool4Off, e2Pool4OffBuf, E2Payload, 4U)
RK_DECLARE_MESG_POOL(e2Pool4On, e2Pool4OnBuf, E2Payload, 4U)

static RK_MESG_ENDPOINT e2ProducerEndpoint;
static RK_MESG_ENDPOINT e2ReceiverEndpoint;
static E2Config const e2Configs[] = {
    { &e2Pool1Off, 1U, 0U },
    { &e2Pool1On, 1U, 1U },
    { &e2Pool4Off, 4U, 0U },
    { &e2Pool4On, 4U, 1U }
};
static E2Config const *volatile e2Current;
static volatile unsigned e2TimeoutMode;
static E2Times volatile e2Times;
static RK_MESG *e2Prefilled[4];

static void E2WaitStart_(void)
{
    BenchRequire(kEventGet(E2_START, RK_EVENT_ALL, NULL, RK_WAIT_FOREVER),
                 "E2 worker start");
}

static int E2IsParked_(RK_TASK_HANDLE task)
{
    return (task->status == RK_SLEEPING_EV_FLAG) &&
           (task->flagsReq == E2_START);
}

static void E2WaitParked_(void)
{
    for (RK_TICK i = 0UL; i < RS_SETUP_TIMEOUT; ++i)
    {
        if (E2IsParked_(e2Producer) && E2IsParked_(e2Receiver) &&
            E2IsParked_(e2Medium))
        {
            return;
        }
        kSleep(1UL);
    }
    BenchFail("E2 workers did not park");
}

static void E2WaitBlockedAllocator_(E2Config const *config)
{
    for (RK_TICK i = 0UL; i < RS_SETUP_TIMEOUT; ++i)
    {
        RK_CR_AREA
        RK_CR_ENTER
        if ((e2Producer->status == RK_WAITING_ALLOC) &&
            (e2Producer->waitingQueuePtr == &config->pool->waitingQueue) &&
            (e2ProducerEndpoint.allocDestPtr != NULL))
        {
            BenchCheck(config->pool->nFreeBlocks == 0UL,
                       "E2 prefilled pool not exhausted");
            BenchCheck(config->pool->waitingQueue.size == 1UL,
                       "E2 allocator queue cardinality");
            BenchCheck(kTCBQPeek(&config->pool->waitingQueue) == e2Producer,
                       "E2 wrong blocked allocator");
            BenchCheck(e2ReceiverEndpoint.mesgQueue.size == config->capacity,
                       "E2 prefilled queue cardinality");
            BenchCheck(e2ReceiverEndpoint.ownedMesgList.size == config->capacity,
                       "E2 queued custody count");
            BenchCheck(e2ProducerEndpoint.ownedMesgList.size == 0UL,
                       "E2 producer retained sent ownership");
            for (unsigned n = 0U; n < config->capacity; ++n)
            {
                BenchCheck((e2Prefilled[n] != NULL) &&
                           (e2Prefilled[n]->owner == e2Receiver) &&
                           (e2Prefilled[n]->state == RK_MESG_STATE_QUEUED),
                           "E2 send-time ownership");
            }
            BenchCheck(RK_TASK_PRIO(e2Producer) ==
                       (config->ceiling ? E2_CEILING_PRIO : E2_PRODUCER_PRIO),
                       "E2 pending allocation priority");
            BenchCheck(RK_TASK_PRIO(e2Receiver) ==
                       (config->ceiling ? E2_CEILING_PRIO : E2_RECEIVER_PRIO),
                       "E2 queued receiver priority");
            BenchCheck(E2IsParked_(e2Receiver), "E2 receiver was not parked");
            RK_CR_EXIT
            return;
        }
        RK_CR_EXIT
        kSleep(1UL);
    }
    BenchFail("E2 producer did not block allocating");
}

static void E2Emit_(char const *scenario, unsigned sample,
                    E2Config const *config, unsigned timeoutMode)
{
    uint32_t const work = BenchDelta(e2Times.release, e2Times.workEnd);
    uint32_t const drain = BenchDelta(e2Times.release, e2Times.drainEnd);
    uint32_t const medium = BenchDelta(e2Times.release, e2Times.mediumEnd);
    BenchCheck(e2Times.received == config->capacity + (timeoutMode ? 0U : 1U),
               "E2 processed message count");
    BenchCheck(work <= drain, "E2 work/drain boundary order");
    BenchCheck((config->ceiling != 0U) ? (medium > work) : (medium < work),
               "E2 medium work relative to protected interval");
    BenchCheck(medium < drain, "E2 medium work missing from complete drain");
    BenchEmit(scenario, sample, "work_cycles", work, 1U);
    BenchEmit(scenario, sample, "drain_cycles", drain, 1U);
    BenchEmit(scenario, sample, "allocation_wait_cycles",
              BenchDelta(e2Times.allocStart, e2Times.allocEnd), 1U);
    BenchEmit(scenario, sample, "receiver_dispatch_cycles",
              BenchDelta(e2Times.release, e2Times.receiverStart), 1U);
    BenchEmit(scenario, sample, "medium_dispatch_cycles",
              BenchDelta(e2Times.release, e2Times.mediumStart), 1U);
    BenchEmit(scenario, sample, "medium_completion_cycles", medium, 1U);
    BenchEmit(scenario, sample, "medium_execution_interval_cycles",
              BenchDelta(e2Times.mediumStart, e2Times.mediumEnd), 1U);
}

int main(void)
{
    kCoreInit();
    BenchInit("E2");
    kInit();
    BenchFail("E2 scheduler returned");
    return 1;
}

VOID kApplicationInit(VOID)
{
    BenchRequire(kTaskInit(&e2Control, E2ControlTask, RK_NO_ARGS, "E2ctrl",
                          e2ControlStack, RS_STACK, E2_CONTROL_PRIO, RK_PREEMPT),
                 "E2 controller init");
    BenchRequire(kTaskInit(&e2Producer, E2ProducerTask, RK_NO_ARGS, "E2prod",
                          e2ProducerStack, RS_STACK, E2_PRODUCER_PRIO, RK_PREEMPT),
                 "E2 producer init");
    BenchRequire(kTaskInit(&e2Receiver, E2ReceiverTask, RK_NO_ARGS, "E2recv",
                          e2ReceiverStack, RS_STACK, E2_RECEIVER_PRIO, RK_PREEMPT),
                 "E2 receiver init");
    BenchRequire(kTaskInit(&e2Medium, E2MediumTask, RK_NO_ARGS, "E2mid",
                          e2MediumStack, RS_STACK, E2_MEDIUM_PRIO, RK_PREEMPT),
                 "E2 medium init");
    BenchKeepAwakeInit();
    BenchRequire(kMesgEndpointInit(&e2ProducerEndpoint, e2Producer,
                                  RK_MESG_SEND_ONLY), "E2 producer endpoint");
    BenchRequire(kMesgEndpointInit(&e2ReceiverEndpoint, e2Receiver,
                                  RK_MESG_SEND_RECV), "E2 receiver endpoint");
    BenchRequire(kMesgPoolInit(&e2Pool1Off, e2Pool1OffBuf, sizeof(E2Payload),
                              1U, RK_MESG_PRIO_CEILING_NONE), "E2 pool 1 off");
    BenchRequire(kMesgPoolInit(&e2Pool1On, e2Pool1OnBuf, sizeof(E2Payload),
                              1U, E2_CEILING_PRIO), "E2 pool 1 on");
    BenchRequire(kMesgPoolInit(&e2Pool4Off, e2Pool4OffBuf, sizeof(E2Payload),
                              4U, RK_MESG_PRIO_CEILING_NONE), "E2 pool 4 off");
    BenchRequire(kMesgPoolInit(&e2Pool4On, e2Pool4OnBuf, sizeof(E2Payload),
                              4U, E2_CEILING_PRIO), "E2 pool 4 on");
}

VOID E2ControlTask(VOID *args)
{
    RK_UNUSEARGS
    E2WaitParked_();
    for (unsigned c = 0U; c < sizeof(e2Configs) / sizeof(e2Configs[0]); ++c)
    {
        E2Config const *const config = &e2Configs[c];
        for (unsigned timeoutMode = 0U; timeoutMode <= 1U; ++timeoutMode)
        {
            char scenario[48];
            (void)snprintf(scenario, sizeof(scenario), "%s_n%u_ceiling_%s",
                           timeoutMode ? "timeout" : "handoff", config->capacity,
                           config->ceiling ? "on" : "off");
            for (unsigned sample = 0U; sample <= RS_SAMPLES; ++sample)
            {
                e2Current = config;
                e2TimeoutMode = timeoutMode;
                e2Times = (E2Times){ 0 };
                BenchCheck(config->pool->nFreeBlocks == config->capacity,
                           "E2 pool not free before trial");
                BenchSignal(e2Producer, E2_START);
                E2WaitBlockedAllocator_(config);
                if (timeoutMode != 0U)
                {
                    /* Receiver stays parked until the producer's 50 ms wait
                     * expires. This wait is intentionally in allocWait only. */
                    BenchWait(E2_PRODUCER_DONE);
                    BenchCheck(e2Times.allocResult == RK_ERR_TIMEOUT,
                               "E2 allocation did not time out");
                    BenchCheck(RK_TASK_PRIO(e2Producer) == E2_PRODUCER_PRIO,
                               "E2 timed-out producer priority");
                    BenchCheck(e2ProducerEndpoint.allocDestPtr == NULL,
                               "E2 stale allocation contribution");
                    BenchCheck(config->pool->waitingQueue.size == 0UL,
                               "E2 timed-out allocator still queued");
                    BenchCheck(config->pool->nFreeBlocks == 0UL,
                               "E2 timeout altered queued ownership");
                    BenchCheck(RK_TASK_PRIO(e2Receiver) ==
                               (config->ceiling ? E2_CEILING_PRIO : E2_RECEIVER_PRIO),
                               "E2 timeout withdrew unrelated custody ceiling");
                }

                e2Times.release = BenchNow();
                BenchSignal(e2Receiver, E2_START);
                BenchSignal(e2Medium, E2_START);
                BenchWait(E2_RECEIVER_DONE | E2_MEDIUM_DONE |
                          (timeoutMode ? 0UL : E2_PRODUCER_DONE));
                E2WaitParked_();

                BenchCheck(config->pool->nFreeBlocks == config->capacity,
                           "E2 pool not fully reclaimed");
                BenchCheck((config->pool->waitingQueue.size == 0UL) &&
                           (e2ReceiverEndpoint.mesgQueue.size == 0UL) &&
                           (e2ReceiverEndpoint.ownedMesgList.size == 0UL) &&
                           (e2ProducerEndpoint.ownedMesgList.size == 0UL) &&
                           (e2ProducerEndpoint.allocDestPtr == NULL),
                           "E2 outstanding resource relationship");
                BenchCheck((RK_TASK_PRIO(e2Producer) == E2_PRODUCER_PRIO) &&
                           (RK_TASK_PRIO(e2Receiver) == E2_RECEIVER_PRIO),
                           "E2 final priority restoration");
                E2Emit_(scenario, sample, config, timeoutMode);
            }
        }
    }
    BenchFinish();
}

VOID E2ProducerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        E2WaitStart_();
        E2Config const *const config = e2Current;
        for (unsigned i = 0U; i < config->capacity; ++i)
        {
            RK_MESG *message = NULL;
            BenchRequire(kMesgAlloc(config->pool, &message, RK_NO_WAIT),
                         "E2 prefill allocation");
            BenchCheck(message->owner == e2Producer, "E2 allocation owner");
            ((E2Payload *)kMesgPayload(message))->sequence = i;
            e2Prefilled[i] = message;
            BenchRequire(kMesgSend(e2Receiver, message), "E2 prefill send");
            BenchCheck(message->owner == e2Receiver, "E2 send owner");
        }

        RK_MESG *extra = NULL;
        e2Times.allocStart = BenchNow();
        RK_ERR const result = kMesgAlloc(config->pool, &extra,
                                        e2TimeoutMode ? RS_WAIT_TIMEOUT :
                                                        RS_SETUP_TIMEOUT);
        e2Times.allocEnd = BenchNow();
        e2Times.allocResult = result;
        if (e2TimeoutMode != 0U)
        {
            BenchCheck((result == RK_ERR_TIMEOUT) && (extra == NULL),
                       "E2 timeout allocation result");
        }
        else
        {
            BenchRequire(result, "E2 extra allocation handoff");
            BenchCheck((extra == e2Prefilled[0]) && (extra->owner == e2Producer),
                       "E2 direct handoff buffer and ownership");
            BenchCheck(RK_TASK_PRIO(e2Producer) ==
                       (config->ceiling ? E2_CEILING_PRIO : E2_PRODUCER_PRIO),
                       "E2 allocated producer priority");
            ((E2Payload *)kMesgPayload(extra))->sequence = config->capacity;
            BenchRequire(kMesgSend(e2Receiver, extra), "E2 extra send");
        }
        BenchSignal(e2Control, E2_PRODUCER_DONE);
    }
}

VOID E2ReceiverTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        E2WaitStart_();
        e2Times.receiverStart = BenchNow();
        E2Config const *const config = e2Current;
        unsigned const messages = config->capacity + (e2TimeoutMode ? 0U : 1U);
        for (unsigned i = 0U; i < messages; ++i)
        {
            RK_MESG *message = NULL;
            BenchRequire(kMesgWait(RK_ANY_TASK, &message, RS_SETUP_TIMEOUT),
                         "E2 receiver wait");
            BenchCheck((message != NULL) && (message->owner == e2Receiver),
                       "E2 receiver ownership");
            BenchCheck(((E2Payload *)kMesgPayload(message))->sequence == i,
                       "E2 sequence order");
            BenchCheck(RK_TASK_PRIO(e2Receiver) ==
                       (config->ceiling ? E2_CEILING_PRIO : E2_RECEIVER_PRIO),
                       "E2 processing priority");
            BenchBurn(RS_WORK);
            if (i + 1U == messages)
            {
                e2Times.workEnd = BenchNow();
                RK_ERR const result = kMesgFree(message);
                e2Times.drainEnd = BenchNow();
                BenchRequire(result, "E2 final free");
            }
            else
            {
                BenchRequire(kMesgFree(message), "E2 intermediate free");
            }
            e2Times.received = i + 1U;
        }
        BenchSignal(e2Control, E2_RECEIVER_DONE);
    }
}

VOID E2MediumTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        E2WaitStart_();
        e2Times.mediumStart = BenchNow();
        BenchBurn(RS_MEDIUM_WORK);
        e2Times.mediumEnd = BenchNow();
        BenchSignal(e2Control, E2_MEDIUM_DONE);
    }
}
