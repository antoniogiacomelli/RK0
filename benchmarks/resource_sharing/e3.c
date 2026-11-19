/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E3: composition, independent withdrawal and priority-maintenance costs.
 *
 * The expected priorities below are an oracle specified independently of the
 * scheduler's calculation. Semantic scenarios are checked before costs are
 * measured. All results are emitted by the controller after workers finish.
 *
 * Cost batches exercise a RUNNING task with an unchanged owned set. Their raw
 * cycle totals include the loop and return-status accumulation. They are not
 * measurements of READY/wait-queue repair, transitive propagation, or WCET.
 */
#include "bench_common.h"

#define CONTROL_PRIO 1U
#define BUFFER_CEILING 3U
#define MUTEX_CEILING 5U
#define GATE_PRIO 8U
#define OWNER_PRIO 10U
#define POOL_CAPACITY 9U
#define MAX_MUTEXES 4U
#define MAX_MESSAGES 8U

/* Each task has its own event word; worker command bits can be reused. */
#define EV_START 1UL
#define EV_RELEASE 2UL
#define EV_OWNER_LOCKED 1UL
#define EV_GATE_RETURNED 2UL
#define EV_OWNER_DONE 4UL
#define EV_GATE_DONE 8UL

enum GateCommand
{
    CMD_HANDOFF,
    CMD_TIMEOUT,
    CMD_UNLOCK_FIRST,
    CMD_FREE_FIRST,
    CMD_COST
};

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, RS_STACK)
RK_DECLARE_TASK(gateHandle, GateTask, gateStack, RS_STACK)
RK_DECLARE_TASK(ownerHandle, OwnerTask, ownerStack, RS_STACK)
RK_DECLARE_MESG_POOL(messagePool, messageStorage, ULONG, POOL_CAPACITY)

static RK_MUTEX dependencyMutex;
static RK_MUTEX withdrawalMutex;
static RK_MUTEX costMutexes[MAX_MUTEXES];
static RK_MESG_ENDPOINT gateEndpoint;
static RK_MESG *volatile gateMessage;
static volatile enum GateCommand gateCommand;
static volatile unsigned ownedMutexCount;
static volatile unsigned ownedMessageCount;
static volatile uint32_t mutexWaitCycles;
static volatile uint32_t recalcBatchCycles;
static volatile uint32_t allocFreeBatchCycles;

static void ExpectPrio_(RK_TASK_HANDLE task, RK_PRIO expected,
                        char const *where)
{
    BenchCheck(RK_TASK_PRIO(task) == expected, where);
}

static void WaitForCommand_(void)
{
    /* Idle workers have no deadline: the complete suite may exceed two seconds. */
    BenchRequire(kEventGet(EV_START, RK_EVENT_ALL, NULL, RK_WAIT_FOREVER),
                 "E3 worker command");
}

static void WaitWorkersParked_(void)
{
    /* Posting completion wakes the priority-1 controller before the posting
     * worker can return to its command wait. Let both workers park before
     * reporting or preparing the next trial, outside every timed interval. */
    for (RK_TICK elapsed = 0UL; elapsed < RS_SETUP_TIMEOUT; ++elapsed)
    {
        RK_BOOL parked;
        RK_CR_AREA
        RK_CR_ENTER
        parked = (RK_BOOL)((gateHandle->status == RK_SLEEPING_EV_FLAG) &&
                           (gateHandle->flagsReq == EV_START) &&
                           (ownerHandle->status == RK_SLEEPING_EV_FLAG) &&
                           (ownerHandle->flagsReq == EV_START));
        RK_CR_EXIT
        if (parked != RK_FALSE)
        {
            return;
        }
        BenchRequire(kSleep(1UL), "E3 wait for parked workers");
    }
    BenchFail("E3 workers did not park between trials");
}

