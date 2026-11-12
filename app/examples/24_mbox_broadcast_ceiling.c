/* SPDX-License-Identifier: Apache-2.0 */
/* Mailbox cohort ceilings with real task dispatch and external interrupts. */
#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 256U
#define N_RECEIVERS 3U
#define CONTROL_PRIO 1U
#define CEILING_PRIO 3U
#define REQUEST RK_EVENT_1
#define DONE(ID) ((RK_TASK_EVENT)(RK_EVENT_2 << (ID)))
#define SHORT_WAIT 12UL
#define SYNC_WAIT 500UL

#if defined(QEMU_MACHINE_LM3S6965EVB)
#define TEST_IRQ_MASK 1UL /* GPIO, IRQ 0 */
#elif defined(QEMU_MACHINE_MICROBIT)
#define TEST_IRQ_MASK (1UL << 2) /* UART0, IRQ 2; trace RX is disabled. */
#else
#error "This regression requires a supported QEMU target"
#endif

typedef struct Frame
{
    ULONG sequence;
    ULONG value;
} Frame;

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(receiver1Handle, ReceiverTask, receiver1Stack, STACKSIZE)
RK_DECLARE_TASK(receiver2Handle, ReceiverTask, receiver2Stack, STACKSIZE)
RK_DECLARE_TASK(receiver3Handle, ReceiverTask, receiver3Stack, STACKSIZE)
RK_DECLARE_MBOX(mailbox, mailboxStorage, Frame)
RK_DECLARE_MBOX(otherMailbox, otherStorage, Frame)
RK_DECLARE_MBOX(plainMailbox, plainStorage, Frame)

static UINT ids[N_RECEIVERS] = {0U, 1U, 2U};
static RK_PRIO const nominalPrio[N_RECEIVERS] = {5U, 6U, 7U};
static RK_TASK_HANDLE *const receivers[N_RECEIVERS] = {
    &receiver1Handle, &receiver2Handle, &receiver3Handle
};
static volatile RK_BOOL lifecycle[N_RECEIVERS];
static volatile RK_BOOL explicitJoin[N_RECEIVERS];
static volatile RK_TICK waits[N_RECEIVERS];
static volatile RK_ERR expected[N_RECEIVERS];
static volatile RK_ERR results[N_RECEIVERS];
static Frame received[N_RECEIVERS];
static Frame irqFrame;
static volatile RK_BOOL irqDone;
static volatile RK_BOOL irqWasISR;
static volatile RK_BOOL irqWaitN;
static volatile RK_BOOL irqValidate;
static volatile RK_ERR irqResult;
static volatile UINT irqCount;
#if (RK_CONF_ERR_CHECK == ON)
static volatile RK_ERR irqJoinResult;
static volatile RK_ERR irqLeaveResult;
#endif

static VOID Check_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("MC FAIL %s\r\n", where);
        while (1) { }
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const wanted,
                     CHAR const *const where)
{
    if (actual != wanted)
    {
        printf("MC FAIL %s expected=%d actual=%d\r\n", where, wanted, actual);
        while (1) { }
    }
}

static VOID Priority_(UINT const id, RK_BOOL const joined)
{
    RK_TASK_HANDLE const task = *receivers[id];
    Check_((RK_BOOL)((task->priority ==
                       ((joined == RK_TRUE) ? CEILING_PRIO : nominalPrio[id])) &&
                     (task->mboxCeilingPtr ==
                       ((joined == RK_TRUE) ? &mailbox : NULL))),
            "effective priority and mailbox membership");
}

static VOID BroadcastIRQ_(VOID)
{
    irqWasISR = (kIsISR() != 0U) ? RK_TRUE : RK_FALSE;
#if (RK_CONF_ERR_CHECK == ON)
    if (irqValidate == RK_TRUE)
    {
        irqJoinResult = kMboxJoin(&mailbox);
        irqLeaveResult = kMboxLeave(&mailbox);
    }
    else
#endif
    {
        UINT count = 0U;
        irqResult = (irqWaitN == RK_TRUE)
                        ? kMboxBroadcastWaitN(&mailbox, &irqFrame, N_RECEIVERS,
                                               RK_NO_WAIT, &count)
                        : kMboxBroadcast(&mailbox, &irqFrame, &count);
        irqCount = count;
    }
    irqDone = RK_TRUE;
}

#if defined(QEMU_MACHINE_LM3S6965EVB)
VOID GPIO_Handler(VOID)
#else
VOID UART0_Handler(VOID)
#endif
{
    BroadcastIRQ_();
}

