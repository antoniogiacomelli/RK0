/* SPDX-License-Identifier: Apache-2.0 */
/* Exchange broadcast commits in the caller and defers multi-receiver wakeup. */
#include <kapi.h>
#include <ksystasks.h>
#include <stdio.h>

#define STACKSIZE 256U
#define REQUEST RK_EVENT_1
#define DONE(ID) ((RK_TASK_EVENT)(RK_EVENT_2 << (ID)))
#define PAIR_DONE (DONE(0U) | DONE(1U))
#define SHORT_WAIT 20UL
#define SYNC_WAIT 500UL

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(worker1Handle, WorkerTask, worker1Stack, STACKSIZE)
RK_DECLARE_TASK(worker2Handle, WorkerTask, worker2Stack, STACKSIZE)
RK_DECLARE_TASK(lateHandle, WorkerTask, lateStack, STACKSIZE)

static RK_EXCHANGE_HANDLE exchange;
static RK_EXCHANGE spare;
RK_DECLARE_MBOX(irqMailbox, irqMailboxStorage, VOID *)
static UINT ids[3] = { 0U, 1U, 2U };
static RK_TASK_HANDLE *const workers[3] = {
    &worker1Handle, &worker2Handle, &lateHandle
};
static VOID *received[3];
static volatile RK_ERR results[3];
static volatile RK_TICK waits[3];
static ULONG payloadA = 0xA1UL;
static ULONG payloadB = 0xB2UL;
static ULONG payloadC = 0xC3UL;
volatile unsigned RK_gQemuTestForceIsr = 0U;
static volatile RK_BOOL mailboxMode;
static volatile RK_BOOL sleepProbe;
static VOID *volatile irqProbeObject;
static volatile UINT irqProbeSteps;
static volatile UINT irqProbeTicks;

static VOID Check_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("XB FAIL %s\r\n", where);
        while (1) { }
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected,
                     CHAR const *const where)
{
    if (actual != expected)
    {
        printf("XB FAIL %s expected=%d actual=%d\r\n", where, expected, actual);
        while (1) { }
    }
}

static VOID Start_(UINT const id, RK_TICK const timeout)
{
    received[id] = NULL;
    results[id] = RK_ERR_ERROR;
    waits[id] = timeout;
    Expect_(kEventSet(*workers[id], REQUEST), RK_ERR_SUCCESS, "start receiver");
}

static VOID WaitReceivers_(UINT const count)
{
    for (RK_TICK i = 0UL; i < SYNC_WAIT; i++)
    {
        ULONG const waiting = (mailboxMode == RK_TRUE)
                                  ? irqMailbox.waitingReceivers.size
                                  : exchange->waitingReceivers.size;
        if (waiting == (ULONG)count)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "receivers entered exchange");
}

static VOID StartPair_(RK_TICK const timeout)
{
    Start_(0U, timeout);
    Start_(1U, timeout);
    WaitReceivers_(2U);
}

static VOID Done_(RK_TASK_EVENT const events, VOID *const message)
{
    Expect_(kEventGet(events, RK_EVENT_ALL, NULL, SYNC_WAIT), RK_ERR_SUCCESS,
             "receiver completion");
    for (UINT i = 0U; i < 3U; i++)
        if ((events & DONE(i)) != 0UL)
        {
            Expect_(results[i], RK_ERR_SUCCESS, "receiver result");
            Check_((RK_BOOL)(received[i] == message), "receiver pointer identity");
        }
}

static VOID FillPostProc_(VOID)
{
    for (UINT i = 0U; i <= RK_NTHREADS; i++)
    {
        RK_ERR const err = kPostProcJobEnq(RK_POSTPROC_JOB_EXCHG_BROADCAST_WAKE,
                                           &spare, 0U);
        if (err == RK_ERR_NOWAIT)
            return;
        Expect_(err, RK_ERR_SUCCESS, "fill deferred job queue");
    }
    Check_(RK_FALSE, "deferred queue reaches capacity");
}