static void WaitForDependency_(void)
{
    /* Poll only during setup. No diagnostic output occurs in the wait interval. */
    for (RK_TICK elapsed = 0UL; elapsed < RS_SETUP_TIMEOUT; ++elapsed)
    {
        RK_BOOL waiting;
        RK_CR_AREA
        RK_CR_ENTER
        waiting = (RK_BOOL)((gateHandle->status == RK_BLOCKED) &&
                            (gateHandle->waitingForMutexPtr == &dependencyMutex) &&
                            (dependencyMutex.waitingQueue.size == 1UL) &&
                            (kTCBQPeek(&dependencyMutex.waitingQueue) == gateHandle));
        RK_CR_EXIT
        if (waiting != RK_FALSE)
        {
            return;
        }
        BenchRequire(kSleep(1UL), "E3 dependency setup sleep");
    }
    BenchFail("E3 gatekeeper did not block on PIP mutex");
}

static void CheckClean_(void)
{
    ExpectPrio_(gateHandle, GATE_PRIO, "E3 gatekeeper nominal after cleanup");
    ExpectPrio_(ownerHandle, OWNER_PRIO, "E3 owner nominal after cleanup");
    BenchCheck(gateEndpoint.ownedMesgList.size == 0UL,
               "E3 residual owned messages");
    BenchCheck(gateHandle->ownedMutexList.size == 0UL,
               "E3 residual gatekeeper mutexes");
    BenchCheck(ownerHandle->ownedMutexList.size == 0UL,
               "E3 residual owner mutexes");
    BenchCheck(messagePool.nFreeBlocks == POOL_CAPACITY,
               "E3 message pool not restored");
}

static void RunComposition_(enum GateCommand command, unsigned sample)
{
    gateCommand = command;
    BenchSignal(ownerHandle, EV_START);
    BenchWait(EV_OWNER_LOCKED);
    ExpectPrio_(ownerHandle, OWNER_PRIO, "E3 owner starts without donation");
    BenchSignal(gateHandle, EV_START);
    WaitForDependency_();

    BenchCheck(dependencyMutex.ownerPtr == ownerHandle,
               "E3 owner retains mutex while G waits");
    BenchCheck((gateMessage != NULL) && (gateMessage->owner == gateHandle),
               "E3 blocked G retains message custody");
    BenchCheck(ownerHandle->mesgEndpointPtr == NULL,
               "E3 owner must have no message endpoint");
    ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 blocked G ceiling is 3");
    ExpectPrio_(ownerHandle, BUFFER_CEILING, "E3 O inherits G effective 3");

    if (command == CMD_HANDOFF)
    {
        BenchSignal(ownerHandle, EV_RELEASE);
    }
    BenchWait(EV_GATE_RETURNED);

    BenchCheck((dependencyMutex.waitingQueue.size == 0UL) &&
               (gateHandle->waitingForMutexPtr == NULL),
               "E3 dependency removed after handoff or timeout");
    ExpectPrio_(ownerHandle, OWNER_PRIO, "E3 donation withdrawn from O");
    ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 G keeps its buffer ceiling");
    BenchCheck((gateMessage != NULL) && (gateMessage->owner == gateHandle),
               "E3 buffer custody survives dependency withdrawal");
    BenchCheck(dependencyMutex.ownerPtr ==
                   ((command == CMD_HANDOFF) ? gateHandle : ownerHandle),
               "E3 correct mutex owner after dependency withdrawal");

    if (command == CMD_TIMEOUT)
    {
        BenchSignal(ownerHandle, EV_RELEASE);
    }
    BenchWait(EV_OWNER_DONE);
    BenchSignal(gateHandle, EV_RELEASE);
    BenchWait(EV_GATE_DONE);
    WaitWorkersParked_();
    CheckClean_();
    BenchCheck((dependencyMutex.lock == RK_FALSE) &&
               (dependencyMutex.ownerPtr == NULL) && (gateMessage == NULL),
               "E3 composition cleanup");

    char const *scenario = (command == CMD_HANDOFF)
                               ? "composition_handoff" : "composition_timeout";
    /* Includes setup/controller coordination and any intentional timeout.
     * Do not interpret this as the isolated cost of priority propagation. */
    BenchEmit(scenario, sample, "mutex_wait_with_coordination_cycles",
              mutexWaitCycles, 1U);
    if (sample != 0U)
    {
        printf("RS CHECK experiment=E3 case=%s sample=%u "
               "blocked_G=3 blocked_O=3 withdrawn_G=3 withdrawn_O=10 "
               "freed_G=8 owner_has_endpoint=0\r\n", scenario, sample);
    }
}