static VOID TriggerIRQ_(VOID)
{
    irqDone = RK_FALSE;
    irqWasISR = RK_FALSE;
    *(volatile ULONG *)0xE000E100UL = TEST_IRQ_MASK; /* NVIC ISER */
    *(volatile ULONG *)0xE000E200UL = TEST_IRQ_MASK; /* NVIC ISPR */
    RK_DSB
    RK_ISB
    for (ULONG spin = 0UL; (irqDone == RK_FALSE) && (spin < 1000000UL); spin++)
        RK_NOP
    *(volatile ULONG *)0xE000E180UL = TEST_IRQ_MASK; /* NVIC ICER */
    Check_((RK_BOOL)((irqDone == RK_TRUE) && (irqWasISR == RK_TRUE)),
            "external interrupt executed");
}

static VOID Start_(UINT const id, RK_BOOL const joined, RK_TICK const timeout,
                    RK_ERR const result)
{
    lifecycle[id] = RK_FALSE;
    explicitJoin[id] = joined;
    waits[id] = timeout;
    expected[id] = result;
    results[id] = RK_ERR_ERROR;
    received[id].sequence = 0UL;
    received[id].value = 0UL;
    Expect_(kEventSet(*receivers[id], REQUEST), RK_ERR_SUCCESS, "start receiver");
}

static VOID WaitReceivers_(UINT const count)
{
    for (RK_TICK tick = 0UL; tick < SYNC_WAIT; tick++)
    {
        if (mailbox.waitingReceivers.size == (ULONG)count)
        {
            for (UINT i = 0U; i < count; i++)
            {
                RK_TASK_HANDLE const task = *receivers[i];
                Priority_(i, RK_TRUE);
                Check_((RK_BOOL)((task->status == RK_RECEIVING) &&
                                 (task->waitingQueuePtr == &mailbox.waitingReceivers)),
                        "receiver parked under the ceiling");
            }
            return;
        }
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "receivers parked before deadline");
}

static VOID Finish_(UINT const count, Frame const *const wanted)
{
    RK_TASK_EVENT events = 0UL;
    for (UINT i = 0U; i < count; i++)
        events |= DONE(i);
    Expect_(kEventGet(events, RK_EVENT_ALL, NULL, SYNC_WAIT), RK_ERR_SUCCESS,
             "receiver completion");
    for (UINT i = 0U; i < count; i++)
    {
        Expect_(results[i], expected[i], "receiver result");
        Priority_(i, RK_FALSE);
        if (wanted != NULL)
            Check_((RK_BOOL)((received[i].sequence == wanted->sequence) &&
                             (received[i].value == wanted->value)),
                    "every receiver obtained the copied frame");
    }
    Check_((RK_BOOL)((mailbox.waitingReceivers.size == 0UL) &&
                     (mailbox.waitingSenders.size == 0UL) &&
                     (mailbox.broadcastReceivers == 0UL) &&
                     (mailbox.ringBuf.nFull == 0UL) &&
                     (controlHandle->priority == CONTROL_PRIO) &&
                     (controlHandle->mboxCeilingPtr == NULL)),
            "mailbox drained and broadcaster priority unchanged");
}

static VOID Lifecycle_(UINT const id)
{
    Priority_(id, RK_FALSE);
    Expect_(kMboxJoin(&mailbox), RK_ERR_SUCCESS, "explicit join");
    Priority_(id, RK_TRUE);
    Expect_(kMboxJoin(&mailbox), RK_ERR_SUCCESS, "repeated join is idempotent");
    Priority_(id, RK_TRUE);
#if (RK_CONF_ERR_CHECK == ON)
    Expect_(kMboxJoin(&otherMailbox), RK_ERR_TASK_INVALID_ST, "second membership");
    Expect_(kMboxLeave(&otherMailbox), RK_ERR_TASK_INVALID_ST, "wrong mailbox leave");
    Priority_(id, RK_TRUE);
#endif
    Expect_(kMboxLeave(&mailbox), RK_ERR_SUCCESS, "explicit leave");
    Priority_(id, RK_FALSE);
    Expect_(kMboxLeave(&mailbox), RK_ERR_SUCCESS, "repeated leave");
    Expect_(kMboxJoin(&plainMailbox), RK_ERR_SUCCESS, "no-ceiling join");
    Priority_(id, RK_FALSE);
    Expect_(kMboxLeave(&plainMailbox), RK_ERR_SUCCESS, "no-ceiling leave");
    Priority_(id, RK_FALSE);
}

