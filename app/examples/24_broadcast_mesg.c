/* SPDX-License-Identifier: Apache-2.0 */
/* BroadcastMesg works without message queues, using real tasks and IRQs. */
#include <kapi.h>
#include <ksystasks.h>
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
RK_DECLARE_BROADCAST_MESG(mailbox, mailboxStorage, Frame)
RK_DECLARE_BROADCAST_MESG(otherMailbox, otherStorage, Frame)
RK_DECLARE_BROADCAST_MESG(plainMailbox, plainStorage, Frame)

static UINT ids[N_RECEIVERS] = {0U, 1U, 2U};
static RK_PRIO nominalPrio[N_RECEIVERS] = {5U, 6U, 7U};
static RK_TASK_HANDLE *const receivers[N_RECEIVERS] = {
    &receiver1Handle, &receiver2Handle, &receiver3Handle
};
static volatile RK_BOOL lifecycle[N_RECEIVERS];
static volatile RK_BOOL explicitJoin[N_RECEIVERS];
static volatile RK_TICK waits[N_RECEIVERS];
static volatile RK_ERR expected[N_RECEIVERS];
static volatile RK_ERR results[N_RECEIVERS];
static Frame received[N_RECEIVERS];
static RK_BROADCAST_MESG *volatile receiveOn[N_RECEIVERS];
static volatile RK_BOOL lateProbe;
#if (RK_CONF_DYNAMIC_TASK == ON)
static RK_MEM_PARTITION receiverStackPool;
#endif
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
        printf("BM FAIL %s\r\n", where);
        while (1) { }
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const wanted,
                     CHAR const *const where)
{
    if (actual != wanted)
    {
        printf("BM FAIL %s expected=%d actual=%d\r\n", where, wanted, actual);
        while (1) { }
    }
}

static VOID Priority_(UINT const id, RK_BOOL const joined)
{
    RK_TASK_HANDLE const task = *receivers[id];
    Check_((RK_BOOL)((task->priority ==
                       ((joined == RK_TRUE) ? CEILING_PRIO : nominalPrio[id])) &&
                     (task->broadcastCeilingPtr ==
                       ((joined == RK_TRUE) ? &mailbox : NULL))),
            "effective priority and mailbox membership");
}

static VOID BroadcastIRQ_(VOID)
{
    irqWasISR = (kIsISR() != 0U) ? RK_TRUE : RK_FALSE;
#if (RK_CONF_ERR_CHECK == ON)
    if (irqValidate == RK_TRUE)
    {
        irqJoinResult = kBroadcastMesgJoin(&mailbox);
        irqLeaveResult = kBroadcastMesgLeave(&mailbox);
    }
    else
#endif
    {
        UINT count = 0U;
        irqResult = (irqWaitN == RK_TRUE)
                        ? kBroadcastMesgSendWaitN(&mailbox, &irqFrame, N_RECEIVERS,
                                               RK_NO_WAIT, &count)
                        : kBroadcastMesgSend(&mailbox, &irqFrame, &count);
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

static VOID StartOn_(UINT const id, RK_BOOL const joined, RK_TICK const timeout,
                      RK_ERR const result, RK_BROADCAST_MESG *const object)
{
    receiveOn[id] = object;
    lifecycle[id] = RK_FALSE;
    explicitJoin[id] = joined;
    waits[id] = timeout;
    expected[id] = result;
    results[id] = RK_ERR_ERROR;
    received[id].sequence = 0UL;
    received[id].value = 0UL;
    Expect_(kEventSet(*receivers[id], REQUEST), RK_ERR_SUCCESS, "start receiver");
}

static VOID Start_(UINT const id, RK_BOOL const joined, RK_TICK const timeout,
                    RK_ERR const result)
{
    StartOn_(id, joined, timeout, result, &mailbox);
}

static VOID WaitReceiversOn_(RK_BROADCAST_MESG *const object, UINT const count)
{
    for (RK_TICK tick = 0UL; tick < SYNC_WAIT; tick++)
    {
        if (object->waitingReceivers.size == (ULONG)count)
        {
            UINT parked = 0U, senders = 0U, active = 0U;
            Expect_(kBroadcastMesgQuery(object, &parked, &senders, &active),
                     RK_ERR_SUCCESS, "parked query");
            Check_((RK_BOOL)((parked == count) && (senders == 0U) && (active == 0U)),
                    "only uncommitted receivers reported as parked");
            for (UINT i = 0U; i < count; i++)
            {
                RK_TASK_HANDLE const task = *receivers[i];
                Priority_(i, object->prioCeilingEnabled);
                Check_((RK_BOOL)((task->status == RK_RECEIVING) &&
                                 (task->waitingQueuePtr == &object->waitingReceivers)),
                        "receiver parked under the ceiling");
            }
            return;
        }
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "receivers parked before deadline");
}

static VOID WaitReceivers_(UINT const count)
{
    WaitReceiversOn_(&mailbox, count);
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
                     (mailbox.activeReceivers == 0UL) &&
                     (controlHandle->priority == CONTROL_PRIO) &&
                     (controlHandle->broadcastCeilingPtr == NULL)),
            "mailbox drained and broadcaster priority unchanged");
}