static void RunWithdrawal_(enum GateCommand command, unsigned sample)
{
    gateCommand = command;
    BenchSignal(gateHandle, EV_START);
    BenchWait(EV_GATE_DONE);
    WaitWorkersParked_();
    CheckClean_();
    BenchCheck((withdrawalMutex.lock == RK_FALSE) &&
               (withdrawalMutex.ownerPtr == NULL), "E3 withdrawal cleanup");
    if (sample != 0U)
    {
        printf("RS CHECK experiment=E3 case=%s sample=%u priorities=%s\r\n",
               (command == CMD_UNLOCK_FIRST) ? "withdraw_unlock_first"
                                             : "withdraw_free_first",
               sample, (command == CMD_UNLOCK_FIRST) ? "8:5:3:3:8"
                                                     : "8:5:3:5:8");
    }
}

static void RunCosts_(void)
{
    static unsigned const mutexCounts[] = {0U, 1U, 4U};
    static unsigned const messageCounts[] = {0U, 1U, 4U, 8U};
    for (unsigned m = 0U; m < 3U; ++m)
    {
        for (unsigned n = 0U; n < 4U; ++n)
        {
            ownedMutexCount = mutexCounts[m];
            ownedMessageCount = messageCounts[n];
            char scenario[32];
            (void)snprintf(scenario, sizeof scenario, "cost_m%u_n%u",
                           ownedMutexCount, ownedMessageCount);
            for (unsigned sample = 0U; sample <= RS_SAMPLES; ++sample)
            {
                gateCommand = CMD_COST;
                BenchSignal(gateHandle, EV_START);
                BenchWait(EV_GATE_DONE);
                WaitWorkersParked_();
                CheckClean_();
                BenchEmit(scenario, sample, "stable_recalc_batch_cycles",
                          recalcBatchCycles, RS_BATCH);
                BenchEmit(scenario, sample,
                          (ownedMessageCount == 0U)
                              ? "alloc_free_priority_changing_batch_cycles"
                              : "alloc_free_priority_stable_batch_cycles",
                          allocFreeBatchCycles, RS_BATCH);
            }
        }
    }
}

static void GateComposition_(enum GateCommand command)
{
    RK_MESG *message = NULL;
    BenchRequire(kMesgAlloc(&messagePool, &message, RK_NO_WAIT),
                 "E3 G acquires ceiling buffer");
    gateMessage = message;
    ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 allocated buffer raises G");
    uint32_t const start = BenchNow();
    RK_ERR const result = kMutexLock(&dependencyMutex,
        (command == CMD_HANDOFF) ? RK_WAIT_FOREVER : RS_WAIT_TIMEOUT);
    mutexWaitCycles = BenchDelta(start, BenchNow());
    BenchCheck(result == ((command == CMD_HANDOFF)
                              ? RK_ERR_SUCCESS : RK_ERR_TIMEOUT),
               "E3 expected mutex result");
    ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 buffer ceiling after mutex return");
    BenchSignal(controlHandle, EV_GATE_RETURNED);
    BenchWait(EV_RELEASE);

    if (command == CMD_HANDOFF)
    {
        BenchRequire(kMutexUnlock(&dependencyMutex), "E3 G unlocks handed mutex");
        ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 unlock preserves buffer ceiling");
    }
    BenchRequire(kMesgFree(message), "E3 G frees buffer");
    gateMessage = NULL;
    ExpectPrio_(gateHandle, GATE_PRIO, "E3 final free restores G nominal");
}

