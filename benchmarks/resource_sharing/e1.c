/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E1: serialised resource work on the NUCLEO-F103RB.
 *
 * Contended trials compare PIP with immediate ceiling only. Invocation trials
 * use a separate, single-outstanding-request contract: H is released only
 * after L has returned. There is no application-created invocation queue.
 *
 * A release timestamp is taken immediately BEFORE the corresponding event
 * release API, with task switching locked during the release group. Thus the
 * release-to-first/done measurements include that release API and controller
 * dispatch overhead. "First" is the worker's first instrumented instruction
 * after its start-event wait returns, not a scheduler-internal dispatch hook.
 * No UART output occurs between trial start and all completion timestamps.
 */

#include "bench_common.h"

#define E1_CONTROL_PRIO 1U
#define E1_HIGH_PRIO 3U
#define E1_MEDIUM_PRIO 5U
#define E1_LOW_PRIO 10U
#define E1_SERVER_PRIO 12U

#define E1_START RK_EVENT_1
#define E1_ENTERED RK_EVENT_1
#define E1_LOW_DONE RK_EVENT_2
#define E1_HIGH_DONE RK_EVENT_3
#define E1_MEDIUM_DONE RK_EVENT_4
#define E1_ALL_DONE (E1_LOW_DONE | E1_HIGH_DONE | E1_MEDIUM_DONE)

enum E1Policy { E1_PIP, E1_CEILING, E1_INVOCATION };
enum E1Mode { E1_CONTENDED, E1_SINGLE, E1_BATCH };

typedef struct E1Stamps
{
    uint32_t hRelease;
    uint32_t hFirst;
    uint32_t hApiBegin;
    uint32_t hDone;
    uint32_t mRelease;
    uint32_t mFirst;
    uint32_t mDone;
    uint32_t lApiBegin;
    uint32_t lWorkBegin;
    uint32_t lWorkEnd;
    uint32_t lDone;
    uint32_t batchBegin;
    uint32_t batchEnd;
} E1Stamps;

RK_DECLARE_TASK(e1Controller, E1Controller, e1ControlStack, RS_STACK)
RK_DECLARE_TASK(e1High, E1High, e1HighStack, RS_STACK)
RK_DECLARE_TASK(e1Medium, E1Medium, e1MediumStack, RS_STACK)
RK_DECLARE_TASK(e1Low, E1Low, e1LowStack, RS_STACK)
RK_DECLARE_TASK(e1Server, E1Server, e1ServerStack, RS_STACK)

static RK_MUTEX e1PipMutex;
static RK_MUTEX e1CeilingMutex;
static RK_CHANNEL e1Channel;
static volatile enum E1Policy e1Policy;
static volatile enum E1Mode e1Mode;
static volatile E1Stamps e1Stamps;
static volatile ULONG e1ResourceWord;
static volatile UINT e1InService;
static volatile ULONG e1LowResult;
static volatile ULONG e1HighResult;
static volatile RK_PRIO e1LowWorkPriority;

static void E1WaitStart_(void)
{
    BenchRequire(kEventGet(E1_START, RK_EVENT_ALL, NULL, RK_WAIT_FOREVER),
                 "E1 worker start");
}

/* Called only outside measured intervals, while all work has completed. */
static void E1WaitParked_(void)
{
    for (RK_TICK tick = 0UL; tick < RS_SETUP_TIMEOUT; ++tick)
    {
        RK_BOOL parked;
        RK_CR_AREA
        RK_CR_ENTER
        parked = (RK_BOOL)((e1High->status == RK_SLEEPING_EV_FLAG) &&
                          (e1Medium->status == RK_SLEEPING_EV_FLAG) &&
                          (e1Low->status == RK_SLEEPING_EV_FLAG) &&
                          (e1Server->status == RK_RECEIVING) &&
                          (e1Channel.receiverWaiting == RK_TRUE) &&
                          (e1Channel.state == RK_CHANNEL_IDLE));
        RK_CR_EXIT
        if (parked == RK_TRUE)
        {
            return;
        }
        kSleep(1UL);
    }
    BenchFail("E1 workers failed to park");
}

static RK_MUTEX *E1Mutex_(void)
{
    return (e1Policy == E1_PIP) ? &e1PipMutex : &e1CeilingMutex;
}

