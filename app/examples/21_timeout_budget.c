/* SPDX-License-Identifier: Apache-2.0 */
/* Finite timeout budgets across wakes, retries, delayed dispatch, and tick wrap. */
#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 256U
#define WAIT_TICKS 20UL
#define SYNC_WAIT 500UL
#define ROLLOVER_LEAD 8UL
#define REQUEST RK_EVENT_1
#define DONE RK_EVENT_2

enum
{
    EXCHANGE_POST,
    QUEUE_SEND,
    QUEUE_JAM,
    QUEUE_RECV,
    BROADCAST_SEND,
    BROADCAST_RECV,
    MESG_ALLOC,
    MESG_WAIT,
    N_OPERATIONS
};

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(workerHandle, WorkerTask, workerStack, STACKSIZE)
RK_DECLARE_EXCHG(exchange)
RK_DECLARE_MESG_QUEUE(queue, queueStorage, ULONG, 1U)
RK_DECLARE_MESG_POOL(pool, poolStorage, ULONG, 1U)

static RK_MESG_ENDPOINT controlEndpoint;
static RK_MESG_ENDPOINT workerEndpoint;
static RK_MESG *heldMessage;
static ULONG initialValue = 1UL;
static ULONG sentValue = 2UL;
static volatile UINT operation;
static volatile RK_TICK requested;
static volatile RK_TICK started;
static volatile RK_TICK finished;
static volatile RK_TICK elapsed;
static volatile RK_ERR workerResult;
static UINT budgetFailures;
static UINT rolloverCases;
static CHAR const *scenario = "ordinary ticks";
static CHAR const *const operationNames[N_OPERATIONS] =
{
    "exchange post", "queue send", "queue jam", "queue receive",
    "broadcast send", "broadcast receive", "message allocate", "message wait"
};

/* Exercise the real counter update as well as deadline arithmetic. */
extern UINT kTickHandler(VOID);

static VOID Stop_(VOID)
{
    while (1)
        (void)kSleep(1000UL);
}

static VOID Check_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("TB FAIL %s (%s): %s\r\n",
               operationNames[operation], scenario, where);
        Stop_();
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected)
{
    Check_((RK_BOOL)(actual == expected), "result");
}

static VOID Budget_(RK_TICK const minimum, RK_TICK const maximum)
{
    if ((elapsed < minimum) || (elapsed > maximum))
    {
        printf("TB FAIL %s (%s) requested=%lu elapsed=%lu expected=%lu..%lu\r\n",
               operationNames[operation], scenario, (ULONG)requested, (ULONG)elapsed,
               (ULONG)minimum, (ULONG)maximum);
        budgetFailures++;
    }
}

static VOID CheckWaiting_(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    Check_((RK_BOOL)((workerHandle->waitingQueuePtr != NULL) &&
                     (workerHandle->waitingQueuePtr->size == 1UL)),
            "worker waiting");
    Check_((RK_BOOL)((workerHandle->timeoutNode.listRefPtr != NULL) ==
                     (requested != RK_WAIT_FOREVER)), "timer armed");
    RK_CR_EXIT
}

static VOID TestClockRollover_(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TICK const savedTick = RK_gRunTime.globalTick;
    UINT const savedWraps = RK_gRunTime.nWraps;
    Check_((RK_BOOL)((RK_gTimeOutListHeadPtr == NULL) &&
                     (RK_gTimerListHeadPtr == NULL)), "rollover has no active timers");
    RK_gRunTime.globalTick = RK_TICK_TYPE_MAX - 1UL;
    (void)kTickHandler();
    Check_((RK_BOOL)((kTickGet() == RK_TICK_TYPE_MAX) &&
                     (RK_gRunTime.nWraps == savedWraps)), "clock visits maximum tick");
    (void)kTickHandler();
    Check_((RK_BOOL)((kTickGet() == 0UL) &&
                     (RK_gRunTime.nWraps == savedWraps + 1UL)), "clock wraps at zero");
    RK_gRunTime.globalTick = savedTick;
    RK_gRunTime.nWraps = savedWraps;
    RK_CR_EXIT
}

