/* SPDX-License-Identifier: Apache-2.0 */
/* Finite timeout budgets across normal wakes, retries, and delayed dispatch. */
#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 256U
#define WAIT_TICKS 20UL
#define SYNC_WAIT 500UL
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
static volatile RK_TICK elapsed;
static volatile RK_ERR workerResult;
static UINT budgetFailures;
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
        printf("TB FAIL %s: %s\r\n", operationNames[operation], where);
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
        printf("TB FAIL %s requested=%lu elapsed=%lu expected=%lu..%lu\r\n",
               operationNames[operation], (ULONG)requested, (ULONG)elapsed,
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

static VOID Complete_(RK_ERR const expected)
{
    Expect_(kEventGet(DONE, RK_EVENT_ALL, NULL, SYNC_WAIT), RK_ERR_SUCCESS);
    Expect_(workerResult, expected);
    Check_((RK_BOOL)((workerHandle->waitingQueuePtr == NULL) &&
                     (workerHandle->timeoutNode.listRefPtr == NULL) &&
                     (workerHandle->timeoutNode.timeoutType == 0U) &&
                     (workerHandle->timeOut == RK_FALSE)), "wait cleanup");
    Check_((RK_BOOL)((workerHandle->priority == 5U) &&
                     (workerHandle->mboxCeilingPtr == NULL)), "ceiling cleanup");
    Check_((RK_BOOL)((workerEndpoint.waitDestPtr == NULL) &&
                     (workerEndpoint.allocDestPtr == NULL) &&
                     (workerHandle->mesgQueueRecvBufPtr == NULL)),
            "destination cleanup");
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
    Check_((RK_BOOL)(budgetFailures == 0U), "finite timeout budget");
    printf("TB PASS finite timeout budgets, retries, late dispatch and rollover\r\n");
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
        elapsed = kTickGet() - started;
        workerResult = err;
        if (message != NULL)
            Expect_(kMesgFree(message), RK_ERR_SUCCESS);
        if ((err == RK_ERR_SUCCESS) &&
            ((operation == QUEUE_RECV) || (operation == BROADCAST_RECV)))
            Check_((RK_BOOL)(value == sentValue), "received payload");
        Expect_(kEventSet(controlHandle, DONE), RK_ERR_SUCCESS);
    }
}