static VOID Cohort_(RK_BOOL const joined, RK_BOOL const fromISR,
                     RK_BOOL const waitN, ULONG const sequence)
{
    Frame frame = {sequence, sequence ^ 0xA55A1234UL};
    Frame const wanted = frame;
    for (UINT i = 0U; i < N_RECEIVERS; i++)
        Start_(i, (i == 0U) ? joined : RK_FALSE, SHORT_WAIT, RK_ERR_SUCCESS);
    WaitReceivers_(N_RECEIVERS);

    UINT count = 0U;
    if (fromISR == RK_TRUE)
    {
        irqValidate = RK_FALSE;
        irqWaitN = waitN;
        irqFrame = frame;
        kSchLock();
        TriggerIRQ_();
        Expect_(irqResult, RK_ERR_SUCCESS, "ISR broadcast");
        count = irqCount;
        Check_((RK_BOOL)(mailbox.broadcastReceivers == N_RECEIVERS),
                "ISR committed the complete cohort");
        RK_TICK const started = kTickGet();
        while ((RK_TICK)(kTickGet() - started) < SHORT_WAIT + 2UL) { }
        for (UINT i = 0U; i < N_RECEIVERS; i++)
        {
            RK_TASK_HANDLE const task = *receivers[i];
            Priority_(i, RK_TRUE);
            Check_((RK_BOOL)((task->status == RK_RECEIVING) &&
                             (task->timeoutNode.timeoutType == 0U) &&
                             (task->timeOut == RK_FALSE) &&
                             (results[i] == RK_ERR_ERROR)),
                    "deferred delivery retains ceiling and survives timeout");
        }
        irqFrame.sequence = 0UL;
        irqFrame.value = 0UL;
        kSchUnlock();
    }
    else
    {
        RK_ERR const err = (waitN == RK_TRUE)
                               ? kMboxBroadcastWaitN(&mailbox, &frame, N_RECEIVERS,
                                                      RK_NO_WAIT, &count)
                               : kMboxBroadcast(&mailbox, &frame, &count);
        Expect_(err, RK_ERR_SUCCESS, "task broadcast");
    }
    Check_((RK_BOOL)(count == N_RECEIVERS), "broadcast recipient count");
    for (UINT i = 0U; i < N_RECEIVERS; i++)
    {
        Priority_(i, RK_TRUE);
        Check_((RK_BOOL)(((*receivers[i])->status == RK_READY) &&
                         (results[i] == RK_ERR_ERROR)),
                "all receivers ready under ceiling before resuming");
    }
    frame.sequence = 0UL;
    frame.value = 0UL;
    Finish_(N_RECEIVERS, &wanted);
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    Check_((RK_BOOL)((mailbox.mboxPrioCeilingEnabled == RK_TRUE) &&
                     (mailbox.mboxPrioCeiling == CEILING_PRIO)), "ceiling init");

    lifecycle[0] = RK_TRUE;
    expected[0] = RK_ERR_SUCCESS;
    Expect_(kEventSet(receiver1Handle, REQUEST), RK_ERR_SUCCESS, "lifecycle request");
    Finish_(1U, NULL);
    printf("MC explicit join/leave: PASS\r\n");

#if (RK_CONF_ERR_CHECK == ON)
    Expect_(kMboxJoin(&mailbox), RK_ERR_INVALID_PRIO, "ceiling admission");
    Frame unused;
    Expect_(kMboxBroadcastRecv(&mailbox, &unused, SHORT_WAIT), RK_ERR_INVALID_PRIO,
             "lazy ceiling admission");
    irqValidate = RK_TRUE;
    TriggerIRQ_();
    Expect_(irqJoinResult, RK_ERR_INVALID_ISR_PRIMITIVE, "ISR join rejected");
    Expect_(irqLeaveResult, RK_ERR_INVALID_ISR_PRIMITIVE, "ISR leave rejected");
    Start_(0U, RK_FALSE, RK_MAX_PERIOD + 1UL, RK_ERR_INVALID_TIMEOUT);
    Finish_(1U, NULL);
    printf("MC validation and rejection cleanup: PASS\r\n");
#endif

    Start_(0U, RK_FALSE, RK_NO_WAIT, RK_ERR_BUFFER_EMPTY);
    Finish_(1U, NULL);
    for (UINT joined = 0U; joined < 2U; joined++)
    {
        Start_(0U, (joined != 0U) ? RK_TRUE : RK_FALSE, SHORT_WAIT, RK_ERR_TIMEOUT);
        WaitReceivers_(1U);
        Finish_(1U, NULL);
    }
    printf("MC parked boost and timeout restoration: PASS\r\n");

    Cohort_(RK_FALSE, RK_FALSE, RK_FALSE, 1UL);
    Cohort_(RK_TRUE, RK_FALSE, RK_TRUE, 2UL);
    Cohort_(RK_FALSE, RK_TRUE, RK_FALSE, 3UL);
    Cohort_(RK_TRUE, RK_TRUE, RK_TRUE, 4UL);
    printf("MC task/ISR fanout and explicit membership: PASS\r\n");

    Frame wanted = {5UL, 0x12345678UL};
    Start_(0U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS);
    Start_(1U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS);
    WaitReceivers_(2U);
    Start_(2U, RK_TRUE, RK_WAIT_FOREVER, RK_ERR_SUCCESS);
    UINT count = 0U;
    Expect_(kMboxBroadcastWaitN(&mailbox, &wanted, N_RECEIVERS, SYNC_WAIT, &count),
             RK_ERR_SUCCESS, "wait for third receiver");
    Check_((RK_BOOL)((count == N_RECEIVERS) &&
                     (controlHandle->mboxBcastMinRecv == 0U)), "WaitN cleanup");
    Finish_(N_RECEIVERS, &wanted);
    printf("MC PASS mailbox broadcast ceiling\r\n");
    while (1)
        (void)kSleep(1000UL);
}