static VOID Lifecycle_(UINT const id)
{
    Priority_(id, RK_FALSE);
    Expect_(kBroadcastMesgJoin(&mailbox), RK_ERR_SUCCESS, "explicit join");
    Priority_(id, RK_TRUE);
    Expect_(kBroadcastMesgJoin(&mailbox), RK_ERR_SUCCESS, "repeated join is idempotent");
    Priority_(id, RK_TRUE);
#if (RK_CONF_ERR_CHECK == ON)
    Expect_(kBroadcastMesgJoin(&otherMailbox), RK_ERR_TASK_INVALID_ST, "second membership");
    Expect_(kBroadcastMesgLeave(&otherMailbox), RK_ERR_TASK_INVALID_ST, "wrong mailbox leave");
    Priority_(id, RK_TRUE);
#endif
    Expect_(kBroadcastMesgLeave(&mailbox), RK_ERR_SUCCESS, "explicit leave");
    Priority_(id, RK_FALSE);
    Expect_(kBroadcastMesgLeave(&mailbox), RK_ERR_SUCCESS, "repeated leave");
    Expect_(kBroadcastMesgJoin(&plainMailbox), RK_ERR_SUCCESS, "no-ceiling join");
    Priority_(id, RK_FALSE);
    Expect_(kBroadcastMesgLeave(&plainMailbox), RK_ERR_SUCCESS, "no-ceiling leave");
    Priority_(id, RK_FALSE);
}