/* Same protected operation for mutex owners and the invocation server. */
static ULONG E1ResourceWork_(RK_BOOL lowRequest)
{
    BenchCheck(e1InService == 0U, "E1 overlapping resource work");
    e1InService = 1U;
    if ((lowRequest == RK_TRUE) && (e1Mode != E1_BATCH))
    {
        e1Stamps.lWorkBegin = BenchNow();
        BenchSignal(e1Controller, E1_ENTERED);
        /* The controller has now released the relevant interference tasks. */
        e1LowWorkPriority = RK_RUNNING_PRIO;
        RK_PRIO const expected =
            ((e1Policy == E1_CEILING) || (e1Mode == E1_CONTENDED))
                ? E1_HIGH_PRIO : E1_LOW_PRIO;
        BenchCheck(RK_RUNNING_PRIO == expected,
                   "E1 low service effective priority");
    }
    if (e1Mode != E1_BATCH)
    {
        BenchBurn(RS_WORK);
    }
    ULONG const result = ++e1ResourceWord;
    if ((lowRequest == RK_TRUE) && (e1Mode != E1_BATCH))
    {
        e1Stamps.lWorkEnd = BenchNow();
    }
    e1InService = 0U;
    return result;
}

static ULONG E1Operation_(RK_BOOL lowRequest)
{
    if (e1Policy == E1_INVOCATION)
    {
        ULONG request = (lowRequest == RK_TRUE) ? 1UL : 2UL;
        ULONG reply = 0UL;
        ULONG replyBytes = 0UL;
        RK_CHANNEL_ATTR attr = {&request, sizeof(request), &reply,
                                sizeof(reply), &replyBytes};
        BenchRequire(kChannelCall(&e1Channel, &attr, RS_SETUP_TIMEOUT),
                     "E1 invocation");
        BenchCheck(replyBytes == sizeof(reply), "E1 invocation reply size");
        return reply;
    }

    RK_MUTEX *const mutex = E1Mutex_();
    BenchRequire(kMutexLock(mutex, RS_SETUP_TIMEOUT), "E1 mutex lock");
    RK_PRIO const expected =
        ((lowRequest == RK_FALSE) || (e1Policy == E1_CEILING))
            ? E1_HIGH_PRIO : E1_LOW_PRIO;
    BenchCheck(RK_RUNNING_PRIO == expected, "E1 initial mutex-owner priority");
    ULONG const result = E1ResourceWork_(lowRequest);
    BenchRequire(kMutexUnlock(mutex), "E1 mutex unlock");
    return result;
}

VOID E1High(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        E1WaitStart_();
        e1Stamps.hFirst = BenchNow();
        e1Stamps.hApiBegin = BenchNow();
        e1HighResult = E1Operation_(RK_FALSE);
        e1Stamps.hDone = BenchNow();
        BenchSignal(e1Controller, E1_HIGH_DONE);
    }
}

VOID E1Medium(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        E1WaitStart_();
        e1Stamps.mFirst = BenchNow();
        BenchBurn(RS_MEDIUM_WORK);
        e1Stamps.mDone = BenchNow();
        BenchSignal(e1Controller, E1_MEDIUM_DONE);
    }
}

VOID E1Low(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        E1WaitStart_();
        if (e1Mode == E1_BATCH)
        {
            e1Stamps.batchBegin = BenchNow();
            for (unsigned i = 0U; i < RS_BATCH; ++i)
            {
                e1LowResult = E1Operation_(RK_TRUE);
            }
            e1Stamps.batchEnd = BenchNow();
        }
        else
        {
            e1Stamps.lApiBegin = BenchNow();
            e1LowResult = E1Operation_(RK_TRUE);
            e1Stamps.lDone = BenchNow();
        }
        BenchCheck(RK_RUNNING_PRIO == E1_LOW_PRIO,
                   "E1 low priority restored");
        BenchSignal(e1Controller, E1_LOW_DONE);
    }
}