VOID ReceiverTask(VOID *args)
{
    UINT const id = *(UINT *)args;
    while (1)
    {
        Expect_(kEventGet(REQUEST, RK_EVENT_ANY, NULL, RK_WAIT_FOREVER),
                 RK_ERR_SUCCESS, "receiver request");
        RK_ERR err = RK_ERR_SUCCESS;
        if (lifecycle[id] == RK_TRUE)
        {
            Lifecycle_(id);
        }
        else
        {
            if (explicitJoin[id] == RK_TRUE)
            {
                Expect_(kMboxJoin(&mailbox), RK_ERR_SUCCESS, "join before receive");
                Priority_(id, RK_TRUE);
            }
            err = kMboxBroadcastRecv(&mailbox, &received[id], waits[id]);
            Expect_(err, expected[id], "broadcast receive result");
            /* Explicit membership survives success and expiry; lazy membership
             * must already be gone when the receive call returns. */
            Priority_(id, explicitJoin[id]);
            if (explicitJoin[id] == RK_TRUE)
                Expect_(kMboxLeave(&mailbox), RK_ERR_SUCCESS, "leave after receive");
            Priority_(id, RK_FALSE);
        }
        results[id] = err;
        Expect_(kEventSet(controlHandle, DONE(id)), RK_ERR_SUCCESS, "receiver done");
    }
}

VOID kApplicationInit(VOID)
{
    Expect_(kMboxInitCeiling(&mailbox, mailboxStorage, RK_MBOX_MESG_SIZE(Frame),
                            CEILING_PRIO), RK_ERR_SUCCESS, "mailbox init");
    Expect_(kMboxInitCeiling(&otherMailbox, otherStorage, RK_MBOX_MESG_SIZE(Frame),
                            CEILING_PRIO), RK_ERR_SUCCESS, "other mailbox init");
    Expect_(kMboxInit(&plainMailbox, plainStorage, RK_MBOX_MESG_SIZE(Frame)),
             RK_ERR_SUCCESS, "plain mailbox init");
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "MCctrl",
                      controlStack, STACKSIZE, CONTROL_PRIO, RK_PREEMPT),
             RK_ERR_SUCCESS, "control init");
    Expect_(kTaskInit(&receiver1Handle, ReceiverTask, &ids[0], "MCrecv1",
                      receiver1Stack, STACKSIZE, nominalPrio[0], RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver1 init");
    Expect_(kTaskInit(&receiver2Handle, ReceiverTask, &ids[1], "MCrecv2",
                      receiver2Stack, STACKSIZE, nominalPrio[1], RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver2 init");
    Expect_(kTaskInit(&receiver3Handle, ReceiverTask, &ids[2], "MCrecv3",
                      receiver3Stack, STACKSIZE, nominalPrio[2], RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver3 init");
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}
