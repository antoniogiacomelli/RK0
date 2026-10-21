/* SPDX-License-Identifier: Apache-2.0 */
/* Native message-ceiling -> PIP dependency, independent restoration, and
 * scheduler recomputation/public allocation costs as owned sets grow.
 */
#include "bench.h"
#include <ksch.h>

#ifndef EXP_ITERATIONS
#define EXP_ITERATIONS 256U
#endif
#define OWNER_LOCKED RK_EVENT_1
#define GATE_RETURNED RK_EVENT_2
#define OWNER_DONE RK_EVENT_3
#define GATE_DONE RK_EVENT_4
#define OWNER_RELEASE RK_EVENT_2
#define GATE_RELEASE RK_EVENT_2
#define COST_GO RK_EVENT_3
#define COST_DONE RK_EVENT_5

typedef struct { ULONG value; } Payload;
RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, 320U)
RK_DECLARE_TASK(gateHandle, GateTask, gateStack, 320U)
RK_DECLARE_TASK(ownerHandle, OwnerTask, ownerStack, EXP_STACK)
RK_DECLARE_MESG_POOL(pool, poolBuffer, Payload, 9U)
static RK_MESG_ENDPOINT endpoint;
static RK_MUTEX dependency;
static RK_MUTEX owned[4];
static volatile UINT phase;
static volatile ULONG dependencyElapsed;

static VOID Priority_(RK_TASK_HANDLE const task, RK_PRIO const expected)
{
    BenchCheck_((RK_BOOL)(task->priority == expected), "effective priority");
}

static VOID SameTask_(VOID)
{
    for (UINT order = 0U; order < 2U; order++)
    {
        RK_MESG *message = NULL;
        BenchOK_(kMutexLock(&owned[0], RK_NO_WAIT), "same task lock");
        RK_PRIO const mutexOnly = gateHandle->priority;
        Priority_(gateHandle, 5U);
        BenchOK_(kMesgAlloc(&pool, &message, RK_NO_WAIT), "same task alloc");
        RK_PRIO const both = gateHandle->priority;
        Priority_(gateHandle, 3U);
        if (order == 0U) { BenchOK_(kMutexUnlock(&owned[0]), "unlock first"); }
        else { BenchOK_(kMesgFree(message), "free first"); }
        RK_PRIO const remaining = gateHandle->priority;
        Priority_(gateHandle, (order == 0U) ? 3U : 5U);
        if (order == 0U) { BenchOK_(kMesgFree(message), "free last"); }
        else { BenchOK_(kMutexUnlock(&owned[0]), "unlock last"); }
        Priority_(gateHandle, 8U);
        printf("EXP RESULT experiment=E3 path=independent order=%s "
               "mutex_prio=%u both_prio=%u remaining_prio=%u restored_prio=%u\r\n",
               (order == 0U) ? "unlock_first" : "free_first",
               (UINT)mutexOnly, (UINT)both, (UINT)remaining,
               (UINT)gateHandle->priority);
    }
}

static VOID Costs_(VOID)
{
    static UINT const mutexCounts[] = {0U, 1U, 4U};
    static UINT const messageCounts[] = {0U, 1U, 4U, 8U};
    RK_MESG *messages[8];
    for (UINT mi = 0U; mi < 3U; mi++)
    {
        UINT const m = mutexCounts[mi];
        for (UINT ni = 0U; ni < 4U; ni++)
        {
            UINT const n = messageCounts[ni];
            for (UINT j = 0U; j < m; j++)
            { BenchOK_(kMutexLock(&owned[j], RK_NO_WAIT), "cost lock"); }
            for (UINT j = 0U; j < n; j++)
            { BenchOK_(kMesgAlloc(&pool, &messages[j], RK_NO_WAIT), "cost alloc"); }
            RK_PRIO const expected = (n != 0U) ? 3U : ((m != 0U) ? 5U : 8U);
            Priority_(gateHandle, expected);
            for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
            {
                UINT changes = 0U;
                ULONG const start = BenchNow_();
                for (UINT j = 0U; j < EXP_ITERATIONS; j++)
                { changes += (UINT)kTaskUpdateEffectivePrio(gateHandle); }
                ULONG const recalc = BenchNow_() - start;
                BenchCheck_((RK_BOOL)(changes == 0U), "stable recomputation");
                ULONG const allocStart = BenchNow_();
                for (UINT j = 0U; j < EXP_ITERATIONS; j++)
                {
                    RK_MESG *probe = NULL;
                    BenchOK_(kMesgAlloc(&pool, &probe, RK_NO_WAIT), "probe alloc");
                    BenchOK_(kMesgFree(probe), "probe free");
                }
                ULONG const roundtrip = BenchNow_() - allocStart;
                Priority_(gateHandle, expected);
                if (sample != 0U)
                {
                    printf("EXP RESULT experiment=E3 path=cost sample=%u "
                           "mutexes=%u messages=%u iterations=%u "
                           "recalc_counts=%lu alloc_free_counts=%lu priority=%u\r\n",
                           sample, m, n, (UINT)EXP_ITERATIONS, recalc, roundtrip,
                           (UINT)gateHandle->priority);
                }
            }
            for (UINT j = 0U; j < n; j++)
            { BenchOK_(kMesgFree(messages[j]), "cost free"); }
            for (UINT j = m; j > 0U; j--)
            { BenchOK_(kMutexUnlock(&owned[j - 1U]), "cost unlock"); }
            Priority_(gateHandle, 8U);
        }
    }
}