VOID E1Server(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        ULONG request = 0UL;
        ULONG requestBytes = 0UL;
        RK_CHANNEL_CALL_DATA call = {0};
        BenchRequire(kChannelAccept(&e1Channel, &call, &request,
                                    sizeof(request), &requestBytes,
                                    RK_WAIT_FOREVER), "E1 server accept");
        BenchCheck(e1Policy == E1_INVOCATION, "E1 unexpected invocation");
        BenchCheck(e1Mode != E1_CONTENDED, "E1 invocation contract");
        BenchCheck(requestBytes == sizeof(request), "E1 request size");
        RK_BOOL const lowRequest = (RK_BOOL)(request == 1UL);
        BenchCheck((request == 1UL) || (request == 2UL), "E1 request value");
        BenchCheck(call.caller == ((lowRequest == RK_TRUE) ? e1Low : e1High),
                   "E1 request caller");
        BenchCheck(RK_RUNNING_PRIO == ((lowRequest == RK_TRUE)
                                      ? E1_LOW_PRIO : E1_HIGH_PRIO),
                   "E1 server adopted caller priority");
        ULONG const reply = E1ResourceWork_(lowRequest);
        BenchRequire(kChannelReply(&call, &reply, sizeof(reply)),
                     "E1 server reply");
        /* A new caller can donate before Reply returns to this server.
         * Check restoration once quiescent in the controller, not here. */
    }
}

static void E1Release_(RK_BOOL high, RK_BOOL medium)
{
    kSchLock();
    if (high == RK_TRUE)
    {
        e1Stamps.hRelease = BenchNow();
        BenchSignal(e1High, E1_START);
    }
    if (medium == RK_TRUE)
    {
        e1Stamps.mRelease = BenchNow();
        BenchSignal(e1Medium, E1_START);
    }
    kSchUnlock();
}

static void E1CheckFinished_(void)
{
    BenchCheck(e1InService == 0U, "E1 resource left active");
    BenchCheck(e1PipMutex.ownerPtr == NULL, "E1 PIP owner not withdrawn");
    BenchCheck(e1CeilingMutex.ownerPtr == NULL, "E1 ceiling owner not withdrawn");
    BenchCheck(RK_TASK_PRIO(e1Low) == E1_LOW_PRIO, "E1 low final priority");
    BenchCheck(RK_TASK_PRIO(e1High) == E1_HIGH_PRIO, "E1 high final priority");
    BenchCheck(RK_TASK_PRIO(e1Server) == E1_SERVER_PRIO,
               "E1 server final priority");
    if (e1Mode == E1_BATCH)
    {
        BenchCheck((e1ResourceWord == RS_BATCH) && (e1LowResult == RS_BATCH),
                   "E1 batch resource result");
        return;
    }
    BenchCheck((e1ResourceWord == 2UL) && (e1LowResult == 1UL) &&
               (e1HighResult == 2UL), "E1 serialised resource result");
    uint32_t const workEnd = BenchDelta(e1Stamps.lApiBegin, e1Stamps.lWorkEnd);
    uint32_t const mFirst = BenchDelta(e1Stamps.lApiBegin, e1Stamps.mFirst);
    uint32_t const hFirst = BenchDelta(e1Stamps.lApiBegin, e1Stamps.hFirst);
    if (e1Mode == E1_CONTENDED)
    {
        BenchCheck(mFirst >= workEnd, "E1 medium ran inside protected work");
        BenchCheck((e1Policy == E1_PIP) ? (hFirst < workEnd)
                                      : (hFirst >= workEnd),
                   "E1 PIP/ceiling first-execution ordering");
    }
    else
    {
        BenchCheck((e1Policy == E1_CEILING) ? (mFirst >= workEnd)
                                          : (mFirst < workEnd),
                   "E1 single-request medium ordering");
    }
}

static void E1Report_(char const *scenario, unsigned sample)
{
    if (e1Mode == E1_BATCH)
    {
        BenchEmit(scenario, sample, "transaction_batch",
                  BenchDelta(e1Stamps.batchBegin, e1Stamps.batchEnd), RS_BATCH);
        return;
    }
    BenchEmit(scenario, sample, "h_release_to_first",
              BenchDelta(e1Stamps.hRelease, e1Stamps.hFirst), 1U);
    BenchEmit(scenario, sample, "h_release_to_done",
              BenchDelta(e1Stamps.hRelease, e1Stamps.hDone), 1U);
    BenchEmit(scenario, sample, "h_operation",
              BenchDelta(e1Stamps.hApiBegin, e1Stamps.hDone), 1U);
    BenchEmit(scenario, sample, "m_release_to_first",
              BenchDelta(e1Stamps.mRelease, e1Stamps.mFirst), 1U);
    BenchEmit(scenario, sample, "m_release_to_done",
              BenchDelta(e1Stamps.mRelease, e1Stamps.mDone), 1U);
    BenchEmit(scenario, sample, "m_execution",
              BenchDelta(e1Stamps.mFirst, e1Stamps.mDone), 1U);
    BenchEmit(scenario, sample, "l_resource_work_interval",
              BenchDelta(e1Stamps.lWorkBegin, e1Stamps.lWorkEnd), 1U);
    BenchEmit(scenario, sample, "l_operation",
              BenchDelta(e1Stamps.lApiBegin, e1Stamps.lDone), 1U);
    if (sample != 0U)
    {
        printf("RS CHECK experiment=E1 case=%s sample=%u "
               "serialised=1 low_work_priority=%u\r\n",
               scenario, sample, (unsigned)e1LowWorkPriority);
    }
}