static VOID Prepare_(UINT const op)
{
    operation = op;
    VOID *mail = NULL;
    (void)kExchangeAccept(&exchange, &mail);
    Expect_(kMesgQueueReset(&queue), RK_ERR_SUCCESS);
    if (op == EXCHANGE_POST)
        Expect_(kExchangePost(&exchange, &initialValue, RK_NO_WAIT), RK_ERR_SUCCESS);
    if ((op == QUEUE_SEND) || (op == QUEUE_JAM))
        Expect_(kMesgQueueSend(&queue, &initialValue, RK_NO_WAIT), RK_ERR_SUCCESS);
    if (op == MESG_ALLOC)
        Expect_(kMesgAlloc(&pool, &heldMessage, RK_NO_WAIT), RK_ERR_SUCCESS);
}

static VOID Start_(RK_TICK const timeout)
{
    requested = timeout;
    workerResult = RK_ERR_ERROR;
    elapsed = 0UL;
    Expect_(kEventSet(workerHandle, REQUEST), RK_ERR_SUCCESS);
}

static VOID CheckEmpty_(RK_LIST const *const list)
{
    Check_((RK_BOOL)((list->size == 0UL) &&
                     (list->listDummy.nextPtr == &list->listDummy) &&
                     (list->listDummy.prevPtr == &list->listDummy)),
            "object wait queue cleanup");
}

static VOID Complete_(RK_ERR const expected)
{
    Expect_(kEventGet(DONE, RK_EVENT_ALL, NULL, SYNC_WAIT), RK_ERR_SUCCESS);
    Expect_(workerResult, expected);
    Check_((RK_BOOL)((workerHandle->waitingQueuePtr == NULL) &&
                     (workerHandle->timeoutNode.listRefPtr == NULL) &&
                     (workerHandle->timeoutNode.nextPtr == NULL) &&
                     (workerHandle->timeoutNode.prevPtr == NULL) &&
                     (workerHandle->timeoutNode.dtick == 0UL) &&
                     (workerHandle->timeoutNode.timeoutType == 0U) &&
                     (workerHandle->timeoutNode.waitInfo == 0U) &&
                     (workerHandle->timeOut == RK_FALSE)), "wait cleanup");
    Check_((RK_BOOL)((workerHandle->priority == 5U) &&
                     (workerHandle->mboxCeilingPtr == NULL) &&
                     (workerHandle->mboxBcastMinRecv == 0U)), "ceiling cleanup");
    Check_((RK_BOOL)((workerEndpoint.waitDestPtr == NULL) &&
                     (workerEndpoint.allocDestPtr == NULL) &&
                     (workerHandle->mesgQueueRecvBufPtr == NULL)),
            "destination cleanup");
    CheckEmpty_(&exchange.waitingSenders);
    CheckEmpty_(&exchange.waitingReceivers);
    CheckEmpty_(&queue.waitingSenders);
    CheckEmpty_(&queue.waitingReceivers);
    CheckEmpty_(&pool.waitingQueue);
    CheckEmpty_(&workerEndpoint.waitingReceivers);
    CheckEmpty_(&workerEndpoint.mesgQueue);
    CheckEmpty_(&workerEndpoint.ownedMesgList);
    Check_((RK_BOOL)(queue.broadcastReceivers == 0UL), "broadcast cleanup");
}

static VOID Cleanup_(VOID)
{
    if (heldMessage != NULL)
    {
        RK_MESG *const message = heldMessage;
        heldMessage = NULL;
        Expect_(kMesgFree(message), RK_ERR_SUCCESS);
    }
}