static void GateWithdrawal_(enum GateCommand command)
{
    RK_MESG *message = NULL;
    ExpectPrio_(gateHandle, GATE_PRIO, "E3 withdrawal starts at 8");
    BenchRequire(kMutexLock(&withdrawalMutex, RK_NO_WAIT),
                 "E3 acquire ceiling-5 mutex");
    ExpectPrio_(gateHandle, MUTEX_CEILING, "E3 mutex contributes 5");
    BenchRequire(kMesgAlloc(&messagePool, &message, RK_NO_WAIT),
                 "E3 acquire ceiling-3 message");
    ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 stronger message contributes 3");
    if (command == CMD_UNLOCK_FIRST)
    {
        BenchRequire(kMutexUnlock(&withdrawalMutex), "E3 withdraw mutex first");
        ExpectPrio_(gateHandle, BUFFER_CEILING, "E3 message contribution survives");
        BenchRequire(kMesgFree(message), "E3 withdraw message last");
    }
    else
    {
        BenchRequire(kMesgFree(message), "E3 withdraw message first");
        ExpectPrio_(gateHandle, MUTEX_CEILING, "E3 mutex contribution survives");
        BenchRequire(kMutexUnlock(&withdrawalMutex), "E3 withdraw mutex last");
    }
    ExpectPrio_(gateHandle, GATE_PRIO, "E3 independent withdrawal restores 8");
}

static void GateCosts_(void)
{
    RK_MESG *held[MAX_MESSAGES] = {NULL};
    unsigned const mutexCount = ownedMutexCount;
    unsigned const messageCount = ownedMessageCount;
    BenchCheck((mutexCount <= MAX_MUTEXES) && (messageCount <= MAX_MESSAGES),
               "E3 invalid owned-set size");
    for (unsigned i = 0U; i < mutexCount; ++i)
    {
        BenchRequire(kMutexLock(&costMutexes[i], RK_NO_WAIT), "E3 cost mutex acquire");
    }
    for (unsigned i = 0U; i < messageCount; ++i)
    {
        BenchRequire(kMesgAlloc(&messagePool, &held[i], RK_NO_WAIT),
                     "E3 cost retained message acquire");
    }
    RK_PRIO const expected = (messageCount == 0U) ? GATE_PRIO : BUFFER_CEILING;
    ExpectPrio_(gateHandle, expected, "E3 stable cost initial priority");
    BenchCheck((gateHandle->ownedMutexList.size == mutexCount) &&
               (gateEndpoint.ownedMesgList.size == messageCount) &&
               (messagePool.nFreeBlocks == POOL_CAPACITY - messageCount),
               "E3 stable cost initial relationships");

    RK_BOOL changed = RK_FALSE;
    uint32_t start = BenchNow();
    for (unsigned i = 0U; i < RS_BATCH; ++i)
    {
        changed |= kTaskUpdateEffectivePrio(gateHandle);
    }
    recalcBatchCycles = BenchDelta(start, BenchNow());
    BenchCheck(changed == RK_FALSE, "E3 stable recalculation unexpectedly changed priority");
    ExpectPrio_(gateHandle, expected, "E3 stable recalculation final priority");

    /* The ninth block is the allocation probe. No waiter exists on this pool.
     * For N=0 each pair changes 8 -> 3 -> 8. For N>0 the task stays at 3.
     * Collect statuses without per-call assertions or output in the batch. */
    RK_MESG *probe = NULL;
    unsigned errors = 0U;
    start = BenchNow();
    for (unsigned i = 0U; i < RS_BATCH; ++i)
    {
        errors |= (unsigned)kMesgAlloc(&messagePool, &probe, RK_NO_WAIT);
        errors |= (unsigned)kMesgFree(probe);
    }
    allocFreeBatchCycles = BenchDelta(start, BenchNow());
    BenchCheck(errors == (unsigned)RK_ERR_SUCCESS, "E3 alloc/free batch failed");
    ExpectPrio_(gateHandle, expected, "E3 alloc/free final priority");
    BenchCheck((gateHandle->ownedMutexList.size == mutexCount) &&
               (gateEndpoint.ownedMesgList.size == messageCount) &&
               (messagePool.nFreeBlocks == POOL_CAPACITY - messageCount),
               "E3 alloc/free changed retained relationships");

    for (unsigned i = 0U; i < messageCount; ++i)
    {
        BenchCheck(held[i]->owner == gateHandle, "E3 retained message lost its owner");
        BenchRequire(kMesgFree(held[i]), "E3 cost retained message free");
    }
    for (unsigned i = mutexCount; i > 0U; --i)
    {
        BenchRequire(kMutexUnlock(&costMutexes[i - 1U]), "E3 cost mutex release");
    }
    ExpectPrio_(gateHandle, GATE_PRIO, "E3 cost cleanup nominal priority");
}