static void E1RunCase_(enum E1Mode mode, enum E1Policy policy,
                       char const *scenario)
{
    for (unsigned sample = 0U; sample <= RS_SAMPLES; ++sample)
    {
        E1WaitParked_();
        e1Mode = mode;
        e1Policy = policy;
        e1Stamps = (E1Stamps){0};
        e1ResourceWord = 0UL;
        e1LowResult = 0UL;
        e1HighResult = 0UL;
        e1InService = 0U;
        e1LowWorkPriority = E1_LOW_PRIO;
        BenchSignal(e1Low, E1_START);
        if (mode == E1_BATCH)
        {
            BenchWait(E1_LOW_DONE);
        }
        else
        {
            BenchWait(E1_ENTERED);
            E1Release_((RK_BOOL)(mode == E1_CONTENDED), RK_TRUE);
            if (mode == E1_CONTENDED)
            {
                BenchWait(E1_ALL_DONE);
            }
            else
            {
                BenchWait(E1_LOW_DONE | E1_MEDIUM_DONE);
                E1Release_(RK_TRUE, RK_FALSE);
                BenchWait(E1_HIGH_DONE);
            }
        }
        E1WaitParked_();
        E1CheckFinished_();
        E1Report_(scenario, sample);
    }
}

VOID E1Controller(VOID *args)
{
    RK_UNUSEARGS
    E1RunCase_(E1_CONTENDED, E1_PIP, "contended-pip");
    E1RunCase_(E1_CONTENDED, E1_CEILING, "contended-ceiling");
    E1RunCase_(E1_SINGLE, E1_PIP, "single-pip");
    E1RunCase_(E1_SINGLE, E1_CEILING, "single-ceiling");
    E1RunCase_(E1_SINGLE, E1_INVOCATION, "single-invocation");
    E1RunCase_(E1_BATCH, E1_PIP, "batch-pip");
    E1RunCase_(E1_BATCH, E1_CEILING, "batch-ceiling");
    E1RunCase_(E1_BATCH, E1_INVOCATION, "batch-invocation");
    BenchFinish();
}

int main(void)
{
    kCoreInit();
    BenchInit("E1");
    kInit();
    BenchFail("E1 scheduler returned");
    return 1;
}

VOID kApplicationInit(VOID)
{
    BenchRequire(kTaskInit(&e1Controller, E1Controller, RK_NO_ARGS, "E1ctrl",
                          e1ControlStack, RS_STACK, E1_CONTROL_PRIO, RK_PREEMPT),
                 "E1 controller init");
    BenchRequire(kTaskInit(&e1High, E1High, RK_NO_ARGS, "E1H", e1HighStack,
                          RS_STACK, E1_HIGH_PRIO, RK_PREEMPT), "E1 high init");
    BenchRequire(kTaskInit(&e1Medium, E1Medium, RK_NO_ARGS, "E1M", e1MediumStack,
                          RS_STACK, E1_MEDIUM_PRIO, RK_PREEMPT), "E1 medium init");
    BenchRequire(kTaskInit(&e1Low, E1Low, RK_NO_ARGS, "E1L", e1LowStack,
                          RS_STACK, E1_LOW_PRIO, RK_PREEMPT), "E1 low init");
    BenchRequire(kTaskInit(&e1Server, E1Server, RK_NO_ARGS, "E1srv", e1ServerStack,
                          RS_STACK, E1_SERVER_PRIO, RK_PREEMPT), "E1 server init");
    BenchKeepAwakeInit();
    BenchRequire(kMutexInit(&e1PipMutex, RK_PRIO_INHERITANCE, RK_NO_CEILING),
                 "E1 PIP mutex init");
    BenchRequire(kMutexInit(&e1CeilingMutex, RK_PRIO_CEILING, E1_HIGH_PRIO),
                 "E1 ceiling mutex init");
    BenchRequire(kChannelInit(&e1Channel, &e1Server, SYNCH_INVOCATION),
                 "E1 channel init");
}