static VOID FillPostProc_(VOID)
{
    for (UINT i = 0U; i <= RK_NTHREADS; i++)
    {
        RK_ERR const err = kPostProcJobEnq(RK_POSTPROC_JOB_BROADCAST_MESG_WAKE,
                                           &plainMailbox, 0U);
        if (err == RK_ERR_NOWAIT)
            return;
        Expect_(err, RK_ERR_SUCCESS, "fill PostProc queue");
    }
    Check_(RK_FALSE, "PostProc queue reached capacity");
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
        if (sequence == 3UL)
        {
            FillPostProc_();
            TriggerIRQ_();
            Expect_(irqResult, RK_ERR_NOWAIT, "full ISR queue rejects broadcast");
            Check_((RK_BOOL)((irqCount == 0U) && (mailbox.activeReceivers == 0UL)),
                    "failed reservation committed no frame");
            WaitReceivers_(N_RECEIVERS);
            for (UINT i = 0U; i < N_RECEIVERS; i++)
                Check_((RK_BOOL)(((*receivers[i])->timeoutNode.timeoutType ==
                                  RK_TIMEOUT_BLOCKING) &&
                                 (received[i].sequence == 0UL)),
                        "failed reservation preserves timeouts and destinations");
            kSchUnlock();
            kSchLock();
        }
        TriggerIRQ_();
        Expect_(irqResult, RK_ERR_SUCCESS, "ISR broadcast");
        count = irqCount;
        Check_((RK_BOOL)(mailbox.activeReceivers == N_RECEIVERS),
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
        kSchLock();
        FillPostProc_();
        RK_ERR const err = (waitN == RK_TRUE)
                               ? kBroadcastMesgSendWaitN(&mailbox, &frame, N_RECEIVERS,
                                                      RK_NO_WAIT, &count)
                               : kBroadcastMesgSend(&mailbox, &frame, &count);
        Expect_(err, RK_ERR_SUCCESS, "task broadcast with full PostProc queue");
        kSchUnlock();
    }
    Check_((RK_BOOL)(count == N_RECEIVERS), "broadcast recipient count");
    UINT parked = 99U, active = 0U;
    Expect_(kBroadcastMesgQuery(&mailbox, &parked, NULL, &active), RK_ERR_SUCCESS,
             "committed query");
    Check_((RK_BOOL)((parked == 0U) && (active == N_RECEIVERS) &&
                     (mailbox.waitCalls == N_RECEIVERS)), "committed calls retained");
#if (RK_CONF_TRACE == ON)
    static RK_TRACE_OBJECT_INFO info[RK_CONF_TRACE_MAX_OBJECTS];
    UINT const objects = kTraceMesgSnapshot(info, RK_CONF_TRACE_MAX_OBJECTS);
    RK_BOOL found = RK_FALSE;
    for (UINT i = 0U; i < objects; i++)
        if (info[i].objPtr == &mailbox)
        {
            Check_((RK_BOOL)((info[i].objID == RK_BROADCAST_MESG_KOBJ_ID) &&
                             (info[i].capacity == 1UL) &&
                             (info[i].buffered == 1UL) && (info[i].active != 0U) &&
                             (info[i].objName[0] == 'B')), "broadcast trace snapshot");
            found = RK_TRUE;
        }
    Check_(found, "independent broadcast registered for tracing");
#endif
#if (RK_CONF_DYNAMIC_TASK == ON)
    RK_TASK_HANDLE const saved = receiver3Handle;
    Expect_(kTaskTerminate(&receiver3Handle), RK_ERR_TASK_INVALID_ST,
             "committed receiver cannot be terminated");
    Check_((RK_BOOL)(receiver3Handle == saved), "rejected termination keeps handle");
#endif

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

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
static VOID DynamicCases_(VOID)
{
    RK_BROADCAST_MESG_HANDLE obj = &mailbox;
    ULONG storage[RK_BROADCAST_MESG_SIZE(Frame)];
    Expect_(kBroadcastMesgCreate(&obj, storage, RK_BROADCAST_MESG_SIZE(Frame),
                                  CONTROL_PRIO), RK_ERR_OBJ_NOT_INIT,
             "allocation before partitions init");
    Check_((RK_BOOL)(obj == NULL), "failed create clears handle");
    Expect_(kObjPartitionsInit(), RK_ERR_SUCCESS, "partitions init");
#if (RK_CONF_DYNAMIC_BROADCAST_MESGS_MAX > 0U)
    for (UINT i = 0U; i < RK_CONF_DYNAMIC_BROADCAST_MESGS_MAX + 2U; i++)
    {
        Expect_(kBroadcastMesgCreate(&obj, storage, 3UL, CONTROL_PRIO),
                 RK_ERR_INVALID_MSG_SIZE, "failed init returns pool block");
        Check_((RK_BOOL)(obj == NULL), "failed init clears output");
    }
    RK_BROADCAST_MESG_HANDLE objects[RK_CONF_DYNAMIC_BROADCAST_MESGS_MAX];
    ULONG buffers[RK_CONF_DYNAMIC_BROADCAST_MESGS_MAX][RK_BROADCAST_MESG_SIZE(Frame)];
    for (UINT i = 0U; i < RK_CONF_DYNAMIC_BROADCAST_MESGS_MAX; i++)
        Expect_(kBroadcastMesgCreate(&objects[i], buffers[i],
                                      RK_BROADCAST_MESG_SIZE(Frame), CONTROL_PRIO),
                 RK_ERR_SUCCESS, "allocate fixed pool");
    Expect_(kBroadcastMesgCreate(&obj, storage, RK_BROADCAST_MESG_SIZE(Frame),
                                  CONTROL_PRIO), RK_ERR_MEM_EMPTY, "pool exhausted");
    Check_((RK_BOOL)(obj == NULL), "exhausted create clears output");
    obj = &mailbox;
    Expect_(kBroadcastMesgDestroy(&obj), RK_ERR_MEM_FREE, "static object rejected");
    Expect_(kBroadcastMesgJoin(objects[0]), RK_ERR_SUCCESS, "join dynamic object");
    RK_BROADCAST_MESG_HANDLE const saved = objects[0];
    Expect_(kBroadcastMesgDestroy(&objects[0]), RK_ERR_TASK_INVALID_ST,
             "membership prevents destroy");
    Check_((RK_BOOL)(objects[0] == saved), "busy destroy preserves handle");
    Expect_(kBroadcastMesgLeave(objects[0]), RK_ERR_SUCCESS, "leave dynamic object");
    for (UINT i = 0U; i < RK_CONF_DYNAMIC_BROADCAST_MESGS_MAX; i++)
    {
        Expect_(kBroadcastMesgDestroy(&objects[i]), RK_ERR_SUCCESS, "destroy object");
        Check_((RK_BOOL)(objects[i] == NULL), "destroy clears handle");
    }
    Expect_(kBroadcastMesgCreate(&obj, storage, RK_BROADCAST_MESG_SIZE(Frame),
                                  RK_BROADCAST_MESG_PRIO_CEILING_NONE),
             RK_ERR_SUCCESS, "reuse pool block");
    Check_((RK_BOOL)((obj->waitCalls == 0U) && (obj->joinedTasks == 0U) &&
                     (obj->activeReceivers == 0UL)), "reused object state reset");
    Expect_(kBroadcastMesgDestroy(&obj), RK_ERR_SUCCESS, "destroy reused object");
#else
    Expect_(kBroadcastMesgCreate(&obj, storage, RK_BROADCAST_MESG_SIZE(Frame),
                                  CONTROL_PRIO), RK_ERR_MEM_EMPTY, "zero-size pool");
    Check_((RK_BOOL)(obj == NULL), "zero-size pool clears output");
#endif
    printf("BM fixed-pool allocation and lifetime: PASS\r\n");
}
#endif

static VOID Reprioritize_(UINT const id, RK_PRIO const priority)
{
    RK_CR_AREA
    RK_CR_ENTER
    nominalPrio[id] = priority;
    (*receivers[id])->prioNominal = priority;
    kTaskUpdateEffectivePrioChain(*receivers[id]);
    RK_CR_EXIT
}

static VOID LateReceiver_(VOID)
{
    Frame first = {6UL, 0xABCDEF01UL};
    Frame second = {7UL, 0x10203040UL};
    StartOn_(0U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS, &plainMailbox);
    StartOn_(1U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS, &plainMailbox);
    WaitReceiversOn_(&plainMailbox, 2U);
    UINT count = 0U;
    Expect_(kBroadcastMesgSend(&plainMailbox, &first, &count), RK_ERR_SUCCESS,
             "first frame committed");
    Check_((RK_BOOL)((count == 2U) && (plainMailbox.activeReceivers == 2UL)),
            "first cohort has not consumed the frame yet");
    /* The new receiver runs before the earlier cohort, but must park for a new
     * frame. The broadcaster then waits for both its arrival and the old frame
     * to drain. This exercises two consecutive cohorts without queue storage.
     */
    Reprioritize_(2U, 2U);
    lateProbe = RK_TRUE;
    StartOn_(2U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS, &plainMailbox);
    Expect_(kBroadcastMesgSendWaitN(&plainMailbox, &second, 1U, SYNC_WAIT, &count),
             RK_ERR_SUCCESS, "second frame waits for first cohort to drain");
    Check_((RK_BOOL)(count == 1U), "late receiver belongs only to second cohort");
#if (RK_CONF_DYNAMIC_TASK == ON)
    RK_TASK_HANDLE const saved = receiver3Handle;
    Check_((RK_BOOL)(receiver3Handle->broadcastCeilingPtr == NULL),
            "in-flight receiver has no ceiling membership");
    Expect_(kTaskTerminate(&receiver3Handle), RK_ERR_TASK_INVALID_ST,
             "committed delivery prevents termination without a ceiling");
    Check_((RK_BOOL)(receiver3Handle == saved), "delivery retains receiver handle");
#endif
    Finish_(2U, &first);
    Expect_(kEventGet(DONE(2U), RK_EVENT_ALL, NULL, SYNC_WAIT), RK_ERR_SUCCESS,
             "late receiver completion");
    Check_((RK_BOOL)((received[2].sequence == second.sequence) &&
                     (received[2].value == second.value) &&
                     (plainMailbox.activeReceivers == 0UL) &&
                     (plainMailbox.waitCalls == 0U) &&
                     (plainMailbox.waitingReceivers.size == 0UL) &&
                     (plainMailbox.waitingSenders.size == 0UL)),
            "late receiver did not consume the earlier frame");
    lateProbe = RK_FALSE;
    Reprioritize_(2U, 7U);
    printf("BM consecutive cohorts and late receiver: PASS\r\n");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
    DynamicCases_();
#endif
    Check_((RK_BOOL)((mailbox.prioCeilingEnabled == RK_TRUE) &&
                     (mailbox.prioCeiling == CEILING_PRIO)), "ceiling init");

    lifecycle[0] = RK_TRUE;
    expected[0] = RK_ERR_SUCCESS;
    Expect_(kEventSet(receiver1Handle, REQUEST), RK_ERR_SUCCESS, "lifecycle request");
    Finish_(1U, NULL);
    printf("BM explicit join/leave: PASS\r\n");

#if (RK_CONF_ERR_CHECK == ON)
    Expect_(kBroadcastMesgJoin(&mailbox), RK_ERR_INVALID_PRIO, "ceiling admission");
    Frame unused;
    Expect_(kBroadcastMesgRecv(&mailbox, &unused, SHORT_WAIT), RK_ERR_INVALID_PRIO,
             "lazy ceiling admission");
    irqValidate = RK_TRUE;
    TriggerIRQ_();
    Expect_(irqJoinResult, RK_ERR_INVALID_ISR_PRIMITIVE, "ISR join rejected");
    Expect_(irqLeaveResult, RK_ERR_INVALID_ISR_PRIMITIVE, "ISR leave rejected");
    Start_(0U, RK_FALSE, RK_MAX_PERIOD + 1UL, RK_ERR_INVALID_TIMEOUT);
    Finish_(1U, NULL);
    printf("BM validation and rejection cleanup: PASS\r\n");
#endif

    Start_(0U, RK_FALSE, RK_NO_WAIT, RK_ERR_BUFFER_EMPTY);
    Finish_(1U, NULL);
    for (UINT joined = 0U; joined < 2U; joined++)
    {
        Start_(0U, (joined != 0U) ? RK_TRUE : RK_FALSE, SHORT_WAIT, RK_ERR_TIMEOUT);
        WaitReceivers_(1U);
        Finish_(1U, NULL);
    }
    printf("BM parked boost and timeout restoration: PASS\r\n");

    Cohort_(RK_FALSE, RK_FALSE, RK_FALSE, 1UL);
    Cohort_(RK_TRUE, RK_FALSE, RK_TRUE, 2UL);
    Cohort_(RK_FALSE, RK_TRUE, RK_FALSE, 3UL);
    Cohort_(RK_TRUE, RK_TRUE, RK_TRUE, 4UL);
    printf("BM task/ISR fanout and explicit membership: PASS\r\n");

    Frame wanted = {5UL, 0x12345678UL};
    Start_(0U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS);
    Start_(1U, RK_FALSE, RK_WAIT_FOREVER, RK_ERR_SUCCESS);
    WaitReceivers_(2U);
    Start_(2U, RK_TRUE, RK_WAIT_FOREVER, RK_ERR_SUCCESS);
    UINT count = 0U;
    Expect_(kBroadcastMesgSendWaitN(&mailbox, &wanted, N_RECEIVERS, SYNC_WAIT, &count),
             RK_ERR_SUCCESS, "wait for third receiver");
    Check_((RK_BOOL)((count == N_RECEIVERS) &&
                     (controlHandle->broadcastMinReceivers == 0U)), "WaitN cleanup");
    Finish_(N_RECEIVERS, &wanted);
    LateReceiver_();
#if (RK_CONF_DYNAMIC_TASK == ON)
    Expect_(kTaskTerminate(&receiver3Handle), RK_ERR_SUCCESS,
             "completed receiver can terminate");
    Check_((RK_BOOL)((receiver3Handle == NULL) &&
                     (receiverStackPool.nFreeBlocks == 1UL)), "receiver stack returned");
#endif
    printf("BM PASS BroadcastMesg ceiling\r\n");
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
            RK_BROADCAST_MESG *const object = receiveOn[id];
            if ((lateProbe == RK_TRUE) && (id == 2U))
                Check_((RK_BOOL)(object->activeReceivers == 2UL),
                        "late receiver arrived while previous frame was active");
            if (explicitJoin[id] == RK_TRUE)
            {
                Expect_(kBroadcastMesgJoin(object), RK_ERR_SUCCESS, "join before receive");
                Priority_(id, RK_TRUE);
            }
            err = kBroadcastMesgRecv(object, &received[id], waits[id]);
            Expect_(err, expected[id], "broadcast receive result");
            /* Explicit membership survives success and expiry; lazy membership
             * must already be gone when the receive call returns. */
            Priority_(id, explicitJoin[id]);
            if (explicitJoin[id] == RK_TRUE)
                Expect_(kBroadcastMesgLeave(object), RK_ERR_SUCCESS, "leave after receive");
            Priority_(id, RK_FALSE);
        }
        results[id] = err;
        Expect_(kEventSet(controlHandle, DONE(id)), RK_ERR_SUCCESS, "receiver done");
    }
}