int main(void)
{
    kCoreInit();
    BenchInit("E3");
    kInit();
    BenchFail("E3 scheduler returned");
    return 1;
}

VOID kApplicationInit(VOID)
{
    BenchRequire(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "E3ctrl",
                           controlStack, RS_STACK, CONTROL_PRIO, RK_PREEMPT),
                 "E3 controller init");
    BenchRequire(kTaskInit(&gateHandle, GateTask, RK_NO_ARGS, "E3gate",
                           gateStack, RS_STACK, GATE_PRIO, RK_PREEMPT),
                 "E3 gatekeeper init");
    BenchRequire(kTaskInit(&ownerHandle, OwnerTask, RK_NO_ARGS, "E3owner",
                           ownerStack, RS_STACK, OWNER_PRIO, RK_PREEMPT),
                 "E3 owner init");
    BenchRequire(kMesgEndpointInit(&gateEndpoint, gateHandle, RK_MESG_SEND_ONLY),
                 "E3 gatekeeper endpoint");
    BenchRequire(kMesgPoolInit(&messagePool, messageStorage, sizeof(ULONG),
                              POOL_CAPACITY, BUFFER_CEILING), "E3 message pool");
    BenchRequire(kMutexInit(&dependencyMutex, RK_PRIO_INHERITANCE, RK_NO_CEILING),
                 "E3 dependency mutex");
    BenchRequire(kMutexInit(&withdrawalMutex, RK_PRIO_CEILING, MUTEX_CEILING),
                 "E3 withdrawal mutex");
    for (unsigned i = 0U; i < MAX_MUTEXES; ++i)
    {
        BenchRequire(kMutexInit(&costMutexes[i], RK_PRIO_INHERITANCE, RK_NO_CEILING),
                     "E3 cost mutex init");
    }
    BenchKeepAwakeInit();
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    WaitWorkersParked_();
    printf("RS NOTE experiment=E3 owned_mutex_protocol=PIP owned_mutex_waiters=0 "
           "pool_capacity=9 buffer_ceiling=3 cost_task_state=RUNNING "
           "batch_status_accumulation=1 timeout_ticks=%lu\r\n",
           (unsigned long)RS_WAIT_TIMEOUT);
    for (unsigned sample = 0U; sample <= RS_SAMPLES; ++sample)
    {
        RunComposition_(CMD_HANDOFF, sample);
        RunComposition_(CMD_TIMEOUT, sample);
        RunWithdrawal_(CMD_UNLOCK_FIRST, sample);
        RunWithdrawal_(CMD_FREE_FIRST, sample);
    }
    RunCosts_();
    BenchFinish();
}

VOID GateTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        WaitForCommand_();
        enum GateCommand const command = gateCommand;
        switch (command)
        {
            case CMD_HANDOFF:
            case CMD_TIMEOUT:
                GateComposition_(command);
                break;
            case CMD_UNLOCK_FIRST:
            case CMD_FREE_FIRST:
                GateWithdrawal_(command);
                break;
            case CMD_COST:
                GateCosts_();
                break;
            default:
                BenchFail("E3 invalid gatekeeper command");
                break;
        }
        BenchSignal(controlHandle, EV_GATE_DONE);
    }
}

VOID OwnerTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        WaitForCommand_();
        BenchRequire(kMutexLock(&dependencyMutex, RK_NO_WAIT), "E3 owner initial lock");
        BenchSignal(controlHandle, EV_OWNER_LOCKED);
        BenchWait(EV_RELEASE);
        BenchRequire(kMutexUnlock(&dependencyMutex), "E3 owner release");
        ExpectPrio_(ownerHandle, OWNER_PRIO, "E3 owner restored after release");
        BenchSignal(controlHandle, EV_OWNER_DONE);
    }
}
