/* SPDX-License-Identifier: Apache-2.0 */
/* Task broadcast fanout is inline; multi-receiver ISR wakeup is deferred. */
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

RK_DECLARE_BROADCAST_MESG(spare, spareStorage, VOID *)
RK_DECLARE_BROADCAST_MESG(irqMailbox, irqMailboxStorage, VOID *)
static UINT ids[3] = { 0U, 1U, 2U };
static RK_TASK_HANDLE *const workers[3] = {
    &worker1Handle, &worker2Handle, &lateHandle
};
static VOID *received[3];
static volatile RK_ERR results[3];
static volatile RK_TICK waits[3];
static ULONG payloadA = 0xA1UL;
static ULONG payloadB = 0xB2UL;
volatile unsigned RK_gQemuTestForceIsr = 0U;
static volatile RK_BOOL sleepProbe;
static VOID *volatile irqProbeObject;
static volatile UINT irqProbeSteps;
static volatile UINT irqProbeTicks;
static RK_TASK_HANDLE volatile irqProbeOwner;
static volatile RK_BOOL priorityProbe;

enum {
    PROBE_BROADCAST_TASK, PROBE_BROADCAST_ISR,
    PROBE_WAIT_TASK, PROBE_WAIT_ISR, N_PROBES
};

static VOID Check_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("BF FAIL %s\r\n", where);
        while (1) { }
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected,
                     CHAR const *const where)
{
    if (actual != expected)
    {
        printf("BF FAIL %s expected=%d actual=%d\r\n", where, expected, actual);
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
        ULONG const waiting = irqMailbox.waitingReceivers.size;
        if (waiting == (ULONG)count)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "receivers entered broadcast wait");
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
        RK_ERR const err = kPostProcJobEnq(RK_POSTPROC_JOB_BROADCAST_MESG_WAKE,
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
                     (RK_gRunPtr == irqProbeOwner) &&
                     ((RK_gRunPtr->preempt == RK_NO_PREEMPT) ||
                      (RK_gSchLock > 0U))),
            "broadcast owner permits interrupts while preventing task preemption");
    UINT ready = 0U;
    for (UINT i = 0U; i < 2U; i++)
        if ((*workers[i])->status == RK_READY)
            ready++;
    irqProbeSteps++;
    Check_((RK_BOOL)(ready == irqProbeSteps), "one receiver per atomic transition");
    if (priorityProbe == RK_TRUE)
    {
        if (irqProbeSteps == 2U)
            Check_(kTaskUpdateEffectivePrio(worker1Handle),
                    "demote first ready receiver before final dispatch");
        return;
    }
    if (irqProbeSteps != 1U)
        return;

    /* Pend real SysTick exceptions until the sleeping priority-0 task becomes
     * ready. They must execute before the second receiver is readied, while
     * scheduler protection keeps that task from running. */
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
            "ISR readies another task without preempting the broadcast owner");
}

static VOID ProbeInterrupts_(UINT const mode)
{
    RK_BOOL const fromISR = ((mode == PROBE_BROADCAST_ISR) ||
                             (mode == PROBE_WAIT_ISR)) ? RK_TRUE : RK_FALSE;
    RK_BOOL const waitForReceivers = (mode >= PROBE_WAIT_TASK) ? RK_TRUE : RK_FALSE;
    StartPair_(RK_WAIT_FOREVER);
    sleepProbe = RK_TRUE;
    Start_(2U, RK_WAIT_FOREVER);
    Check_((RK_BOOL)(lateHandle->status == RK_SLEEPING_DELAY),
            "interrupt probe task is sleeping");
    irqProbeSteps = 0U;
    irqProbeTicks = 0U;
    irqProbeObject = &irqMailbox;
    irqProbeOwner = (fromISR == RK_TRUE) ? RK_gPostProcTaskHandle : controlHandle;
    UINT count = 0U;
    VOID *message = &payloadA;
    UINT const savedLock = RK_gSchLock;
    if (fromISR == RK_TRUE)
    {
        kSchLock();
        RK_gQemuTestForceIsr = 1U;
    }
    RK_ERR err;
    if (waitForReceivers == RK_TRUE)
        err = kBroadcastMesgSendWaitN(&irqMailbox, &message, 2U, RK_NO_WAIT, &count);
    else
        err = kBroadcastMesgSend(&irqMailbox, &message, &count);
    if (fromISR == RK_TRUE)
    {
        RK_gQemuTestForceIsr = 0U;
        Check_((RK_BOOL)(irqProbeSteps == 0U), "ISR fanout remains deferred");
        kSchUnlock();
    }
    Expect_(err, RK_ERR_SUCCESS, "interrupt probe broadcast");
    Check_((RK_BOOL)((count == 2U) && (irqProbeSteps == 2U) &&
                     (irqProbeTicks > 0U) && (RK_gSchLock == savedLock)),
            "fanout completes in the correct owner and preserves scheduler lock");
    irqProbeObject = NULL;
    Done_(PAIR_DONE, message);
    Done_(DONE(2U), NULL);
}