/* A higher-priority task wakes the worker, then consumes the opportunity. */
static VOID WakeAndInvalidate_(VOID)
{
    VOID *mail = NULL;
    ULONG value = 0UL;
    if (operation == EXCHANGE_POST)
    {
        Expect_(kExchangeAccept(&exchange, &mail), RK_ERR_SUCCESS);
        Expect_(kExchangePost(&exchange, &initialValue, RK_NO_WAIT), RK_ERR_SUCCESS);
    }
    else if ((operation == QUEUE_SEND) || (operation == QUEUE_JAM))
    {
        Expect_(kMesgQueueRecv(&queue, &value, RK_NO_WAIT), RK_ERR_SUCCESS);
        Expect_(kMesgQueueSend(&queue, &initialValue, RK_NO_WAIT), RK_ERR_SUCCESS);
    }
    else
    {
        /* Reset wakes readers and threshold waiters without satisfying them. */
        Expect_(kMesgQueueReset(&queue), RK_ERR_SUCCESS);
    }
    Check_((RK_BOOL)((workerHandle->status == RK_READY) &&
                     (workerHandle->timeoutNode.listRefPtr == NULL) &&
                     (workerHandle->timeOut == RK_FALSE)), "normal wake cancels timer");
}

static VOID Grant_(VOID)
{
    VOID *mail = NULL;
    ULONG value = 0UL;
    if (operation == EXCHANGE_POST)
        Expect_(kExchangeAccept(&exchange, &mail), RK_ERR_SUCCESS);
    else if ((operation == QUEUE_SEND) || (operation == QUEUE_JAM))
        Expect_(kMesgQueueRecv(&queue, &value, RK_NO_WAIT), RK_ERR_SUCCESS);
    else if (operation == QUEUE_RECV)
        Expect_(kMesgQueueSend(&queue, &sentValue, RK_NO_WAIT), RK_ERR_SUCCESS);
    else if (operation == BROADCAST_SEND)
        Expect_(kMesgQueueBroadcastRecv(&queue, &value, SYNC_WAIT), RK_ERR_SUCCESS);
    else if (operation == BROADCAST_RECV)
        Expect_(kMesgQueueBroadcastWaitN(&queue, &sentValue, 1U,
                                         RK_NO_WAIT, NULL), RK_ERR_SUCCESS);
    else if (operation == MESG_ALLOC)
        Cleanup_();
    else
    {
        RK_MESG *message = NULL;
        Expect_(kMesgAlloc(&pool, &message, RK_NO_WAIT), RK_ERR_SUCCESS);
        Expect_(kMesgSend(workerHandle, message), RK_ERR_SUCCESS);
    }
}

static VOID TestDeadlineArithmetic_(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TICK const savedTick = RK_gRunTime.globalTick;
    RK_TIMEOUT_NODE node = {0};
    RK_gRunTime.globalTick = RK_TICK_TYPE_MAX - 4UL;
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), 10UL);
    node.timeoutType = RK_TIMEOUT_BLOCKING;
    Expect_(kTimeoutNodeAddUntil(&node, 10UL, deadline), RK_ERR_SUCCESS);
    Check_((RK_BOOL)(node.timeout == 10UL), "deadline across rollover");
    Expect_(kTimeoutNodeDisarm(&node), RK_ERR_SUCCESS);
    RK_gRunTime.globalTick = 2UL;
    node.timeoutType = RK_TIMEOUT_BLOCKING;
    Expect_(kTimeoutNodeAddUntil(&node, 10UL, deadline), RK_ERR_SUCCESS);
    Check_((RK_BOOL)(node.timeout == 3UL), "remaining across rollover");
    Expect_(kTimeoutNodeDisarm(&node), RK_ERR_SUCCESS);
    RK_gRunTime.globalTick = deadline;
    Expect_(kTimeoutNodeAddUntil(&node, 10UL, deadline), RK_ERR_TIMEOUT);
    RK_gRunTime.globalTick++;
    Expect_(kTimeoutNodeAddUntil(&node, 10UL, deadline), RK_ERR_TIMEOUT);
    Check_((RK_BOOL)(node.listRefPtr == NULL), "expired deadline stays disarmed");
    /* Invalid original budgets must not become RK_ERR_TIMEOUT after subtraction. */
    Expect_(kTimeoutNodeAddUntil(&node, RK_MAX_PERIOD + 1UL, deadline),
             RK_ERR_INVALID_TIMEOUT);
    Expect_(kTimeoutNodeAddUntil(&node, 0UL, deadline), RK_ERR_INVALID_TIMEOUT);
    node.timeoutType = RK_TIMEOUT_BLOCKING;
    Expect_(kTimeoutNodeAddUntil(&node, RK_MAX_PERIOD,
                                 K_TICK_ADD(kTickGet(), RK_MAX_PERIOD)), RK_ERR_SUCCESS);
    Check_((RK_BOOL)(node.timeout == RK_MAX_PERIOD), "maximum finite budget");
    Expect_(kTimeoutNodeDisarm(&node), RK_ERR_SUCCESS);
    RK_gRunTime.globalTick = savedTick;
    RK_CR_EXIT
}

