/* SPDX-License-Identifier: Apache-2.0 */
/* EXP_E1_MODE: 0=PIP, 1=immediate ceiling, 2=invocation, 3=unprotected.
 * Invocation has one outstanding request; contended mutex trials are separate.
 */
#include "bench.h"

#ifndef EXP_E1_MODE
#define EXP_E1_MODE 0
#endif
#if (EXP_E1_MODE == 0)
#define E1_NAME "pip"
#define E1_PROTOCOL RK_PRIO_INHERITANCE
#elif (EXP_E1_MODE == 1)
#define E1_NAME "ceiling"
#define E1_PROTOCOL RK_PRIO_CEILING
#elif (EXP_E1_MODE == 2)
#define E1_NAME "invocation"
#elif (EXP_E1_MODE == 3)
#define E1_NAME "unprotected"
#define E1_PROTOCOL RK_NO_PROTOCOL
#else
#error "Unknown EXP_E1_MODE"
#endif

#define ENTERED RK_EVENT_1
#define LOW_DONE RK_EVENT_2
#define HIGH_DONE RK_EVENT_3
#define MED_DONE RK_EVENT_4
#define COST_GO RK_EVENT_2
#define COST_DONE RK_EVENT_5

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, 320U)
RK_DECLARE_TASK(lowHandle, LowTask, lowStack, EXP_STACK)
RK_DECLARE_TASK(highHandle, HighTask, highStack, EXP_STACK)
RK_DECLARE_TASK(mediumHandle, MediumTask, mediumStack, EXP_STACK)
#if (EXP_E1_MODE == 2)
RK_DECLARE_TASK(serverHandle, ServerTask, serverStack, EXP_STACK)
static RK_CHANNEL channel;
#else
static RK_MUTEX mutex;
#endif
static volatile ULONG resource;
static volatile UINT inResource;
static volatile UINT lowFinished;
static volatile UINT highFinished;
static volatile UINT mediumBeforeLow;
static volatile UINT mediumBeforeHigh;
static volatile UINT lowWorkFinished;
static volatile UINT mediumBeforeLowWork;
static volatile ULONG lowElapsed;
static volatile ULONG highElapsed;
static volatile ULONG highDispatchDelay;
static volatile ULONG highRelease;
static volatile ULONG mediumElapsed;
static volatile RK_PRIO lowEnterPrio;
static volatile RK_PRIO lowWorkPrio;
static volatile RK_PRIO highWorkPrio;
static volatile UINT costPhase;

static ULONG Operation_(UINT const id)
{
    BenchCheck_((RK_BOOL)(inResource == 0U), "overlapping resource operations");
    inResource = 1U;
    if (id == 0U)
    {
        lowEnterPrio = RK_RUNNING_PRIO;
        if (costPhase == 0U) { BenchSignal_(controlHandle, ENTERED); }
        lowWorkPrio = RK_RUNNING_PRIO;
    }
    else { highWorkPrio = RK_RUNNING_PRIO; }
    BenchWork_(EXP_WORK);
    resource++;
    inResource = 0U;
    if (id == 0U) { lowWorkFinished = 1U; }
    return (resource);
}

static ULONG Access_(UINT const id)
{
#if (EXP_E1_MODE == 2)
    ULONG result = 0UL;
    ULONG bytes = 0UL;
    RK_CHANNEL_ATTR attr = {&id, sizeof(id), &result, sizeof(result), &bytes};
    BenchOK_(kChannelCall(&channel, &attr, EXP_LIMIT), "invocation call");
    BenchCheck_((RK_BOOL)(bytes == sizeof(result)), "reply size");
    return (result);
#else
    BenchOK_(kMutexLock(&mutex, EXP_LIMIT), "resource lock");
    ULONG const result = Operation_(id);
    BenchOK_(kMutexUnlock(&mutex), "resource unlock");
    return (result);
#endif
}

VOID kApplicationInit(VOID)
{
    BenchOK_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "E1ctrl",
                      controlStack, 320U, 1U, RK_PREEMPT), "control init");
    BenchOK_(kTaskInit(&lowHandle, LowTask, RK_NO_ARGS, "E1low",
                      lowStack, EXP_STACK, 10U, RK_PREEMPT), "low init");
    BenchOK_(kTaskInit(&highHandle, HighTask, RK_NO_ARGS, "E1high",
                      highStack, EXP_STACK, 3U, RK_PREEMPT), "high init");
    BenchOK_(kTaskInit(&mediumHandle, MediumTask, RK_NO_ARGS, "E1med",
                      mediumStack, EXP_STACK, 5U, RK_PREEMPT), "medium init");
#if (EXP_E1_MODE == 2)
    BenchOK_(kTaskInit(&serverHandle, ServerTask, RK_NO_ARGS, "E1server",
                      serverStack, EXP_STACK, 12U, RK_PREEMPT), "server init");
    BenchOK_(kChannelInit(&channel, &serverHandle, SYNCH_INVOCATION),
             "channel init");
#else
    BenchOK_(kMutexInit(&mutex, E1_PROTOCOL, 3U), "mutex init");
#endif
}