/* Unit-test-only hook, called outside each receiver's atomic transition. */
VOID kPostProcTestBroadcastStep(VOID *const object)
{
    if (object != irqProbeObject)
        return;

    unsigned mask;
    RK_ASM volatile("MRS %0, PRIMASK" : "=r"(mask));
    Check_((RK_BOOL)((mask == 0U) && (kIsISR() == 0U) &&
                     (RK_gRunPtr == RK_gPostProcTaskHandle) &&
                     (RK_gRunPtr->preempt == RK_NO_PREEMPT)),
            "PostProc broadcast keeps interrupts enabled between receivers");
    UINT ready = 0U;
    for (UINT i = 0U; i < 2U; i++)
        if ((*workers[i])->status == RK_READY)
            ready++;
    irqProbeSteps++;
    Check_((RK_BOOL)(ready == irqProbeSteps), "one receiver per atomic transition");
    if (irqProbeSteps != 1U)
        return;

    /* Pend real SysTick exceptions until the sleeping priority-0 task becomes
     * ready. They must execute before the second receiver is readied, while
     * PostProc's non-preemptible policy keeps that task from running. */
    for (UINT tick = 0U; tick < (UINT)SHORT_WAIT + 5U; tick++)
    {
        RK_TICK const before = RK_gRunTime.globalTick;
        RK_REG_SCB_ICSR = (1UL << 26); /* PENDSTSET */
        RK_DSB
        RK_ISB
        for (UINT spin = 0U; (spin < 100000U) &&
                             (RK_gRunTime.globalTick == before); spin++)
        {
            RK_NOP
        }
        Check_((RK_BOOL)(RK_gRunTime.globalTick != before),
                "real SysTick preempts broadcast processing");
        irqProbeTicks++;
        if (lateHandle->status == RK_READY)
            break;
    }
    Check_((RK_BOOL)((lateHandle->status == RK_READY) &&
                     (results[2] == RK_ERR_ERROR)),
            "ISR readies another task without preempting PostProc");
}

static VOID ProbeInterrupts_(RK_BOOL const useMailbox)
{
    mailboxMode = useMailbox;
    StartPair_(RK_WAIT_FOREVER);
    sleepProbe = RK_TRUE;
    Start_(2U, RK_WAIT_FOREVER);
    Check_((RK_BOOL)(lateHandle->status == RK_SLEEPING_DELAY),
            "interrupt probe task is sleeping");
    irqProbeSteps = 0U;
    irqProbeTicks = 0U;
    irqProbeObject = (useMailbox == RK_TRUE) ? (VOID *)&irqMailbox : (VOID *)exchange;
    UINT count = 0U;
    VOID *message = &payloadA;
    if (useMailbox == RK_TRUE)
        Expect_(kMesgQueueBroadcast(&irqMailbox, &message, &count), RK_ERR_SUCCESS,
                 "mailbox interrupt probe broadcast");
    else
        Expect_(kExchangeBroadcast(exchange, message, &count), RK_ERR_SUCCESS,
                 "exchange interrupt probe broadcast");
    Check_((RK_BOOL)((count == 2U) && (irqProbeSteps == 2U) &&
                     (irqProbeTicks > 0U)), "interrupt probe completed in PostProc");
    irqProbeObject = NULL;
    Done_(PAIR_DONE, message);
    Done_(DONE(2U), NULL);
    mailboxMode = RK_FALSE;
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}