VOID kApplicationInit(VOID)
{
    Expect_(kBroadcastMesgInitCeiling(&mailbox, mailboxStorage, RK_BROADCAST_MESG_SIZE(Frame),
                            CEILING_PRIO), RK_ERR_SUCCESS, "mailbox init");
    Expect_(kTraceNameObject(&mailbox, "Broadcast"), RK_ERR_SUCCESS, "broadcast name");
    Expect_(kBroadcastMesgInitCeiling(&otherMailbox, otherStorage, RK_BROADCAST_MESG_SIZE(Frame),
                            CEILING_PRIO), RK_ERR_SUCCESS, "other mailbox init");
    Expect_(kBroadcastMesgInit(&plainMailbox, plainStorage, RK_BROADCAST_MESG_SIZE(Frame)),
             RK_ERR_SUCCESS, "plain mailbox init");
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "BMctrl",
                      controlStack, STACKSIZE, CONTROL_PRIO, RK_PREEMPT),
             RK_ERR_SUCCESS, "control init");
    Expect_(kTaskInit(&receiver1Handle, ReceiverTask, &ids[0], "BMrecv1",
                      receiver1Stack, STACKSIZE, nominalPrio[0], RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver1 init");
    Expect_(kTaskInit(&receiver2Handle, ReceiverTask, &ids[1], "BMrecv2",
                      receiver2Stack, STACKSIZE, nominalPrio[1], RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver2 init");
#if (RK_CONF_DYNAMIC_TASK == ON)
    Expect_(kMemPartitionInit(&receiverStackPool, receiver3Stack,
                              sizeof(receiver3Stack), 1UL),
             RK_ERR_SUCCESS, "receiver stack pool");
    RK_DYNAMIC_TASK_ATTR attr = {ReceiverTask, &ids[2], "BMrecv3", nominalPrio[2],
                                 RK_PREEMPT, &receiverStackPool};
    Expect_(kTaskSpawn(&attr, &receiver3Handle), RK_ERR_SUCCESS, "spawn receiver3");
#else
    Expect_(kTaskInit(&receiver3Handle, ReceiverTask, &ids[2], "BMrecv3",
                      receiver3Stack, STACKSIZE, nominalPrio[2], RK_PREEMPT),
             RK_ERR_SUCCESS, "receiver3 init");
#endif
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}