static VOID TaskBroadcastFullQueue_(VOID)
{
    StartPair_(RK_WAIT_FOREVER);
    UINT count = 0U;
    VOID *message = &payloadB;
    kSchLock();
    UINT const savedLock = RK_gSchLock;
    FillPostProc_();
    RK_ERR const err = kBroadcastMesgSend(&irqMailbox, &message, &count);
    Expect_(err, RK_ERR_SUCCESS, "task broadcast ignores full deferred job queue");
    Check_((RK_BOOL)((count == 2U) && (worker1Handle->status == RK_READY) &&
                     (worker2Handle->status == RK_READY) &&
                     (RK_gSchLock == savedLock)),
            "task fanout finishes inline without releasing caller's scheduler lock");
    kSchUnlock();
    Done_(PAIR_DONE, message);
}

static VOID ProbeFinalPriority_(VOID)
{
    Check_(kTaskRaiseEffectivePrio(worker1Handle, 0U), "boost first receiver");
    Check_(kTaskRaiseEffectivePrio(worker2Handle, 1U), "boost second receiver");
    StartPair_(RK_WAIT_FOREVER);
    irqProbeSteps = 0U;
    irqProbeObject = &irqMailbox;
    irqProbeOwner = controlHandle;
    priorityProbe = RK_TRUE;
    UINT count = 0U;
    VOID *message = &payloadA;
    RK_ERR const err = kBroadcastMesgSend(&irqMailbox, &message, &count);
    Expect_(err, RK_ERR_SUCCESS, "priority-change broadcast");
    Check_((RK_BOOL)((count == 2U) && (irqProbeSteps == 2U) &&
                     (results[1] == RK_ERR_SUCCESS)),
            "remaining higher-priority receiver preempts caller after fanout");
    irqProbeObject = NULL;
    priorityProbe = RK_FALSE;
    Done_(PAIR_DONE, message);
    Check_(kTaskUpdateEffectivePrio(worker2Handle), "restore second receiver");
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}

VOID kApplicationInit(VOID)
{
    Expect_(kBroadcastMesgInit(&spare, spareStorage,
                               RK_BROADCAST_MESG_SIZE(VOID *)),
             RK_ERR_SUCCESS, "spare broadcast init");
    Expect_(kBroadcastMesgInit(&irqMailbox, irqMailboxStorage,
                               RK_BROADCAST_MESG_SIZE(VOID *)),
             RK_ERR_SUCCESS, "interrupt probe mailbox init");
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "BFctrl",
                      controlStack, STACKSIZE, 2U, RK_PREEMPT),
             RK_ERR_SUCCESS, "control init");
    Expect_(kTaskInit(&worker1Handle, WorkerTask, &ids[0], "BFwork1",
                      worker1Stack, STACKSIZE, 5U, RK_PREEMPT),
             RK_ERR_SUCCESS, "worker1 init");
    Expect_(kTaskInit(&worker2Handle, WorkerTask, &ids[1], "BFwork2",
                      worker2Stack, STACKSIZE, 6U, RK_PREEMPT),
             RK_ERR_SUCCESS, "worker2 init");
    /* A priority-0 task also tests ISR readiness under the task caller's
     * scheduler lock, as well as under non-preemptible PostProc execution. */
    Expect_(kTaskInit(&lateHandle, WorkerTask, &ids[2], "BFlate",
                      lateStack, STACKSIZE, 0U, RK_PREEMPT),
             RK_ERR_SUCCESS, "late receiver init");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    UINT count = 99U;
    VOID *message = &payloadA;

    /* A one-receiver ISR broadcast needs no job, even when the queue is full. */
    Start_(0U, RK_WAIT_FOREVER);
    WaitReceivers_(1U);
    kSchLock();
    FillPostProc_();
    RK_gQemuTestForceIsr = 1U;
    RK_ERR const err = kBroadcastMesgSend(&irqMailbox, &message, &count);
    RK_gQemuTestForceIsr = 0U;
    Expect_(err, RK_ERR_SUCCESS, "single receiver ISR broadcast");
    Check_((RK_BOOL)((count == 1U) && (worker1Handle->status == RK_READY)),
            "single receiver readied inline");
    kSchUnlock();
    Done_(DONE(0U), message);

    TaskBroadcastFullQueue_();
    for (UINT mode = 0U; mode < N_PROBES; mode++)
        ProbeInterrupts_(mode);
    ProbeFinalPriority_();

    printf("BF PASS BroadcastMesg fanout\r\n");
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
        else
            results[id] = kBroadcastMesgRecv(&irqMailbox, &received[id], waits[id]);
        Expect_(kEventSet(controlHandle, DONE(id)), RK_ERR_SUCCESS,
                 "receiver done");
    }
}