VOID kApplicationInit(VOID)
{
    Expect_(kObjPartitionsInit(), RK_ERR_SUCCESS, "dynamic partitions init");
    Expect_(kExchangeCreate(&exchange, NULL), RK_ERR_SUCCESS, "exchange create");
    Expect_(kExchangeInit(&spare, NULL), RK_ERR_SUCCESS, "spare exchange init");
    Expect_(kMesgQueueInit(&irqMailbox, irqMailboxStorage,
                           RK_MESGQ_MESG_SIZE(VOID *), 1UL),
             RK_ERR_SUCCESS, "interrupt probe mailbox init");
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "XBctrl",
                      controlStack, STACKSIZE, 2U, RK_PREEMPT),
             RK_ERR_SUCCESS, "control init");
    Expect_(kTaskInit(&worker1Handle, WorkerTask, &ids[0], "XBwork1",
                      worker1Stack, STACKSIZE, 5U, RK_PREEMPT),
             RK_ERR_SUCCESS, "worker1 init");
    Expect_(kTaskInit(&worker2Handle, WorkerTask, &ids[1], "XBwork2",
                      worker2Stack, STACKSIZE, 6U, RK_PREEMPT),
             RK_ERR_SUCCESS, "worker2 init");
    /* Equal to PostProc: becoming ready first lets this task join after the
     * broadcast commit but before its deferred wake job runs. */
    Expect_(kTaskInit(&lateHandle, WorkerTask, &ids[2], "XBlate",
                      lateStack, STACKSIZE, 0U, RK_PREEMPT),
             RK_ERR_SUCCESS, "late receiver init");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    UINT count = 99U;
    VOID *mail = NULL;

    /* A one-receiver broadcast needs no job, even when the queue is full. */
    Start_(0U, RK_WAIT_FOREVER);
    WaitReceivers_(1U);
    kSchLock();
    FillPostProc_();
    Expect_(kExchangeBroadcast(exchange, &payloadA, &count), RK_ERR_SUCCESS,
             "single receiver broadcast");
    Check_((RK_BOOL)((count == 1U) && (worker1Handle->status == RK_READY) &&
                     (exchange->pendingBroadcastReceivers.size == 0UL)),
            "single receiver readied inline");
    kSchUnlock();
    Done_(DONE(0U), &payloadA);

    StartPair_(SHORT_WAIT);
    kSchLock();
    Expect_(kExchangeBroadcast(exchange, &payloadA, &count), RK_ERR_SUCCESS,
             "task broadcast commit");
    Check_((RK_BOOL)((count == 2U) &&
                     (exchange->waitingReceivers.size == 0UL) &&
                     (exchange->pendingBroadcastReceivers.size == 2UL)),
            "committed cohort awaiting PostProc");
    for (UINT i = 0U; i < 2U; i++)
        Check_((RK_BOOL)((received[i] == &payloadA) &&
                         ((*workers[i])->status == RK_RECEIVING) &&
                         ((*workers[i])->timeoutNode.timeoutType == 0U) &&
                         (results[i] == RK_ERR_ERROR)),
                "commit delivers pointer and cancels timeout without readying");
    Check_(kTaskRaiseEffectivePrio(worker2Handle, 1U),
            "reprioritise committed receiver");
    Check_((RK_BOOL)(K_GET_TCB_ADDR(
                        exchange->pendingBroadcastReceivers.listDummy.nextPtr) ==
                     worker1Handle),
            "priority change preserves committed cohort order");
    UINT receivers = 0U;
    Expect_(kExchangeQuery(exchange, NULL, &receivers, NULL), RK_ERR_SUCCESS,
             "pending receiver query");
    Check_((RK_BOOL)(receivers == 2U), "query counts committed waiters");
    RK_EXCHANGE_HANDLE const saved = exchange;
    Expect_(kExchangeDestroy(&exchange), RK_ERR_TASK_INVALID_ST,
             "pending broadcast prevents destroy");
    Check_((RK_BOOL)(exchange == saved), "busy destroy preserves handle");

    Expect_(kExchangePost(exchange, &payloadB, RK_NO_WAIT), RK_ERR_SUCCESS,
             "post while broadcast pending");
    Expect_(kExchangeOverwrite(exchange, &payloadC), RK_ERR_SUCCESS,
             "overwrite while broadcast pending");
    Expect_(kExchangePend(exchange, &mail, RK_NO_WAIT), RK_ERR_SUCCESS,
             "receive subsequent slot value");
    Check_((RK_BOOL)(mail == &payloadC), "ordinary traffic uses slot");
    Expect_(kExchangeBroadcast(exchange, &payloadB, &count), RK_ERR_SUCCESS,
             "later broadcast stores when no new receivers");
    Check_((RK_BOOL)(count == 0U), "committed cohort excluded from later broadcast");
    Expect_(kExchangePend(exchange, &mail, RK_NO_WAIT), RK_ERR_SUCCESS,
             "receive later broadcast slot");
    Check_((RK_BOOL)(mail == &payloadB), "later pointer remains distinct");

    RK_TICK const started = kTickGet();
    while ((RK_TICK)(kTickGet() - started) < SHORT_WAIT + 5UL) { }
    for (UINT i = 0U; i < 2U; i++)
        Check_((RK_BOOL)(((*workers[i])->status == RK_RECEIVING) &&
                         ((*workers[i])->timeOut == RK_FALSE)),
                "committed delivery survives delayed PostProc");
    kSchUnlock();
    Done_(PAIR_DONE, &payloadA);
    Check_(kTaskUpdateEffectivePrio(worker2Handle), "restore receiver priority");

    /* A real late task reaches pend after commit and before PostProc. */
    StartPair_(RK_WAIT_FOREVER);
    kSchLock();
    Start_(2U, RK_WAIT_FOREVER);
    RK_gQemuTestForceIsr = 1U;
    RK_ERR const isrErr = kExchangeBroadcast(exchange, &payloadB, &count);
    RK_gQemuTestForceIsr = 0U;
    Expect_(isrErr, RK_ERR_SUCCESS, "ISR broadcast commit");
    Check_((RK_BOOL)((count == 2U) &&
                     (exchange->pendingBroadcastReceivers.size == 2UL)),
            "ISR defers multi-receiver wakeup");
    kSchUnlock();
    Done_(PAIR_DONE, &payloadB);
    Check_((RK_BOOL)((results[2] == RK_ERR_ERROR) && (received[2] == NULL) &&
                     (lateHandle->waitingQueuePtr == &exchange->waitingReceivers)),
            "late receiver excluded from committed cohort");
    Expect_(kExchangePost(exchange, &payloadC, RK_NO_WAIT), RK_ERR_SUCCESS,
             "separate delivery to late receiver");
    Done_(DONE(2U), &payloadC);

    StartPair_(SYNC_WAIT);
    kSchLock();
    FillPostProc_();
    Expect_(kExchangeBroadcast(exchange, &payloadA, &count), RK_ERR_NOWAIT,
             "deferred queue full rejects broadcast");
    Check_((RK_BOOL)((count == 0U) && (received[0] == NULL) &&
                     (received[1] == NULL) &&
                     (exchange->waitingReceivers.size == 2UL) &&
                     (exchange->pendingBroadcastReceivers.size == 0UL) &&
                     (worker1Handle->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING) &&
                     (worker2Handle->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)),
            "failed reservation preserves pointers membership and timeouts");
    kSchUnlock();
    Expect_(kExchangeBroadcast(exchange, &payloadC, &count), RK_ERR_SUCCESS,
             "retry after PostProc drains jobs");
    Check_((RK_BOOL)(count == 2U), "retry selects original receivers");
    Done_(PAIR_DONE, &payloadC);
    ProbeInterrupts_(RK_FALSE);
    ProbeInterrupts_(RK_TRUE);
    Expect_(kExchangeDestroy(&exchange), RK_ERR_SUCCESS,
             "destroy after deferred work completes");
    Check_((RK_BOOL)(exchange == NULL), "destroy clears handle");

    printf("XB PASS deferred exchange broadcast\r\n");
    while (1)
        (void)kSleep(1000UL);
}

VOID WorkerTask(VOID *args)
{
    UINT const id = *(UINT const *)args;
    while (1)
    {
        Expect_(kEventGet(REQUEST, RK_EVENT_ALL, NULL, RK_WAIT_FOREVER),
                 RK_ERR_SUCCESS, "receiver request");
        if ((id == 2U) && (sleepProbe == RK_TRUE))
        {
            sleepProbe = RK_FALSE;
            results[id] = kSleep(SHORT_WAIT);
        }
        else if (mailboxMode == RK_TRUE)
            results[id] = kMesgQueueBroadcastRecv(&irqMailbox, &received[id], waits[id]);
        else
            results[id] = kExchangePend(exchange, &received[id], waits[id]);
        Expect_(kEventSet(controlHandle, DONE(id)), RK_ERR_SUCCESS,
                 "receiver done");
    }
}