typedef struct
{
    RK_TICK tick;
    UINT wraps;
} TB_CLOCK;

/* Reconstruct the armed deadline from the delta list, not from test arithmetic. */
static RK_TICK WaitingDeadline_(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    volatile RK_TIMEOUT_NODE const *node = RK_gTimeOutListHeadPtr;
    RK_TICK remaining = 0UL;
    while ((node != NULL) && (node != &workerHandle->timeoutNode))
    {
        remaining += node->dtick;
        node = node->nextPtr;
    }
    Check_((RK_BOOL)(node == &workerHandle->timeoutNode), "worker timer is linked");
    remaining += node->dtick;
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), remaining);
    RK_CR_EXIT
    return (deadline);
}

/* Seed only an idle clock. Subsequent wrap and expiry use real SysTick IRQs. */
static RK_TICK BeginTaskRollover_(UINT const op, CHAR const *const name,
                                 TB_CLOCK *const clock)
{
    scenario = name;
    Prepare_(op);
    RK_CR_AREA
    RK_CR_ENTER
    Check_((RK_BOOL)((RK_gTimeOutListHeadPtr == NULL) &&
                     (RK_gTimerListHeadPtr == NULL)), "idle clock before seed");
    clock->tick = RK_gRunTime.globalTick;
    clock->wraps = RK_gRunTime.nWraps;
    RK_gRunTime.globalTick = RK_TICK_TYPE_MAX - ROLLOVER_LEAD;
    RK_CR_EXIT
    Start_(WAIT_TICKS);
    Expect_(kSleep(1UL), RK_ERR_SUCCESS);
    CheckWaiting_();
    RK_TICK const deadline = WaitingDeadline_();
    Check_((RK_BOOL)((RK_gRunTime.nWraps == clock->wraps) &&
                     (started > RK_TICK_TYPE_MAX - WAIT_TICKS) &&
                     (deadline < started)), "blocked before wrap with a wrapped deadline");
    return (deadline);
}

static VOID FinishTaskRollover_(TB_CLOCK const *const clock)
{
    Cleanup_();
    RK_CR_AREA
    RK_CR_ENTER
    Check_((RK_BOOL)((finished < started) &&
                     (RK_gRunTime.nWraps == clock->wraps + 1U)),
            "task returned after one real clock wrap");
    Check_((RK_BOOL)((RK_gTimeOutListHeadPtr == NULL) &&
                     (RK_gTimerListHeadPtr == NULL)), "no stale timers after wrap");
    RK_gRunTime.globalTick = clock->tick;
    RK_gRunTime.nWraps = clock->wraps;
    RK_CR_EXIT
    rolloverCases++;
}

static VOID CheckReadyPending_(RK_BOOL const timedOut)
{
    Check_((RK_BOOL)((workerResult == RK_ERR_ERROR) &&
                     (workerHandle->status == RK_READY) &&
                     (workerHandle->waitingQueuePtr == NULL) &&
                     (workerHandle->timeoutNode.listRefPtr == NULL) &&
                     (workerHandle->timeoutNode.timeoutType == 0U) &&
                     (workerHandle->timeOut == timedOut)),
            "ready worker has not been dispatched");
}