VOID LowTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        RK_TASK_EVENT flags = 0UL;
        BenchOK_(kEventGet(EXP_GO | COST_GO, RK_EVENT_ANY, &flags,
                           RK_WAIT_FOREVER), "low start");
        if ((flags & COST_GO) != 0UL)
        {
            for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
            {
                resource = 0UL;
                ULONG const costStart = BenchNow_();
                ULONG result = 0UL;
                for (UINT i = 0U; i < EXP_ITERATIONS; i++) { result = Access_(0U); }
                ULONG const elapsed = BenchNow_() - costStart;
                BenchCheck_((RK_BOOL)(result == EXP_ITERATIONS), "cost operations");
                if (sample != 0U)
                {
                    printf("EXP RESULT experiment=E1 mode=%s path=cost contract=single "
                           "sample=%u iterations=%u operation_counts=%lu\r\n",
                           E1_NAME, sample, (UINT)EXP_ITERATIONS, elapsed);
                }
            }
            BenchSignal_(controlHandle, COST_DONE);
            continue;
        }
        ULONG const start = BenchNow_();
        ULONG const result = Access_(0U);
        lowElapsed = BenchNow_() - start;
        BenchCheck_((RK_BOOL)(result == 1UL), "low result");
        lowFinished = 1U;
        BenchSignal_(controlHandle, LOW_DONE);
    }
}

VOID HighTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        highDispatchDelay = BenchNow_() - highRelease;
        ULONG const result = Access_(1U);
        highElapsed = BenchNow_() - highRelease;
        BenchCheck_((RK_BOOL)(result == 2UL), "high result");
        highFinished = 1U;
        BenchSignal_(controlHandle, HIGH_DONE);
    }
}

VOID MediumTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        ULONG const start = BenchNow_();
        BenchWork_(EXP_INTERFERENCE);
        mediumElapsed = BenchNow_() - start;
        mediumBeforeLow = (lowFinished == 0U) ? 1U : 0U;
        mediumBeforeHigh = (highFinished == 0U) ? 1U : 0U;
        mediumBeforeLowWork = (lowWorkFinished == 0U) ? 1U : 0U;
        BenchSignal_(controlHandle, MED_DONE);
    }
}

#if (EXP_E1_MODE == 2)
VOID ServerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        UINT id = 0U;
        RK_CHANNEL_CALL_DATA call = {0};
        ULONG bytes = 0UL;
        BenchOK_(kChannelAccept(&channel, &call, &id, sizeof(id), &bytes,
                                RK_WAIT_FOREVER), "accept");
        BenchCheck_((RK_BOOL)(bytes == sizeof(id)), "request size");
        ULONG const result = Operation_(id);
        BenchOK_(kChannelReply(&call, &result, sizeof(result)), "reply");
    }
}
#endif

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    BenchHeader_("E1");
#if (EXP_E1_MODE == 2)
    UINT const cases = 1U;
#else
    UINT const cases = 2U;
#endif
    for (UINT contended = 0U; contended < cases; contended++)
    {
        for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
        {
            resource = 0UL;
            lowFinished = 0U;
            lowWorkFinished = 0U;
            highFinished = 0U;
            BenchSignal_(lowHandle, EXP_GO);
            BenchWait_(ENTERED);
            kSchLock();
            BenchSignal_(mediumHandle, EXP_GO);
            if (contended != 0U)
            {
                highRelease = BenchNow_();
                BenchSignal_(highHandle, EXP_GO);
            }
            kSchUnlock();
            if (contended == 0U)
            {
                BenchWait_(LOW_DONE);
                highRelease = BenchNow_();
                BenchSignal_(highHandle, EXP_GO);
                BenchWait_(HIGH_DONE | MED_DONE);
            }
            else { BenchWait_(LOW_DONE | HIGH_DONE | MED_DONE); }
            BenchCheck_((RK_BOOL)((resource == 2UL) && (inResource == 0U)),
                        "final resource state");
            BenchCheck_((RK_BOOL)((lowHandle->priority == 10U) &&
                                  (highHandle->priority == 3U)), "restoration");
#if (EXP_E1_MODE == 2)
            BenchCheck_((RK_BOOL)((channel.state == RK_CHANNEL_IDLE) &&
                                  (serverHandle->priority == 12U)),
                        "invocation closed and server restored");
            RK_PRIO const ownerRestored = serverHandle->priority;
#else
            BenchCheck_((RK_BOOL)(mutex.ownerPtr == NULL), "mutex released");
            RK_PRIO const ownerRestored = lowHandle->priority;
            if ((contended != 0U) && (EXP_E1_MODE != 3))
            {
                BenchCheck_((RK_BOOL)((lowWorkPrio == 3U) &&
                                      (mediumBeforeHigh == 0U)),
                            "protected high completion");
            }
            if ((contended != 0U) && (EXP_E1_MODE == 3))
            {
                BenchCheck_((RK_BOOL)(mediumBeforeHigh != 0U),
                            "inversion control exhibits interference");
            }
#endif
            if (sample != 0U)
            {
                printf("EXP RESULT experiment=E1 mode=%s path=latency contract=%s sample=%u "
                       "low_counts=%lu high_counts=%lu dispatch_counts=%lu "
                       "medium_counts=%lu medium_before_low=%u "
                       "medium_before_high=%u medium_before_low_work=%u low_enter_prio=%u "
                       "low_work_prio=%u high_work_prio=%u low_restored_prio=%u "
                       "high_restored_prio=%u owner_restored_prio=%u\r\n",
                       E1_NAME, (contended != 0U) ? "contended" : "single",
                       sample, lowElapsed, highElapsed, highDispatchDelay,
                       mediumElapsed, mediumBeforeLow, mediumBeforeHigh, mediumBeforeLowWork,
                       (UINT)lowEnterPrio, (UINT)lowWorkPrio,
                       (UINT)highWorkPrio, (UINT)lowHandle->priority,
                       (UINT)highHandle->priority, (UINT)ownerRestored);
            }
        }
    }
    costPhase = 1U;
    BenchSignal_(lowHandle, COST_GO);
    BenchOK_(kEventGet(COST_DONE, RK_EVENT_ALL, NULL, RK_MS_TO_TICKS(60000)),
             "cost completion");
    printf("EXP PASS E1\r\n");
    BenchStop_();
}