VOID kApplicationInit(VOID)
{
    BenchOK_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "E3ctrl",
                      controlStack, 320U, 1U, RK_PREEMPT), "control init");
    BenchOK_(kTaskInit(&gateHandle, GateTask, RK_NO_ARGS, "E3gate",
                      gateStack, 320U, 8U, RK_PREEMPT), "gate init");
    BenchOK_(kTaskInit(&ownerHandle, OwnerTask, RK_NO_ARGS, "E3owner",
                      ownerStack, EXP_STACK, 10U, RK_PREEMPT), "owner init");
    BenchOK_(kMesgEndpointInit(&endpoint, gateHandle, RK_MESG_SEND_ONLY),
             "endpoint init");
    BenchOK_(kMesgPoolInit(&pool, poolBuffer, sizeof(Payload), 9U, 3U), "pool init");
    BenchOK_(kMutexInit(&dependency, RK_PRIO_INHERITANCE, RK_NO_CEILING),
             "dependency init");
    for (UINT j = 0U; j < 4U; j++)
    { BenchOK_(kMutexInit(&owned[j], RK_PRIO_CEILING, 5U), "owned init"); }
}

VOID OwnerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        BenchGo_();
        BenchOK_(kMutexLock(&dependency, RK_NO_WAIT), "owner lock");
        BenchSignal_(controlHandle, OWNER_LOCKED);
        BenchWait_(OWNER_RELEASE); /* Controller choreography, outside cost samples. */
        BenchOK_(kMutexUnlock(&dependency), "owner unlock");
        BenchSignal_(controlHandle, OWNER_DONE);
    }
}

VOID GateTask(VOID *args)
{
    RK_UNUSEARGS
    for (UINT path = 0U; path < 2U; path++)
    {
        for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
        {
            BenchGo_();
            RK_MESG *message = NULL;
            BenchOK_(kMesgAlloc(&pool, &message, RK_NO_WAIT), "gate alloc");
            Priority_(gateHandle, 3U);
            ULONG const start = BenchNow_();
            RK_ERR const err = kMutexLock(&dependency,
                                          (phase == 0U) ? EXP_LIMIT : 50UL);
            dependencyElapsed = BenchNow_() - start;
            BenchCheck_((RK_BOOL)(err == ((phase == 0U) ? RK_ERR_SUCCESS
                                                                       : RK_ERR_TIMEOUT)),
                        "dependency result");
            BenchSignal_(controlHandle, GATE_RETURNED);
            BenchWait_(GATE_RELEASE);
            if (phase == 0U)
            { BenchOK_(kMutexUnlock(&dependency), "gate unlock"); }
            Priority_(gateHandle, 3U);
            BenchOK_(kMesgFree(message), "gate free");
            Priority_(gateHandle, 8U);
            BenchSignal_(controlHandle, GATE_DONE);
        }
    }
    BenchWait_(COST_GO);
    SameTask_();
    Costs_();
    BenchSignal_(controlHandle, COST_DONE);
    BenchStop_();
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    BenchHeader_("E3");
    for (UINT path = 0U; path < 2U; path++)
    {
        phase = path;
        for (UINT sample = 0U; sample <= EXP_SAMPLES; sample++)
        {
            BenchSignal_(ownerHandle, EXP_GO);
            BenchWait_(OWNER_LOCKED);
            BenchSignal_(gateHandle, EXP_GO);
            RK_TICK const deadline = K_TICK_ADD(kTickGet(), EXP_LIMIT);
            while (gateHandle->waitingForMutexPtr != &dependency)
            {
                BenchCheck_((RK_BOOL)(K_TICK_EXPIRED(deadline) == 0U),
                            "gate dependency established");
                kSleep(1UL);
            }
            BenchCheck_((RK_BOOL)((gateHandle->status == RK_BLOCKED) &&
                                  (dependency.ownerPtr == ownerHandle) &&
                                  (dependency.waitingQueue.size == 1UL) &&
                                  (ownerHandle->mesgEndpointPtr == NULL)),
                        "cross-task PIP dependency");
            Priority_(gateHandle, 3U);
            Priority_(ownerHandle, 3U);
            RK_PRIO const propagated = ownerHandle->priority;
            if (path == 0U) { BenchSignal_(ownerHandle, OWNER_RELEASE); }
            BenchWait_(GATE_RETURNED);
            Priority_(ownerHandle, 10U);
            Priority_(gateHandle, 3U);
            BenchCheck_((RK_BOOL)((dependency.waitingQueue.size == 0UL) &&
                                  (gateHandle->waitingForMutexPtr == NULL) &&
                                  (dependency.ownerPtr == ((path == 0U)
                                      ? gateHandle : ownerHandle))),
                        "dependency removal and remaining ownership");
            if (path != 0U) { BenchSignal_(ownerHandle, OWNER_RELEASE); }
            BenchWait_(OWNER_DONE);
            BenchSignal_(gateHandle, GATE_RELEASE);
            BenchWait_(GATE_DONE);
            BenchCheck_((RK_BOOL)((pool.nFreeBlocks == 9UL) &&
                                  (dependency.ownerPtr == NULL)), "phase cleanup");
            if (sample != 0U)
            {
                printf("EXP RESULT experiment=E3 path=%s sample=%u wait_counts=%lu "
                       "propagated_prio=%u owner_restored=%u gate_restored=%u\r\n",
                       (path == 0U) ? "handoff" : "timeout", sample,
                       dependencyElapsed, (UINT)propagated,
                       (UINT)ownerHandle->priority, (UINT)gateHandle->priority);
            }
        }
    }
    BenchSignal_(gateHandle, COST_GO);
    BenchOK_(kEventGet(COST_DONE, RK_EVENT_ALL, NULL, RK_MS_TO_TICKS(60000)),
             "cost completion");
    BenchCheck_((RK_BOOL)(pool.nFreeBlocks == 9UL), "cost pool cleanup");
    printf("EXP PASS E3\r\n");
    BenchStop_();
}