/* Keep interrupts enabled and the controller running past the original deadline. */
static RK_TICK DelayDispatch_(RK_TICK const deadline)
{
    RK_TICK const delay = K_TICK_DELTA(K_TICK_ADD(deadline, 3UL), kTickGet());
    Check_((RK_BOOL)((delay > 0UL) && (delay <= WAIT_TICKS + 3UL)),
            "bounded dispatch delay");
    Expect_(kDelay(delay), RK_ERR_SUCCESS);
    Check_((RK_BOOL)K_TICK_IS_AFTER(kTickGet(), deadline), "dispatch after deadline");
    return (K_TICK_DELTA(kTickGet(), started));
}

static VOID TestTaskRollover_(UINT const op)
{
    TB_CLOCK clock;
    RK_TICK deadline = BeginTaskRollover_(op, "blocked expiry across wrap", &clock);
    Expect_(kSleep(ROLLOVER_LEAD + 1UL), RK_ERR_SUCCESS);
    CheckWaiting_();
    Check_((RK_BOOL)(RK_gRunTime.nWraps == clock.wraps + 1U), "still blocked after wrap");
    Check_((RK_BOOL)(WaitingDeadline_() == deadline), "armed deadline survives wrap");
    Complete_(RK_ERR_TIMEOUT);
    Budget_(WAIT_TICKS, WAIT_TICKS + 2UL);
    FinishTaskRollover_(&clock);

    deadline = BeginTaskRollover_(op, "grant after wrap before expiry", &clock);
    Expect_(kSleep(ROLLOVER_LEAD + 1UL), RK_ERR_SUCCESS);
    CheckWaiting_();
    Check_((RK_BOOL)((RK_gRunTime.nWraps == clock.wraps + 1U) &&
                     K_TICK_IS_BEFORE(kTickGet(), deadline)), "grant window after wrap");
    Grant_();
    Complete_(RK_ERR_SUCCESS);
    Budget_(ROLLOVER_LEAD + 1UL, WAIT_TICKS - 1UL);
    FinishTaskRollover_(&clock);

    if (op <= BROADCAST_RECV)
    {
        /* The same retries must support both expiry and a later valid grant. */
        for (UINT grant = 0U; grant < 2U; grant++)
        {
            deadline = BeginTaskRollover_(op,
                (grant == 0U) ? "retry timeout across wrap" : "retry success across wrap",
                &clock);
            UINT before = 0U;
            UINT after = 0U;
            for (UINT retry = 0U; retry < 3U; retry++)
            {
                Expect_(kSleep(3UL), RK_ERR_SUCCESS);
                kSchLock();
                if (RK_gRunTime.nWraps == clock.wraps)
                    before++;
                else
                    after++;
                WakeAndInvalidate_();
                kSchUnlock();
                Expect_(kSleep(1UL), RK_ERR_SUCCESS);
                CheckWaiting_();
                Check_((RK_BOOL)(WaitingDeadline_() == deadline), "retry retains original deadline");
            }
            Check_((RK_BOOL)((before > 0U) && (after > 0U)), "retries straddle clock wrap");
            if (grant != 0U)
            {
                Check_((RK_BOOL)K_TICK_IS_BEFORE(kTickGet(), deadline),
                        "retry grant before original deadline");
                Grant_();
                Complete_(RK_ERR_SUCCESS);
                Budget_(ROLLOVER_LEAD + 1UL, WAIT_TICKS - 1UL);
            }
            else
            {
                Complete_(RK_ERR_TIMEOUT);
                Budget_(WAIT_TICKS, WAIT_TICKS + 2UL);
            }
            FinishTaskRollover_(&clock);
        }

        deadline = BeginTaskRollover_(op, "cancelled timer and late retry across wrap", &clock);
        kSchLock();
        WakeAndInvalidate_();
        RK_TICK const dispatchDelay = DelayDispatch_(deadline);
        CheckReadyPending_(RK_FALSE);
        kSchUnlock();
        Complete_(RK_ERR_TIMEOUT);
        Budget_(dispatchDelay, dispatchDelay + 2UL);
        FinishTaskRollover_(&clock);
    }

    deadline = BeginTaskRollover_(op, "armed expiry and late dispatch across wrap", &clock);
    kSchLock();
    RK_TICK dispatchDelay = DelayDispatch_(deadline);
    CheckReadyPending_(RK_TRUE);
    kSchUnlock();
    Complete_(RK_ERR_TIMEOUT);
    Budget_(dispatchDelay, dispatchDelay + 2UL);
    FinishTaskRollover_(&clock);

    /* These operations commit a delivery/ownership grant before waking the task.
     * Free capacity/threshold wakes for senders are opportunities to retry instead. */
    if ((op == QUEUE_RECV) || (op == BROADCAST_RECV) || (op >= MESG_ALLOC))
    {
        /* Commit on each side of wrap, then dispatch after the same deadline. */
        for (UINT afterWrap = 0U; afterWrap < 2U; afterWrap++)
        {
            deadline = BeginTaskRollover_(op,
                (afterWrap == 0U) ? "pre-wrap grant and late dispatch"
                                  : "post-wrap grant and late dispatch",
                &clock);
            if (afterWrap != 0U)
                Expect_(kSleep(ROLLOVER_LEAD + 1UL), RK_ERR_SUCCESS);
            CheckWaiting_();
            Check_((RK_BOOL)(WaitingDeadline_() == deadline), "grant retains original deadline");
            kSchLock();
            Grant_();
            Check_((RK_BOOL)((RK_gRunTime.nWraps == clock.wraps + afterWrap) &&
                             K_TICK_IS_BEFORE(kTickGet(), deadline)),
                    "grant committed on intended side of wrap before expiry");
            CheckReadyPending_(RK_FALSE);
            dispatchDelay = DelayDispatch_(deadline);
            CheckReadyPending_(RK_FALSE);
            kSchUnlock();
            Complete_(RK_ERR_SUCCESS);
            Budget_(dispatchDelay, dispatchDelay + 2UL);
            FinishTaskRollover_(&clock);
        }
    }
    printf("TB task rollover %s: PASS\r\n", operationNames[op]);
    scenario = "ordinary ticks";
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}

VOID kApplicationInit(VOID)
{
    Expect_(kExchangeInit(&exchange, NULL), RK_ERR_SUCCESS);
    Expect_(kMboxInitCeiling(&queue, queueStorage, 1UL, 2U), RK_ERR_SUCCESS);
    Expect_(kMesgPoolInit(&pool, poolStorage, sizeof(ULONG), 1UL,
                          RK_MESG_PRIO_CEILING_NONE), RK_ERR_SUCCESS);
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "TBctrl",
                      controlStack, STACKSIZE, 2U, RK_PREEMPT), RK_ERR_SUCCESS);
    Expect_(kTaskInit(&workerHandle, WorkerTask, RK_NO_ARGS, "TBwork",
                      workerStack, STACKSIZE, 5U, RK_PREEMPT), RK_ERR_SUCCESS);
    Expect_(kMesgEndpointInit(&controlEndpoint, controlHandle, RK_MESG_SEND_RECV),
             RK_ERR_SUCCESS);
    Expect_(kMesgEndpointInit(&workerEndpoint, workerHandle, RK_MESG_SEND_RECV),
             RK_ERR_SUCCESS);
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    TestDeadlineArithmetic_();
    TestClockRollover_();
    for (UINT op = 0U; op < N_OPERATIONS; op++)
    {
        Prepare_(op);
        Start_(RK_NO_WAIT);
        Complete_((op <= QUEUE_JAM) ? RK_ERR_BUFFER_FULL : RK_ERR_BUFFER_EMPTY);
        Cleanup_();

        Prepare_(op);
        Start_(RK_MAX_PERIOD + 1UL);
        Complete_(RK_ERR_INVALID_TIMEOUT);
        Cleanup_();

        Prepare_(op);
        Start_(WAIT_TICKS);
        Complete_(RK_ERR_TIMEOUT);
        Budget_(WAIT_TICKS, WAIT_TICKS + 2UL);
        Cleanup_();

        Prepare_(op);
        Start_(WAIT_TICKS);
        (void)kSleep(1UL);
        CheckWaiting_();
        (void)kSleep(3UL);
        Grant_();
        Complete_(RK_ERR_SUCCESS);
        Cleanup_();

        if (op <= BROADCAST_RECV)
        {
            Prepare_(op);
            Start_(WAIT_TICKS);
            (void)kSleep(1UL);
            CheckWaiting_();
            for (UINT retry = 0U; retry < 3U; retry++)
            {
                (void)kSleep(3UL);
                WakeAndInvalidate_();
                (void)kSleep(1UL);
                CheckWaiting_();
            }
            Complete_(RK_ERR_TIMEOUT);
            Budget_(WAIT_TICKS, WAIT_TICKS + 2UL);
            printf("TB retry %s requested=%lu elapsed=%lu\r\n",
                   operationNames[op], (ULONG)requested, (ULONG)elapsed);

            Prepare_(op);
            Start_(WAIT_TICKS);
            (void)kSleep(1UL);
            CheckWaiting_();
            (void)kSleep(3UL);
            WakeAndInvalidate_();
            /* Delay dispatch beyond the deadline while its timer is cancelled. */
            Expect_(kDelay(WAIT_TICKS + 3UL), RK_ERR_SUCCESS);
            RK_TICK const dispatchDelay = kTickGet() - started;
            Complete_(RK_ERR_TIMEOUT);
            Budget_(dispatchDelay, dispatchDelay + 2UL);
        }

        Prepare_(op);
        Start_(RK_WAIT_FOREVER);
        (void)kSleep(1UL);
        CheckWaiting_();
        if (op <= BROADCAST_RECV)
        {
            for (UINT retry = 0U; retry < 3U; retry++)
            {
                WakeAndInvalidate_();
                (void)kSleep(1UL);
                CheckWaiting_();
            }
        }
        Grant_();
        Complete_(RK_ERR_SUCCESS);
        Cleanup_();
    }
    for (UINT op = 0U; op < N_OPERATIONS; op++)
        TestTaskRollover_(op);
    Check_((RK_BOOL)(budgetFailures == 0U), "finite timeout budget");
    printf("TB PASS finite timeout budgets, retries, late dispatch and task rollover (%u cases)\r\n",
           rolloverCases);
    Stop_();
}

VOID WorkerTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        Expect_(kEventGet(REQUEST, RK_EVENT_ALL, NULL, RK_WAIT_FOREVER), RK_ERR_SUCCESS);
        ULONG value = 0UL;
        RK_MESG *message = NULL;
        started = kTickGet();
        RK_ERR err;
        switch (operation)
        {
            case EXCHANGE_POST:
                err = kExchangePost(&exchange, &sentValue, requested);
                break;
            case QUEUE_SEND:
                err = kMesgQueueSend(&queue, &sentValue, requested);
                break;
            case QUEUE_JAM:
                err = kMesgQueueJam(&queue, &sentValue, requested);
                break;
            case QUEUE_RECV:
                err = kMesgQueueRecv(&queue, &value, requested);
                break;
            case BROADCAST_SEND:
                err = kMesgQueueBroadcastWaitN(&queue, &sentValue, 1U, requested, NULL);
                break;
            case BROADCAST_RECV:
                err = kMesgQueueBroadcastRecv(&queue, &value, requested);
                break;
            case MESG_ALLOC:
                err = kMesgAlloc(&pool, &message, requested);
                break;
            default:
                err = kMesgWait(RK_ANY_TASK, &message, requested);
                break;
        }
        finished = kTickGet();
        elapsed = K_TICK_DELTA(finished, started);
        workerResult = err;
        if (operation >= MESG_ALLOC)
            Check_((RK_BOOL)((message != NULL) == (err == RK_ERR_SUCCESS)),
                    "message grant matches result");
        if (message != NULL)
            Expect_(kMesgFree(message), RK_ERR_SUCCESS);
        if ((err == RK_ERR_SUCCESS) &&
            ((operation == QUEUE_RECV) || (operation == BROADCAST_RECV)))
            Check_((RK_BOOL)(value == sentValue), "received payload");
        Expect_(kEventSet(controlHandle, DONE), RK_ERR_SUCCESS);
    }
}
