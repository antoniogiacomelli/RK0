/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.72.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/**
 * @warning
 * For ARMv6M some examples will overflow RAM because by default every kernel
 * service is ON.
 *
 */


/**
 * @note
 * This file is intentionally written as executable documentation. Select one
 * example with RK0_APP_EXAMPLE, build it, and watch the logger/trace output to
 * see how the primitive behaves under the scheduler.
 */

#define APP_BARRIER_SHARED (1U<<0)
#define APP_TRACE_EXERCISE (1U<<1)
#define APP_SYNCH_MESG_CONTROLLER (1U<<2)
#define APP_TASK_EVENTS (1U<<3)
#define APP_MBOX_BROADCAST_RECV (1U<<4)
#define APP_SYNCH_MESG_HANDOFF (1U<<5)
#define APP_NAMED_COMM_SHOWCASE (1U<<6)
#define APP_ASYNCH_DIRECT_MESG (1U<<7)
#define APP_ASYNCH_DIRECT_MESG2 (1U<<8)

#ifndef RK0_APP_EXAMPLE
#define RK0_APP_EXAMPLE APP_ASYNCH_DIRECT_MESG
#endif

#include <kapi.h>
/* Configure the application logger facility here */
#include <logger.h>
#include <qemu_uart.h>
#include <stdio.h>
int main(void)
{
    /*
     * Boot sequence:
     * 1. kCoreInit() starts the CPU/board layer.
     * 2. kInit() starts RK0 and calls kApplicationInit().
     */
    kCoreInit();

    kInit();

    while (1)
    {
        /* Returning from kInit() is treated as an application fault. */
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}


#if (RK0_APP_EXAMPLE == APP_TASK_EVENTS)
/*** TASK EVENT FLAGS CONTROLLER ***/
/*
 * Pattern:
 *
 *     Sensor task   ---- SAMPLE flag ----+
 *                                        +--> Control task waits for ALL
 *     Setpoint task -- SETPOINT flag ----+
 *
 *     Alert source -- WARN/FAULT flags ----> Alert task waits for ANY
 *
 * Task event flags are per-task signal bits in the receiver TCB. kEventSet()
 * ORs bits into the target task's event register. kEventGet() waits until ANY
 * or ALL requested bits are present, copies the matched register value to the
 * caller if requested, and clears the requested bits on success.
 *
 * This is signal semantics, not message semantics: repeated SAMPLE posts before
 * Control wakes still leave one SAMPLE bit set. If each occurrence matters, use
 * a counting semaphore or a message queue instead.
 */

#define LOG_PRIORITY 5U
#if defined(QEMU_MACHINE_MICROBIT)
#define STACKSIZE 128
#else
#define STACKSIZE 160
#endif
#define EVT_CONTROL_PRIO 2U
#define EVT_ALERT_PRIO 2U
#define EVT_SOURCE_PRIO 3U
#define EVT_ALARM_PRIO 4U

#define EVT_SAMPLE_FLAG RK_EVENT_1
#define EVT_SETPOINT_FLAG RK_EVENT_2
#define EVT_WARN_FLAG RK_EVENT_3
#define EVT_FAULT_FLAG RK_EVENT_4
#define EVT_CONTROL_MASK ((RK_TASK_EVENT)(EVT_SAMPLE_FLAG | EVT_SETPOINT_FLAG))
#define EVT_ALERT_MASK ((RK_TASK_EVENT)(EVT_WARN_FLAG | EVT_FAULT_FLAG))

RK_DECLARE_TASK(evtControlHandle, EvtControlTask, evtControlStack, STACKSIZE)
RK_DECLARE_TASK(evtAlertHandle, EvtAlertTask, evtAlertStack, STACKSIZE)
RK_DECLARE_TASK(evtSensorHandle, EvtSensorTask, evtSensorStack, STACKSIZE)
RK_DECLARE_TASK(evtSetpointHandle, EvtSetpointTask, evtSetpointStack, STACKSIZE)
RK_DECLARE_TASK(evtAlarmHandle, EvtAlarmTask, evtAlarmStack, STACKSIZE)

static volatile UINT evtSampleSeq;
static volatile UINT evtSetpointSeq;
static volatile UINT evtWarnSeq;
static volatile UINT evtFaultSeq;
static volatile INT evtSampleMv;
static volatile INT evtSetpointMv;

VOID kApplicationInit(VOID)
{
    /*
     * API setup:
     * 1. Create the tasks that will receive event bits.
     * 2. Create the tasks that will set those bits.
     * 3. Clear the receiver registers before the first wait.
     */
    RK_ERR err = kTaskInit(&evtControlHandle, EvtControlTask, RK_NO_ARGS,
                             "EvtCtl", evtControlStack, STACKSIZE,
                             EVT_CONTROL_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&evtAlertHandle, EvtAlertTask, RK_NO_ARGS, "EvtAlr",
                      evtAlertStack, STACKSIZE, EVT_ALERT_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&evtSensorHandle, EvtSensorTask, RK_NO_ARGS, "EvtSens",
                      evtSensorStack, STACKSIZE, EVT_SOURCE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&evtSetpointHandle, EvtSetpointTask, RK_NO_ARGS,
                      "EvtSet", evtSetpointStack, STACKSIZE, EVT_SOURCE_PRIO,
                      RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&evtAlarmHandle, EvtAlarmTask, RK_NO_ARGS, "EvtSrc",
                      evtAlarmStack, STACKSIZE, EVT_ALARM_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * Clear the target task registers explicitly so the example starts from a
     * known state even after a debugger restart that preserves RAM.
     */
    err = kEventClear(evtControlHandle, EVT_CONTROL_MASK);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kEventClear(evtAlertHandle, EVT_ALERT_MASK);
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID EvtControlTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_TASK_EVENT gotFlags = 0UL;
        RK_TASK_EVENT afterFlags = 0UL;

        /*
         * ALL mode waits until both inputs have arrived at least once. On
         * success, SAMPLE and SETPOINT are cleared from this task's register.
         */
        RK_ERR err = kEventGet(EVT_CONTROL_MASK, RK_EVENT_ALL, &gotFlags,
                               RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        err = kEventQuery(NULL, &afterFlags);
        K_ASSERT(err == RK_ERR_SUCCESS);

        INT const errorMv = evtSetpointMv - evtSampleMv;
        logPost("EvtCtl ALL mask=%lx sampleSeq=%u setSeq=%u",
                (ULONG)(gotFlags & EVT_CONTROL_MASK), evtSampleSeq,
                evtSetpointSeq);
        logPost("EvtCtl errorMv=%d pending=%lx", errorMv,
                (ULONG)(afterFlags & EVT_CONTROL_MASK));
    }
}

VOID EvtAlertTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_TASK_EVENT gotFlags = 0UL;

        /*
         * ANY mode wakes for either alert bit. If both bits were already set,
         * gotFlags shows both and both requested bits are cleared on return.
         */
        RK_ERR err = kEventGet(EVT_ALERT_MASK, RK_EVENT_ANY, &gotFlags,
                               RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        logPost("EvtAlr ANY mask=%lx warnSeq=%u faultSeq=%u",
                (ULONG)(gotFlags & EVT_ALERT_MASK), evtWarnSeq, evtFaultSeq);
    }
}

VOID EvtSensorTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        evtSampleSeq++;
        evtSampleMv = 980 + (INT)((evtSampleSeq * 41U) % 300U);

        /* kEventSet() ORs SAMPLE into EvtCtl's per-task event register. */
        RK_ERR err = kEventSet(evtControlHandle, EVT_SAMPLE_FLAG);
        K_ASSERT(err == RK_ERR_SUCCESS);
        logPost("EvtSens set SAMPLE n=%u", evtSampleSeq);

        kSleepPeriodic(RK_MS_TO_TICKS(160));
    }
}

VOID EvtSetpointTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        evtSetpointSeq++;
        evtSetpointMv = 1120 + (INT)((evtSetpointSeq % 3U) * 40U);

        /* kEventSet() does not count repeats; it leaves SETPOINT asserted. */
        RK_ERR err = kEventSet(evtControlHandle, EVT_SETPOINT_FLAG);
        K_ASSERT(err == RK_ERR_SUCCESS);
        logPost("EvtSet set SETP n=%u", evtSetpointSeq);

        kSleepPeriodic(RK_MS_TO_TICKS(520));
    }
}

VOID EvtAlarmTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_TASK_EVENT flags = EVT_WARN_FLAG;

        evtWarnSeq++;
        if ((evtWarnSeq % 4U) == 0U)
        {
            evtFaultSeq++;
            flags |= EVT_FAULT_FLAG;
        }

        /* Multiple bits may be posted together; EvtAlr waits for ANY of them. */
        RK_ERR err = kEventSet(evtAlertHandle, flags);
        K_ASSERT(err == RK_ERR_SUCCESS);
        logPost("EvtSrc set mask=%lx", (ULONG)flags);

        kSleepPeriodic(RK_MS_TO_TICKS(300));
    }
}

#elif (RK0_APP_EXAMPLE == APP_MBOX_BROADCAST_RECV)
/*** MAILBOX BROADCAST / BROADCAST-RECEIVE ***/
/*
 * Pattern:
 *
 *     one broadcaster -> all tasks currently blocked in broadcast receive
 *
 * A broadcast mailbox is a single-message queue. kMboxBroadcast() deposits one
 * message only when at least one task is already blocked in
 * kMboxBroadcastRecv(). Each blocked receiver gets the same message. The last
 * receiver drains the single mailbox slot, allowing the next broadcast round.
 */

#define LOG_PRIORITY 5U
#define STACKSIZE 256
#define MBOX_RX_COUNT 3U
#define MBOX_BCAST_PRIO 1U
#define MBOX_RX1_PRIO 2U
#define MBOX_RX2_PRIO 2U
#define MBOX_RX3_PRIO 2U
#define MBOX_PARKED_FLAGS ((RK_TASK_EVENT)0x111UL)
#define MBOX_RX_PERIOD_TICKS RK_MS_TO_TICKS(200)

typedef struct
{
    UINT seq;
    UINT sample;
    UINT checksum;
    UINT reserved;
} MboxBroadcastMsg;

RK_DECLARE_TASK(mboxTxHandle, MboxTxTask, mboxTxStack, STACKSIZE)
RK_DECLARE_TASK(mboxRx1Handle, MboxRx1Task, mboxRx1Stack, STACKSIZE)
RK_DECLARE_TASK(mboxRx2Handle, MboxRx2Task, mboxRx2Stack, STACKSIZE)
RK_DECLARE_TASK(mboxRx3Handle, MboxRx3Task, mboxRx3Stack, STACKSIZE)

RK_DECLARE_MBOX(mboxBroadcast, mboxBroadcastBuf, MboxBroadcastMsg)

static volatile UINT mboxTxSeq;
static volatile UINT mboxRxPass[MBOX_RX_COUNT];

static UINT MboxChecksum_(MboxBroadcastMsg const *const msgPtr)
{
    return (msgPtr->seq ^ msgPtr->sample ^ 0xA55A5AA5U);
}

static VOID MboxVerifyMsg_(UINT const receiverIdx,
                           MboxBroadcastMsg const *const msgPtr)
{
    K_ASSERT(receiverIdx < MBOX_RX_COUNT);
    K_ASSERT(msgPtr->seq != 0U);
    K_ASSERT(msgPtr->checksum == MboxChecksum_(msgPtr));
    K_ASSERT(msgPtr->seq > mboxRxPass[receiverIdx]);
    mboxRxPass[receiverIdx] = msgPtr->seq;
}

static RK_TASK_EVENT MboxParkedFlag_(UINT const receiverIdx)
{
    K_ASSERT(receiverIdx < MBOX_RX_COUNT);
    return ((RK_TASK_EVENT)(RK_EVENT_1 << (receiverIdx * 4U)));
}

static RK_TASK_EVENT MboxParkBarrierWait_(VOID)
{
    RK_TASK_EVENT parkedFlags = 0UL;
    RK_TASK_EVENT const requiredEvent = MBOX_PARKED_FLAGS;

    /*
     * The broadcaster waits until every receiver has announced that it is about
     * to block in kMboxBroadcastRecv(). The ALL wait clears these parked bits.
     */
    RK_ERR err = kEventGet(requiredEvent, RK_OPT_EVENT_ALL,
                           &parkedFlags, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    K_ASSERT((parkedFlags & requiredEvent) == requiredEvent);

    return (parkedFlags);
}

static inline VOID MboxRecvLoop_(UINT const receiverIdx)
{
    while (1)
    {
        MboxBroadcastMsg msg = {0U, 0U, 0U, 0U};
        kSchLock();
        /*
         * Announce "I am parked" and enter kMboxBroadcastRecv() without letting
         * the broadcaster run between the two operations.
         */
        RK_ERR err = kEventSet(mboxTxHandle, MboxParkedFlag_(receiverIdx));
        K_ASSERT(err == RK_ERR_SUCCESS);

        /*
         * kMboxBroadcastRecv() blocks until one broadcast message is deposited.
         * Each parked receiver receives the same copied message.
         */
        err = kMboxBroadcastRecv(&mboxBroadcast, &msg, RK_WAIT_FOREVER);
        kSchUnlock();
        K_ASSERT(err == RK_ERR_SUCCESS);
        MboxVerifyMsg_(receiverIdx, &msg);
        logPost("MboxRx%u seq=%u sample=%u",
                receiverIdx + 1U, msg.seq, msg.sample);

        err = kSleepRelease(MBOX_RX_PERIOD_TICKS);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

VOID kApplicationInit(VOID)
{
    MboxBroadcastMsg bootMsg = {0U, 0U, 0U, 0U};
    UINT nRecv = 99U;

    /*
     * API setup:
     * 1. Initialize the one-slot mailbox storage.
     * 2. Probe broadcast admission before receivers exist.
     * 3. Create one broadcaster task and several broadcast receivers.
     */
    RK_ERR err = kMboxInit(&mboxBroadcast, mboxBroadcastBuf,
                           RK_MESGQ_MESG_SIZE(MboxBroadcastMsg));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&mboxBroadcast, "BcMbox");
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * Broadcast has synchronous-message admission: no blocked broadcast
     * receiver means no message is deposited.
     */
    err = kMboxBroadcast(&mboxBroadcast, &bootMsg, &nRecv);
    K_ASSERT(err == RK_ERR_BUFFER_EMPTY);
    K_ASSERT(nRecv == 0U);

    err = kTaskInit(&mboxTxHandle, MboxTxTask, RK_NO_ARGS, "MboxTx",
                      mboxTxStack, STACKSIZE, MBOX_BCAST_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&mboxRx1Handle, MboxRx1Task, RK_NO_ARGS, "MboxR1",
                      mboxRx1Stack, STACKSIZE, MBOX_RX1_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTaskInit(&mboxRx2Handle, MboxRx2Task, RK_NO_ARGS, "MboxR2",
                      mboxRx2Stack, STACKSIZE, MBOX_RX2_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTaskInit(&mboxRx3Handle, MboxRx3Task, RK_NO_ARGS, "MboxR3",
                      mboxRx3Stack, STACKSIZE, MBOX_RX3_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    RK_INIT_OBJ_PARTITIONS
    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID MboxTxTask(VOID *args)
{
    RK_UNUSEARGS

    logPost("Mbox broadcast rx=%u txPrio=%u", MBOX_RX_COUNT,
            MBOX_BCAST_PRIO);

    while (1)
    {
        UINT nRecv = 0U;
        UINT nQueued = 99U;
        UINT nWaitingReceivers = 0U;
        UINT nWaitingSenders = 99U;
        MboxBroadcastMsg msg = {0U, 0U, 0U, 0U};
        RK_ERR err = RK_ERR_SUCCESS;
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
        RK_MUTEX_HANDLE mutexPtr = NULL;
#endif
        RK_TASK_EVENT const parkedFlags = MboxParkBarrierWait_();

        err = kMboxQueryWaitingReceivers(&mboxBroadcast, &nWaitingReceivers);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(nWaitingReceivers == MBOX_RX_COUNT);

        if (mboxTxSeq != 0U)
        {
            K_ASSERT(mboxRxPass[0] == mboxTxSeq);
            K_ASSERT(mboxRxPass[1] == mboxTxSeq);
            K_ASSERT(mboxRxPass[2] == mboxTxSeq);
        }

        /* Query calls let the example assert the receiver side is parked. */
        err = kMboxQueryMessageCount(&mboxBroadcast, &nQueued);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(nQueued == 0U);

        err = kMboxQueryWaitingSenders(&mboxBroadcast, &nWaitingSenders);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(nWaitingSenders == 0U);

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
        err = kMutexCreate(&mutexPtr, RK_PRIO_INHERITANCE);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(mutexPtr != NULL);
        err = kMutexLock(mutexPtr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        mboxTxSeq++;
        err = kMutexUnlock(mutexPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        err = kMutexDestroy(&mutexPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(mutexPtr == NULL);
#else
        mboxTxSeq++;
#endif

        msg.seq = mboxTxSeq;
        msg.sample = 1000U + (mboxTxSeq * 17U);
        msg.checksum = MboxChecksum_(&msg);
        /*
         * kMboxBroadcast() succeeds only because receivers are already waiting;
         * nRecv returns how many blocked tasks received this copied message.
         */
        err = kMboxBroadcast(&mboxBroadcast, &msg, &nRecv);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(nRecv == MBOX_RX_COUNT);

        logPost("MboxTx seq=%u targets=%u parked=%lx",
                mboxTxSeq, nRecv, (ULONG)(parkedFlags & MBOX_PARKED_FLAGS));
    }
}

VOID MboxRx1Task(VOID *args)
{
    RK_UNUSEARGS
    logPost("MboxRx1Task started\n\r");
    MboxRecvLoop_(0U);
}

VOID MboxRx2Task(VOID *args)
{
    RK_UNUSEARGS
    logPost("MboxRx2Task started\n\r");
    MboxRecvLoop_(1U);
}

VOID MboxRx3Task(VOID *args)
{
    RK_UNUSEARGS
    logPost("MboxRx2Task started\n\r");

    MboxRecvLoop_(2U);
}

#elif (RK0_APP_EXAMPLE == APP_SYNCH_MESG_CONTROLLER)
/*** SAME-PRIORITY SYNCHRONOUS MESSAGE CONTROLLER PIPELINE ***/
/*
 * Pattern:
 *
 *     Sense -> Filt -> Ctrl -> Act -> Sense
 *
 * The application models a small sampled controller. Sense produces a raw
 * millivolt sample, Filt smooths it, Ctrl computes a duty-cycle command, and
 * Act applies the command. All four tasks run at CTRL_PIPE_PRIO.
 *
 * A ControlFrame is copied stage-to-stage by Synchronous Message. A send
 * completes only when the next stage has copied the frame into its own storage,
 * so a fast producer cannot outrun a slow consumer. This is the core lesson:
 * Synchronous Message is unbuffered message passing, not a queue and not a
 * request/reply RPC.
 *
 * Act uses kSleepPeriodic(CTRL_PIPE_PERIOD_TICKS) before returning the frame to
 * Sense. That periodic release models the control period without accumulating
 * execution-time drift, and gives the logger/trace task a regular chance to
 * run.
 */

#define LOG_PRIORITY 5U
#define STACKSIZE 160
#define CTRL_PIPE_PRIO 2U
#define CTRL_PIPE_PERIOD_TICKS RK_MS_TO_TICKS(250)

typedef struct
{
    UINT seq;
    INT setpointMv;
    INT rawMv;
    INT filteredMv;
    INT errorMv;
    UINT dutyPermille;
} ControlFrame;

/* Each pipeline stage owns one task-backed Synchronous Message receive endpoint. */
RK_DECLARE_TASK(senseHandle, SenseTask, senseStack, STACKSIZE)
RK_DECLARE_TASK(filterHandle, FilterTask, filterStack, STACKSIZE)
RK_DECLARE_TASK(ctrlHandle, CtrlTask, ctrlStack, STACKSIZE)
RK_DECLARE_TASK(actHandle, ActTask, actStack, STACKSIZE)

/* Receive one copied frame into the running stage's storage. */
static VOID CtrlPipeRecv_(ControlFrame *const framePtr)
{
    ULONG mesgBytes = 0UL;
    RK_ERR err = kSyncRecv(framePtr, &mesgBytes, RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
    {
        logError("%s recv err %d", RK_RUNNING_NAME, err);
    }
    if (mesgBytes != sizeof(ControlFrame))
    {
        logError("%s recv bytes %lu", RK_RUNNING_NAME, mesgBytes);
    }
    else
    {
        logPost("%s recv: %lu bytes", RK_RUNNING_NAME, mesgBytes);
    }
}

/* Copy the frame to the next stage and wait until that copy completes. */
static VOID CtrlPipeSend_(RK_TASK_HANDLE const receiverHandle,
                          ControlFrame const *const framePtr)
{
    RK_ERR err = kSynchSendWait(receiverHandle, framePtr,
                                sizeof(ControlFrame), RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
    {
        logError("%s send err %d", RK_RUNNING_NAME, err);
    }
    else
    {
        logPost("%s sent: %d bytes", RK_RUNNING_NAME, sizeof(ControlFrame));
    }
}

/* Convert the proportional command to a 0..1000 permille duty cycle. */
static UINT CtrlClampDuty_(INT const command)
{
    if (command < 0)
    {
        return (0U);
    }
    if (command > 1000)
    {
        return (1000U);
    }
    return ((UINT)command);
}

VOID kApplicationInit(VOID)
{
    /*
     * Same-priority tasks are deliberate here. The demo is about cooperative
     * sequencing through Synchronous Message, so priority inheritance should
     * not be part of the normal trace output for these stages.
     */
    RK_ERR err = kTaskInit(&senseHandle, SenseTask, RK_NO_ARGS, "Sense",
                             senseStack, STACKSIZE, CTRL_PIPE_PRIO,
                             RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&filterHandle, FilterTask, RK_NO_ARGS, "Filt",
                      filterStack, STACKSIZE, CTRL_PIPE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&ctrlHandle, CtrlTask, RK_NO_ARGS, "Ctrl",
                      ctrlStack, STACKSIZE, CTRL_PIPE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&actHandle, ActTask, RK_NO_ARGS, "Act",
                      actStack, STACKSIZE, CTRL_PIPE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* Bind one receive slot to each stage. */
    err = kSynchMesgInit(senseHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSynchMesgInit(filterHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSynchMesgInit(ctrlHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSynchMesgInit(actHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);

    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID SenseTask(VOID *args)
{
    RK_UNUSEARGS

    /*
     * Sense initializes the first frame. After that, each stage receives a copy
     * into its own stack storage and sends a copy to the next stage.
     */
    ControlFrame frame = {0};
    RK_BOOL firstCycle = RK_TRUE;

    frame.seq = 0U;
    frame.setpointMv = 1200;
    frame.rawMv = 0;
    frame.filteredMv = 0;
    frame.errorMv = 0;
    frame.dutyPermille = 0U;

    logPost("CTRL pipe prio=%u sp=%d", CTRL_PIPE_PRIO,
            frame.setpointMv);

    while (1)
    {
        /* Wait for Act to return the previous frame copy before sampling. */
        if (firstCycle == RK_FALSE)
        {
            CtrlPipeRecv_(&frame);
        }
        firstCycle = RK_FALSE;

        frame.seq++;
        /* Deterministic pseudo-sensor input: enough variation to show control. */
        frame.rawMv = 960 + (INT)((frame.seq * 37U) % 420U);
        logPost("Sense n=%u raw=%d", frame.seq, frame.rawMv);
        CtrlPipeSend_(filterHandle, &frame);
    }
}

VOID FilterTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        ControlFrame frame = {0};
        CtrlPipeRecv_(&frame);
        /* Simple IIR filter: 75% previous filtered value, 25% new sample. */
        if (frame.seq == 1U)
        {
            frame.filteredMv = frame.rawMv;
        }
        else
        {
            frame.filteredMv =
                ((frame.filteredMv * 3) + frame.rawMv) / 4;
        }
        logPost("Filt n=%u avg=%d", frame.seq, frame.filteredMv);
        CtrlPipeSend_(ctrlHandle, &frame);
    }
}

VOID CtrlTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        ControlFrame frame = {0};
        CtrlPipeRecv_(&frame);
        /* Proportional-only control law around a 50% nominal duty cycle. */
        frame.errorMv = frame.setpointMv - frame.filteredMv;
        frame.dutyPermille =
            CtrlClampDuty_(500 + (frame.errorMv / 2));
        logPost("Ctrl n=%u err=%d duty=%u", frame.seq,
                frame.errorMv, frame.dutyPermille);
        CtrlPipeSend_(actHandle, &frame);
    }
}

VOID ActTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        ControlFrame frame = {0};
        CtrlPipeRecv_(&frame);
        logPost("Act n=%u duty=%u", frame.seq, frame.dutyPermille);
        /*
         * The actuator stage closes the timing loop. kSleepPeriodic() is used
         * here because this is a real periodic release; kSleep() would add the
         * stage execution time to every cycle and drift. Until Act returns the
         * frame to Sense, no new sample can enter the pipeline.
         */
        kSleepPeriodic(CTRL_PIPE_PERIOD_TICKS);
        CtrlPipeSend_(senseHandle, &frame);
    }
}

#elif (RK0_APP_EXAMPLE == APP_SYNCH_MESG_HANDOFF)
/*** SYNCHRONOUS MESSAGE HANDOFF ***/
/*
 * Pattern:
 *
 *     high-priority sender -> low-priority owner, while a medium task runs
 *
 * XrSend uses Synchronous Message as "send and wait until copied". There is no
 * reply value. A successful send only means XrOwn copied the payload; after a
 * sender timeout, the kernel invalidates that pending message so the receiver
 * cannot consume a stale request.
 *
 * The priorities make priority inversion visible. While XrSend is blocked in
 * kSynchSendWait(), XrOwn inherits the sender priority long enough to copy
 * the message, so XrMid cannot delay the handoff.
 */

#define LOG_PRIORITY 2U
#define STACKSIZE 192
#define XR_CLIENT_PRIO 1U
#define XR_MEDIUM_PRIO 3U
#define XR_SERVER_PRIO 4U

typedef struct
{
    UINT seq;
    ULONG x;
    ULONG y;
} SynchMesgMsg;

RK_DECLARE_TASK(xrSenderHandle, XrSenderTask, xrSenderStack, STACKSIZE)
RK_DECLARE_TASK(xrOwnerHandle, XrOwnerTask, xrOwnerStack, STACKSIZE)
RK_DECLARE_TASK(xrMediumHandle, XrMediumTask, xrMediumStack, STACKSIZE)

static SynchMesgMsg xrReq;

VOID kApplicationInit(VOID)
{
    /* Create the owner first; the endpoint is bound to this task. */
    RK_ERR err = kTaskInit(&xrOwnerHandle, XrOwnerTask, RK_NO_ARGS,
                             "XrOwn", xrOwnerStack, STACKSIZE,
                             XR_SERVER_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&xrSenderHandle, XrSenderTask, RK_NO_ARGS,
                      "XrSend", xrSenderStack, STACKSIZE,
                      XR_CLIENT_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&xrMediumHandle, XrMediumTask, RK_NO_ARGS,
                      "XrMid", xrMediumStack, STACKSIZE,
                      XR_MEDIUM_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * A Synchronous Message endpoint is task-backed. Senders target the owner
     * task handle, not a buffered queue object. The endpoint fixes the maximum
     * word-aligned payload size; each send provides the actual size copied.
     */
    err = kSynchMesgInit(xrOwnerHandle, sizeof(SynchMesgMsg));
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID XrSenderTask(VOID *args)
{
    RK_UNUSEARGS

    xrReq.seq = 0U;
    logPost("XR demo sender=%u mid=%u owner=%u",
            XR_CLIENT_PRIO, XR_MEDIUM_PRIO, XR_SERVER_PRIO);

    while (1)
    {
        xrReq.seq++;
        xrReq.x = 10UL + xrReq.seq;
        xrReq.y = 100UL + xrReq.seq;

        logPost("XR sendwait s=%u eff=%u owner=%u",
                xrReq.seq, RK_RUNNING_PRIO, RK_TASK_PRIO(xrOwnerHandle));

        /*
         * Send means wait for the receiver-side copy. If this timeout fires,
         * XrOwn must not receive this payload later; the request is no longer
         * valid.
         */
        RK_ERR err = kSynchSendWait(xrOwnerHandle, &xrReq,
                                    sizeof(SynchMesgMsg),
                                    RK_MS_TO_TICKS(300));
        if (err == RK_ERR_SUCCESS)
        {
            logPost("XR sendwait done s=%u owner=%u", xrReq.seq,
                    RK_TASK_PRIO(xrOwnerHandle));
        }
        else
        {
            logError("XR send err %d", err);
        }

        kSleep(RK_MS_TO_TICKS(500));
    }
}

VOID XrOwnerTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        SynchMesgMsg req = {0};
        ULONG mesgBytes = 0UL;
        /*
         * This log normally appears when XrOwn has inherited XrSend's priority.
         * The handoff itself remains one-way: XrOwn receives a copied payload.
         */
        if (RK_RUNNING_PRIO != RK_RUNNING_NOM_PRIO)
        {
            logPost("XR boost eff=%u nom=%u take",
                    RK_RUNNING_PRIO, RK_RUNNING_NOM_PRIO);
        }
        RK_ERR err = kSyncRecv(&req, &mesgBytes, RK_WAIT_FOREVER);
        if (err == RK_ERR_SUCCESS)
        {
            K_ASSERT(mesgBytes == sizeof(SynchMesgMsg));
            logPost("XR recv s=%u eff=%u nom=%u",
                    req.seq, RK_RUNNING_PRIO, RK_RUNNING_NOM_PRIO);
        }
        else
        {
            logError("XR recv err %d", err);
        }
    }
}

VOID XrMediumTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        /*
         * This runnable medium-priority task is the interference source. During
         * a pending high-priority send, priority inheritance should let XrOwn
         * run ahead of XrMid.
         */
        logPost("XR mid eff=%u", RK_RUNNING_PRIO);
        kBusyDelay(RK_MS_TO_TICKS(40));
        kSleep(RK_MS_TO_TICKS(60));
    }
}

#elif (RK0_APP_EXAMPLE == APP_NAMED_COMM_SHOWCASE)
/*** NAMED SYNCHRONOUS COMMUNICATION SHOWCASE ***/
/*
 * Pattern:
 *
 *     client -- send ------------------------> server receive
 *     client -- invoke/block --> server accept
 *     client <-- reply/block --- server reply
 *
 * Synchronous one-way send is the first stage of invocation. Invocation keeps
 * the caller blocked after accept until the server replies. The timeout cases
 * below verify both pre-accept queued timeout and post-accept abandoned call
 * cleanup.
 */

#define STACKSIZE 192
#define NC_CLIENT_PRIO 1U
#define NC_SERVER_PRIO 2U

#define NC_OP_NOTIFY 1U
#define NC_OP_QUEUE_TIMEOUT 2U
#define NC_OP_ADD 3U
#define NC_OP_SLOW 4U
#define NC_OP_MUL 5U

typedef struct
{
    UINT op;
    UINT seq;
    ULONG a;
    ULONG b;
} NamedSyncReq;

typedef struct
{
    UINT seq;
    ULONG value;
    RK_ERR status;
} NamedSyncReply;

RK_DECLARE_TASK(ncClientHandle, NcClientTask, ncClientStack, STACKSIZE)
RK_DECLARE_TASK(ncServerHandle, NcServerTask, ncServerStack, STACKSIZE)

static VOID NcReqSet_(NamedSyncReq *const reqPtr,
                      UINT const op,
                      UINT const seq,
                      ULONG const a,
                      ULONG const b)
{
    reqPtr->op = op;
    reqPtr->seq = seq;
    reqPtr->a = a;
    reqPtr->b = b;
}

static VOID NcReplySet_(NamedSyncReply *const replyPtr,
                        NamedSyncReq const *const reqPtr,
                        ULONG const value)
{
    replyPtr->seq = reqPtr->seq;
    replyPtr->value = value;
    replyPtr->status = RK_ERR_SUCCESS;
}

VOID kApplicationInit(VOID)
{
    /*
     * API setup:
     * 1. Create the client and server tasks.
     * 2. Bind one Synchronous Message endpoint to the server task.
     *
     * Clients target ncServerHandle directly; there is no separate channel or
     * port object in this path.
     */
    RK_ERR err = kTaskInit(&ncClientHandle, NcClientTask, RK_NO_ARGS,
                           "NcCli", ncClientStack, STACKSIZE,
                           NC_CLIENT_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&ncServerHandle, NcServerTask, RK_NO_ARGS,
                    "NcSrv", ncServerStack, STACKSIZE,
                    NC_SERVER_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSynchMesgInit(ncServerHandle, sizeof(NamedSyncReq));
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID NcClientTask(VOID *args)
{
    RK_UNUSEARGS

    NamedSyncReq req = {0U, 0U, 0UL, 0UL};
    NamedSyncReply reply = {0U, 0UL, RK_ERR_ERROR};
    ULONG replyBytes = 0UL;
    /*
     * RK_SYNCH_ATTR describes all client-side buffers for Invocation:
     * - req buffer copied to the server when it accepts.
     * - reply buffer filled when the server replies.
     * - replyBytes tells the caller how many reply bytes were copied.
     *
     * The pointed buffers must stay valid while kSynchMesgCall() is blocked.
     */
    RK_SYNCH_ATTR attr = {&req, sizeof(req), &reply,
                          sizeof(reply), &replyBytes};
    RK_ERR err = RK_ERR_SUCCESS;

    kPuts("NC start\r\n");
    while (1)
    {
        NcReqSet_(&req, NC_OP_NOTIFY, 1U, 7UL, 0UL);
        /*
         * One-way rendezvous: the client blocks only until the server copies
         * req with kSyncRecv(). There is no reply step for this API.
         */
        err = kSynchSendWait(ncServerHandle, &req, sizeof(req),
                             RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        NcReqSet_(&req, NC_OP_QUEUE_TIMEOUT, 2U, 0UL, 0UL);
        /*
         * Queued-call timeout: the server is deliberately sleeping, so the call
         * times out before kSynchMesgAccept() can claim it.
         */
        err = kSynchMesgCall(ncServerHandle, &attr, RK_MS_TO_TICKS(30));
        K_ASSERT(err == RK_ERR_TIMEOUT);

        NcReqSet_(&req, NC_OP_ADD, 3U, 11UL, 31UL);
        replyBytes = 0UL;
        /*
         * Extended rendezvous: accept copies req to the server, then the caller
         * remains blocked until kSynchMesgReply() fills reply.
         */
        err = kSynchMesgCall(ncServerHandle, &attr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(replyBytes == sizeof(reply));
        K_ASSERT((reply.seq == req.seq) && (reply.value == 42UL));

        NcReqSet_(&req, NC_OP_SLOW, 4U, 0UL, 0UL);
        /*
         * Accepted-call timeout: the server accepts this call, then delays long
         * enough that the client gives up while the server still holds the token.
         */
        err = kSynchMesgCall(ncServerHandle, &attr, RK_MS_TO_TICKS(30));
        K_ASSERT(err == RK_ERR_TIMEOUT);

        kSleep(RK_MS_TO_TICKS(160));

        NcReqSet_(&req, NC_OP_MUL, 5U, 6UL, 7UL);
        replyBytes = 0UL;
        /* kSynchMesgInvoke() is the public alias for kSynchMesgCall(). */
        err = kSynchMesgInvoke(ncServerHandle, &attr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(replyBytes == sizeof(reply));
        K_ASSERT((reply.seq == req.seq) && (reply.value == 42UL));

        kPuts("NC PASS\r\n");
    }
}

VOID NcServerTask(VOID *args)
{
    RK_UNUSEARGS

    NamedSyncReq req = {0U, 0U, 0UL, 0UL};
    NamedSyncReply reply = {0U, 0UL, RK_ERR_ERROR};
    RK_SYNCH_CALL_DATA call;
    ULONG reqBytes = 0UL;
    RK_ERR err = RK_ERR_SUCCESS;
    while (1)
    {
        /*
         * kSyncRecv() receives only the first-stage one-way send. It copies the
         * payload into req and releases the blocked sender immediately.
         */
        err = kSyncRecv(&req, &reqBytes, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(reqBytes == sizeof(req));
        K_ASSERT(req.op == NC_OP_NOTIFY);

        kSleep(RK_MS_TO_TICKS(100));

        /*
         * Accept step: copies the next invocation request into req and fills call
         * with the active caller/reply route. Keep call unchanged until reply.
         */
        err = kSynchMesgAccept(&call, &req, &reqBytes, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(reqBytes == sizeof(req));
        K_ASSERT(req.op == NC_OP_ADD);
        NcReplySet_(&reply, &req, req.a + req.b);
        /* Reply step: call identifies which blocked caller receives reply. */
        err = kSynchMesgReply(&call, &reply, sizeof(reply));
        K_ASSERT(err == RK_ERR_SUCCESS);

        /* This accepted call is allowed to timeout before the reply happens. */
        err = kSynchMesgAccept(&call, &req, &reqBytes, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(req.op == NC_OP_SLOW);
        kSleep(RK_MS_TO_TICKS(100));
        /*
         * The server must still finish the accepted-call cleanup. A NULL reply
         * payload is valid here because the caller has already timed out.
         */
        err = kSynchMesgReply(&call, NULL, 0UL);
        K_ASSERT(err == RK_ERR_SUCCESS);

        /* Normal invocation again, this time through kSynchMesgInvoke(). */
        err = kSynchMesgAccept(&call, &req, &reqBytes, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(req.op == NC_OP_MUL);
        NcReplySet_(&reply, &req, req.a * req.b);
        err = kSynchMesgReply(&call, &reply, sizeof(reply));
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

#elif (RK0_APP_EXAMPLE == APP_ASYNCH_DIRECT_MESG)
/*** ASYNCHRONOUS DIRECT MESSAGE SHOWCASE ***/
#if (RK_CONF_ASYNCH_MESG != ON)
#error "APP_ASYNCH_DIRECT_MESG requires RK_CONF_ASYNCH_MESG"
#endif
#if (RK_CONF_MESG_QUEUE != ON)
#error "APP_ASYNCH_DIRECT_MESG requires RK_CONF_MESG_QUEUE"
#endif
/*
 * Pattern:
 *
 *     sender -- alloc/fill/send ----> receiver endpoint
 *     receiver -- wait/free -------> message pool
 *
 * kMesgWait() can take RK_ANY_TASK or a specific sender handle. This example
 * queues a message from A while the receiver is waiting specifically for B,
 * then verifies the queued A message is still delivered by a later ANY wait.
 */

#define STACKSIZE 176
#define AM_RX_PRIO 1U
#define AM_A_PRIO 2U
#define AM_B_PRIO 3U
#define AM_SYNC_PRIO 4U
#define AM_POOL_CEILING_PRIO AM_A_PRIO
#define AM_POOL_DEPTH 4U

#define AM_SRC_A 1U
#define AM_SRC_B 2U

typedef struct
{
    UINT src;
    UINT seq;
    ULONG value;
} AsyncDirectPayload;

RK_DECLARE_TASK(amReceiverHandle, AmReceiverTask, amReceiverStack, STACKSIZE)
RK_DECLARE_TASK(amSenderAHandle, AmSenderATask, amSenderAStack, STACKSIZE)
RK_DECLARE_TASK(amSenderBHandle, AmSenderBTask, amSenderBStack, STACKSIZE)
RK_DECLARE_TASK(amSyncOnlyHandle, AmSyncOnlyTask, amSyncOnlyStack, STACKSIZE)
RK_DECLARE_MESG_POOL(amPool, amPoolBuf, AsyncDirectPayload, AM_POOL_DEPTH)

static RK_MESG *AmMesgMake_(UINT const src,
                            UINT const seq,
                            ULONG const value)
{
    /*
     * Allocation step: kMesgAlloc() waits for an RK_MESG block from the pool.
     * Until kMesgSend() succeeds, the sender owns it.
     */
    RK_MESG *mesgPtr = NULL;
    RK_ERR err = kMesgAlloc(&amPool, &mesgPtr, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    K_ASSERT(mesgPtr != NULL);
    K_ASSERT(kMesgPayloadBytes(mesgPtr) >= sizeof(AsyncDirectPayload));

    /*
     * Payload step: RK_MESG_PAYLOAD() converts the generic RK_MESG buffer into
     * the application payload type declared by RK_DECLARE_MESG_POOL().
     */
    AsyncDirectPayload *const payloadPtr =
        RK_MESG_PAYLOAD(mesgPtr, AsyncDirectPayload);
    K_ASSERT(payloadPtr != NULL);
    payloadPtr->src = src;
    payloadPtr->seq = seq;
    payloadPtr->value = value;

    return (mesgPtr);
}

static VOID AmAssertSenderPid_(RK_MESG const *const mesgPtr,
                               RK_TASK_HANDLE const senderHandle)
{
    /* kMesgSend() records sender metadata; the receiver can verify it later. */
    RK_PID senderID = 0U;
    RK_ERR err = kMesgGetSenderID(mesgPtr, &senderID);
    K_ASSERT(err == RK_ERR_SUCCESS);
    K_ASSERT(senderID == kTaskGetPID(senderHandle));
}

VOID kApplicationInit(VOID)
{
    /*
     * API setup:
     * 1. Create sender and receiver tasks.
     * 2. Initialize the message pool that owns RK_MESG buffers.
     * 3. Initialize the receiver's direct-message endpoint.
     * 4. Verify the endpoint policy: a task is async or sync, not both.
     */
    RK_ERR err = kTaskInit(&amReceiverHandle, AmReceiverTask, RK_NO_ARGS,
                           "AmRx", amReceiverStack, STACKSIZE,
                           AM_RX_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&amSenderAHandle, AmSenderATask, RK_NO_ARGS,
                    "AmA", amSenderAStack, STACKSIZE,
                    AM_A_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&amSenderBHandle, AmSenderBTask, RK_NO_ARGS,
                    "AmB", amSenderBStack, STACKSIZE,
                    AM_B_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&amSyncOnlyHandle, AmSyncOnlyTask, RK_NO_ARGS,
                    "AmSync", amSyncOnlyStack, STACKSIZE,
                    AM_SYNC_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * Pool setup: AM_POOL_DEPTH fixed-size messages plus a priority ceiling.
     * While a task owns an amPool message, it runs at least at the highest
     * priority of tasks that may block waiting for this pool.
     */
    err = kMesgPoolInit(&amPool, amPoolBuf, sizeof(AsyncDirectPayload),
                        AM_POOL_DEPTH, AM_POOL_CEILING_PRIO);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* The async endpoint belongs to the receiver task handle. */
    err = kMesgEndpointInit(amReceiverHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);

#if (RK_CONF_SYNCH_MESG == ON)
    /* The same task cannot own both async and synchronous message endpoints. */
    err = kSynchMesgInit(amReceiverHandle, sizeof(AsyncDirectPayload));
    K_ASSERT(err == RK_ERR_HAS_OWNER);

    err = kSynchMesgInit(amSyncOnlyHandle, sizeof(AsyncDirectPayload));
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kMesgEndpointInit(amSyncOnlyHandle);
    K_ASSERT(err == RK_ERR_HAS_OWNER);
#endif
}

VOID AmReceiverTask(VOID *args)
{
    RK_UNUSEARGS

    RK_MESG *mesgPtr = NULL;
    UINT payloadExpected = 0U;

    kPuts("AM start\r\n");
    while (1)
    {
            AsyncDirectPayload *payloadPtr = NULL;

        /* Empty nonblocking wait returns immediately and transfers no ownership. */
        RK_ERR err = kMesgWait(RK_ANY_TASK, &mesgPtr, RK_NO_WAIT);
        K_ASSERT(err == RK_ERR_BUFFER_EMPTY);
        payloadExpected += 1u;
        /*
         * ANY wait: when a message arrives, ownership of mesgPtr moves from the
         * sender/endpoint queue to this receiver task.
         */
        err = kMesgWait(RK_ANY_TASK, &mesgPtr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(kMesgGetSenderHandle(mesgPtr) == amSenderAHandle);
        AmAssertSenderPid_(mesgPtr, amSenderAHandle);
        payloadPtr = RK_MESG_PAYLOAD(mesgPtr, AsyncDirectPayload);
        K_ASSERT((payloadPtr->src == AM_SRC_A) && (payloadPtr->seq == payloadExpected));
        K_ASSERT(payloadPtr->value == 11UL);
           printf("AM recv %lu, seq: %u, from: %u \r\n", payloadPtr->value, payloadPtr->seq, payloadPtr->src);
        /* Return the consumed buffer to the pool exactly once. */
        err = kMesgFree(mesgPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        mesgPtr = NULL;

        /*
         * Sender-filtered wait: messages from other senders may stay queued while
         * this call waits for amSenderBHandle.
         */
        err = kMesgWait(amSenderBHandle, &mesgPtr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(kMesgGetSenderHandle(mesgPtr) == amSenderBHandle);
        AmAssertSenderPid_(mesgPtr, amSenderBHandle);
        payloadPtr = RK_MESG_PAYLOAD(mesgPtr, AsyncDirectPayload);
        K_ASSERT((payloadPtr->src == AM_SRC_B) && (payloadPtr->seq % 2U == 0));
        K_ASSERT(payloadPtr->value == 22UL);
           printf("AM recv %lu, seq: %u, from: %u \r\n", payloadPtr->value, payloadPtr->seq, payloadPtr->src);
        err = kMesgFree(mesgPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        mesgPtr = NULL;

        err = kMesgWait(RK_ANY_TASK, &mesgPtr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(kMesgGetSenderHandle(mesgPtr) == amSenderAHandle);
        AmAssertSenderPid_(mesgPtr, amSenderAHandle);
        payloadPtr = RK_MESG_PAYLOAD(mesgPtr, AsyncDirectPayload);
        K_ASSERT((payloadPtr->src == AM_SRC_A) && (payloadPtr->seq % 3U == 0));
        K_ASSERT(payloadPtr->value == 33UL);
        /* Free after the delayed ANY wait proves sender filtering preserved A. */
        err = kMesgFree(mesgPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);

        /* Bounded filtered wait returns timeout when no matching sender appears. */
        err = kMesgWait(amSenderBHandle, &mesgPtr, RK_MS_TO_TICKS(30));
        K_ASSERT(err == RK_ERR_TIMEOUT);

        printf("AM recv %lu, seq: %u, from: %u \r\n", payloadPtr->value, payloadPtr->seq, payloadPtr->src);
    }
}

VOID AmSenderATask(VOID *args)
{
    RK_UNUSEARGS

    RK_MESG *mesgPtr = NULL;
     UINT seq1 = 0;
    UINT seq3 = 0;

    while (1)
    {
        seq1+=1; seq3+=3;
        kSleep(RK_MS_TO_TICKS(10));
        /* Allocate and fill while the sender still owns the buffer. */
        mesgPtr = AmMesgMake_(AM_SRC_A, seq1, 11UL);
        /* Successful send transfers ownership to the receiver endpoint queue. */
       RK_ERR err = kMesgSend(amReceiverHandle, mesgPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);

        kSleep(RK_MS_TO_TICKS(20));
        /* This second A message is queued while Rx waits specifically for B. */
        mesgPtr = AmMesgMake_(AM_SRC_A, seq3, 33UL);
        err = kMesgSend(amReceiverHandle, mesgPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

VOID AmSenderBTask(VOID *args)
{
    RK_UNUSEARGS

    RK_MESG *mesgPtr = NULL;
    UINT seq2 = 0;
    while (1)
    {
        seq2 += 2U;
        kSleep(RK_MS_TO_TICKS(50));
        /* B satisfies the receiver's sender-filtered wait. */
        mesgPtr = AmMesgMake_(AM_SRC_B, seq2, 22UL);
        RK_ERR   err = kMesgSend(amReceiverHandle, mesgPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

VOID AmSyncOnlyTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

#elif (RK0_APP_EXAMPLE == APP_ASYNCH_DIRECT_MESG2)
#define STACKSIZE 256U

typedef struct
{
    UINT opcode;
    ULONG value;
} Request_t;

typedef struct
{
    RK_ERR status;
    ULONG result;
} Reply_t;

RK_DECLARE_TASK(clientHandle, ClientTask, clientStack, STACKSIZE)
RK_DECLARE_TASK(serverHandle, ServerTask, serverStack, STACKSIZE)

VOID kApplicationInit(VOID)
{
    /*
     * API setup:
     * 1. Create the server and client tasks.
     * 2. Initialize the server task as a Synchronous Message endpoint for
     *    Request_t-sized requests.
     */
    RK_ERR err = kTaskInit(&serverHandle, ServerTask, RK_NO_ARGS,
                           "Srv", serverStack, STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&clientHandle, ClientTask, RK_NO_ARGS,
                    "Cli", clientStack, STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* The endpoint is attached to serverHandle; clients call that task handle. */
    err = kSynchMesgInit(serverHandle, sizeof(Request_t));
    K_ASSERT(err == RK_ERR_SUCCESS);
    kPuts("Synch invocation example start\r\n");
}

VOID ClientTask(VOID *args)
{
    RK_UNUSEARGS

    ULONG value = 42UL;
    while (1)
    {
        Request_t req = {1U, value++};
        Reply_t reply = {RK_ERR_SUCCESS, 0UL};
        ULONG replyBytes = 0UL;
        /*
         * Client step:
         * - req is copied into the server when it accepts the call.
         * - reply is where kSynchMesgReply() copies the server answer.
         * - replyBytes is written by the kernel with the actual reply length.
         *
         * These stack objects stay valid because kSynchMesgCall() blocks until
         * the server replies.
         */
        RK_SYNCH_ATTR attr = {&req, sizeof(req), &reply,
                              sizeof(reply), &replyBytes};

        /* Call blocks after accept; it returns only after reply or timeout. */
        RK_ERR err = kSynchMesgCall(serverHandle, &attr, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(replyBytes == sizeof(reply));
        K_ASSERT(reply.status == RK_ERR_SUCCESS);
        K_ASSERT(reply.result == (req.value + 1UL));

        printf("client: value=%lu result=%lu\r\n", req.value, reply.result);
        kBusyDelay(2);
    }
}

VOID ServerTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_SYNCH_CALL_DATA acceptedCall = {0};
        Request_t req = {0U, 0UL};
        ULONG reqBytes = 0UL;

        /*
         * Accept step:
         * - req receives the copied Request_t.
         * - acceptedCall becomes the reply token for this invocation.
         *
         * The server must pass the same acceptedCall to kSynchMesgReply().
         */
        RK_ERR err = kSynchMesgAccept(&acceptedCall, &req, &reqBytes,
                                      RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(reqBytes == sizeof(req));

        Reply_t reply = {RK_ERR_SUCCESS, req.value + 1UL};
        printf("server: opcode=%u value=%lu result=%lu\r\n",
               req.opcode, req.value, reply.result);
        kBusyDelay(2);
        /* Reply step: copy Reply_t to the blocked caller and ready that caller. */
        err = kSynchMesgReply(&acceptedCall, &reply, sizeof(reply));
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}
#elif (RK0_APP_EXAMPLE == APP_TRACE_EXERCISE)

/*** TRACE EXERCISE APPLICATION ***/
/*
 * Pattern:
 *
 *     TrTx produces activity across kernel objects; TrRx consumes it.
 *
 * This is not a recommended application design. It intentionally touches a
 * semaphore, mutex, Sleep Queue, message queue, timer, memory partition,
 * MRM, and Synchronous Message so trace "list", "top", and "hist" commands have
 * interesting data to display.
 *
 * The Synchronous Message send is deliberately outside the mutex ownership
 * window. RK0 rejects blocking Synchronous Message operations while the caller
 * owns a mutex.
 */

#define LOG_PRIORITY 5
#define STACKSIZE 192
#define TRACE_Q_DEPTH 2U
#define TRACE_MRM_BUFS 2U
#define TRACE_MRM_WORDS 2U

typedef struct
{
    UINT seq;
    ULONG value;
} TraceMsg;

RK_DECLARE_TASK(traceTxHandle, TraceTxTask, traceTxStack, STACKSIZE)
RK_DECLARE_TASK(traceRxHandle, TraceRxTask, traceRxStack, STACKSIZE)
RK_DECLARE_TASK(traceWaitHandle, TraceWaitTask, traceWaitStack, STACKSIZE)

static RK_SEMAPHORE traceSema;
static RK_MUTEX traceMutex;
static RK_SLEEP_QUEUE traceCondq;
static RK_MESG_QUEUE traceQ;
#if (RK_CONF_CALLOUT_TIMER == ON)
static RK_TIMER traceTimer;
#endif
static RK_MEM_PARTITION traceMem;
static RK_MRM traceMrm;

RK_DECLARE_MESG_QUEUE_BUF(traceQBuf, TraceMsg, TRACE_Q_DEPTH)
RK_DECLARE_MEM_POOL(TraceMsg, traceMemPool, 2U)
static RK_MRM_BUF traceMrmPool[TRACE_MRM_BUFS] K_ALIGN(4);
static ULONG traceMrmData[TRACE_MRM_BUFS][TRACE_MRM_WORDS] K_ALIGN(4);

static volatile UINT traceTimerTicks;

#if (RK_CONF_CALLOUT_TIMER == ON)
static VOID TraceTimerCbk(VOID *args)
{
    RK_UNUSEARGS

    traceTimerTicks++;
    /* Timer callbacks should only do bounded work; wake TrRx through a sema. */
    kSemaphorePost(&traceSema);
}
#endif

static VOID TraceNameObjects(VOID)
{
    /*
     * Trace names are optional metadata. They do not change object behavior, but
     * make trace output readable when several kernel objects are active.
     */
    RK_ERR err = kTraceNameObject(&traceSema, "TrSema");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceMutex, "TrMutex");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceCondq, "TrCond");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceQ, "TrQueue");
    K_ASSERT(err == RK_ERR_SUCCESS);
#if (RK_CONF_CALLOUT_TIMER == ON)
    err = kTraceNameObject(&traceTimer, "TrTimer");
    K_ASSERT(err == RK_ERR_SUCCESS);
#endif
    err = kTraceNameObject(&traceMem, "TrMem");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceMrm, "TrMrm");
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID kApplicationInit(VOID)
{
    TraceMsg bootMsg = {.seq = 0U, .value = 0x1000UL};
    RK_MRM_BUF *bootBufPtr = NULL;
    RK_ERR err = RK_ERR_SUCCESS;

    /*
     * API setup:
     * 1. Create producer, consumer, and wait-demo tasks.
     * 2. Bind a Synchronous Message endpoint to TrRx.
     * 3. Initialize one object from each traced object class.
     * 4. Name the objects, then start logger and trace.
     */
    err = kTaskInit(&traceRxHandle, TraceRxTask, RK_NO_ARGS, "TrRx",
                      traceRxStack, STACKSIZE, 1U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTaskInit(&traceTxHandle, TraceTxTask, RK_NO_ARGS, "TrTx",
                      traceTxStack, STACKSIZE, 2U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTaskInit(&traceWaitHandle, TraceWaitTask, RK_NO_ARGS, "TrWait",
                      traceWaitStack, STACKSIZE, 3U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSynchMesgInit(traceRxHandle, sizeof(TraceMsg));
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* Initialize a small set of objects so each trace object class is visible. */
    err = kSemaphoreInit(&traceSema, 0U, 3U);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMutexInit(&traceMutex, RK_PRIO_INHERITANCE);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSleepQueueInit(&traceCondq);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMesgQueueInit(&traceQ, traceQBuf, RK_MESGQ_MESG_SIZE(TraceMsg),
                         TRACE_Q_DEPTH);
    K_ASSERT(err == RK_ERR_SUCCESS);
#if (RK_CONF_CALLOUT_TIMER == ON)
    err = kTimerInit(&traceTimer, 0, RK_MS_TO_TICKS(250),
                     TraceTimerCbk, RK_NO_ARGS, RK_OPT_TIMER_RELOAD);
    K_ASSERT(err == RK_ERR_SUCCESS);
#endif
    err = kMemPartitionInit(&traceMem, traceMemPool, sizeof(TraceMsg), 2U);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMRMInit(&traceMrm, traceMrmPool, traceMrmData, TRACE_MRM_BUFS,
                   TRACE_MRM_WORDS);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * Seed the MRM once so TrRx can show both the initial publish and later
     * updates in the trace history.
     */
    bootBufPtr = kMRMReserve(&traceMrm);
    K_ASSERT(bootBufPtr != NULL);
    bootBufPtr->nUsers = 0U;
    err = kMRMPublish(&traceMrm, bootBufPtr, &bootMsg);
    K_ASSERT(err == RK_ERR_SUCCESS);

    TraceNameObjects();

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID TraceTxTask(VOID *args)
{
    RK_UNUSEARGS

    UINT seq = 0U;
    while (1)
    {
        TraceMsg msg;
        TraceMsg *memMsgPtr = NULL;
        RK_MRM_BUF *mrmBufPtr = NULL;

        seq++;
        msg.seq = seq;
        msg.value = 0xA500UL + seq;

        /*
         * This short mutex section exists so TrWait can contend on TrMutex and
         * trace can show mutex ownership. Do not call Synchronous Message
         * while holding it.
         */
        kMutexLock(&traceMutex, RK_WAIT_FOREVER);
        memMsgPtr = (TraceMsg *)kMemPartitionAlloc(&traceMem);
        if (memMsgPtr != NULL)
        {
            *memMsgPtr = msg;
        }
        kMutexUnlock(&traceMutex);

        /*
         * The queue has depth two. The third send can block briefly, which gives
         * trace history a queue-wait case without relying on external input.
         */
        kMesgQueueSend(&traceQ, &msg, RK_MS_TO_TICKS(80));
        kMesgQueueSend(&traceQ, &msg, RK_MS_TO_TICKS(80));
        kMesgQueueSend(&traceQ, &msg, RK_MS_TO_TICKS(40));
        /* Wake consumers through separate semaphore and Sleep Queue paths. */
        kSemaphorePost(&traceSema);
        kSleepQueueWake(&traceCondq, 1U, NULL);

        /* MRM publishes the latest full message; readers borrow then unget it. */
        mrmBufPtr = kMRMReserve(&traceMrm);
        if (mrmBufPtr != NULL)
        {
            mrmBufPtr->nUsers = 0U;
            kMRMPublish(&traceMrm, mrmBufPtr, &msg);
        }

        /* One-way synchronous handoff to TrRx after the shared objects update. */
        kSynchSendWait(traceRxHandle, &msg, sizeof(msg), RK_WAIT_FOREVER);

        if (memMsgPtr != NULL)
        {
            kMutexLock(&traceMutex, RK_WAIT_FOREVER);
            kMemPartitionFree(&traceMem, memMsgPtr);
            kMutexUnlock(&traceMutex);
        }

        logPost("TraceTx seq=%u timer=%u", seq, traceTimerTicks);
        kSleep(RK_MS_TO_TICKS(300));
    }
}

VOID TraceRxTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        TraceMsg msg = {0U, 0UL};
        TraceMsg syncMsg = {0U, 0UL};
        TraceMsg mrmMsg = {0U, 0UL};
        ULONG syncBytes = 0UL;

        /* Queue receive consumes one buffered TraceMsg if a producer posted one. */
        kMesgQueueRecv(&traceQ, &msg, RK_MS_TO_TICKS(120));
        /* Semaphore pend consumes one wakeup token from timer or TraceTx. */
        kSemaphorePend(&traceSema, RK_MS_TO_TICKS(70));

        /* MRM get copies the newest published value and returns a borrowed slot. */
        RK_MRM_BUF *mrmBufPtr = kMRMGet(&traceMrm, &mrmMsg);
        if (mrmBufPtr != NULL)
        {
            /* Unget releases the borrowed MRM slot for future publish cycles. */
            kMRMUnget(&traceMrm, mrmBufPtr);
        }

        /* kSyncRecv() accepts one pending synchronous send from TraceTx. */
        if (kSyncRecv(&syncMsg, &syncBytes, RK_MS_TO_TICKS(80)) !=
            RK_ERR_SUCCESS)
        {
            syncBytes = 0UL;
        }

        logPost("TraceRx qseq=%u mrm=%u smsg=%u bytes=%lu", msg.seq,
                mrmMsg.seq, syncMsg.seq, syncBytes);
        kSleep(RK_MS_TO_TICKS(90));
    }
}

VOID TraceWaitTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSleepQueueSleep(&traceCondq, RK_MS_TO_TICKS(220));
        if (kMutexLock(&traceMutex, RK_MS_TO_TICKS(60)) == RK_ERR_SUCCESS)
        {
            /* Hold the mutex briefly so priority inheritance is traceable. */
            kBusyDelay(RK_MS_TO_TICKS(20));
            kMutexUnlock(&traceMutex);
        }
        kSleep(RK_MS_TO_TICKS(180));
    }
}

#elif (RK0_APP_EXAMPLE == APP_BARRIER_SHARED)

/*** SYNCH BARRIER USING MONITORS ***/
/*
 * Pattern:
 *
 *     workers share one monitor: mutex + Sleep Queue
 *
 * This is the direct shared-state version of the barrier. The Barrier_t monitor
 * is the single authority, and all access to count/round happens under BarLock.
 */
#define LOG_BARRIER_ENTER(c, t, name)                                          \
    logPost("[BARRIER: %u/%u]: %s ENTERED  ", (c), (t), (name))
#define LOG_BARRIER_BLOCK(c, t, name)                                          \
    logPost("[BARRIER: %u/%u]: %s WAITING  ", (c), (t), (name))
#define LOG_BARRIER_WAKE(c, t, name)                                           \
    logPost("[BARRIER: %u/%u]: %s WAKING ALL TASKS ", (c), (t), (name))


#define STACKSIZE 256

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)

/* Synchronisation Barrier */

typedef struct
{
    RK_MUTEX lock;
    RK_SLEEP_QUEUE cond;
    UINT count; /* number of tasks currently in this barrier round */
    UINT round; /* incremented each time all tasks synchronize */
} Barrier_t;

VOID BarrierInit(Barrier_t *const barPtr)
{
    /* kCondVarInit() binds the Sleep Queue wait path to this monitor mutex. */
    kCondVarInit(&barPtr->cond, &barPtr->lock);
    barPtr->count = 0;
    barPtr->round = 0;
}

VOID BarrierWait(Barrier_t *const barPtr, UINT const nTasks, RK_TICK timeout)
{
    UINT myRound = 0;
    /* All barrier state is protected by the monitor lock. */
    kMutexLock(&barPtr->lock, RK_WAIT_FOREVER);

    /*
     * The saved round distinguishes an old wake from completion of the current
     * barrier round.
     */
    myRound = barPtr->round;
    barPtr->count++;
    LOG_BARRIER_ENTER(barPtr->count, nTasks, RK_RUNNING_NAME);

    if (barPtr->count == nTasks)
    {
        LOG_BARRIER_WAKE(barPtr->count, nTasks, RK_RUNNING_NAME);

        /* Last arrival opens the barrier and wakes every waiter in this round. */
        barPtr->round++;
        barPtr->count = 0;
        kCondVarBroadcast(&barPtr->cond);
    }
    else
    {
        LOG_BARRIER_BLOCK(barPtr->count, nTasks, RK_RUNNING_NAME);
        /*
         * Wait in a loop because a timeout or unrelated wake does not mean this
         * round has completed.
         */
        while ((UINT)(barPtr->round - myRound) == 0U)
        {
            /*
             * kCondVarWait() releases the mutex while blocked and reacquires it
             * before returning, so the loop can safely re-check round.
             */
            RK_ERR err = kCondVarWait(&barPtr->cond, &barPtr->lock, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                logError("Wait failed with error code %d", err);
            }
        }
    }
    kMutexUnlock(&barPtr->lock);
}

#define N_BARR_TASKS 3

Barrier_t syncBarrier;

/* Note expressions within K_ASSERT brackets are supressed if compiling
with NDEBUG */
VOID kApplicationInit(VOID)
{
    /* All worker tasks share the same Barrier_t monitor. */
    RK_ERR err = kTaskInit(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1,
                             STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2,
                      STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3,
                      STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    BarrierInit(&syncBarrier);
    err = kTraceNameObject(&syncBarrier.lock, "BarLock");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&syncBarrier.cond, "BarCond");
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(4);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 running");
        kBusyDelay(RK_MS_TO_TICKS(100)); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS, RK_MS_TO_TICKS(600));
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 running");
        kBusyDelay(RK_MS_TO_TICKS(200)); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS, RK_MS_TO_TICKS(400));
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 running");
        kBusyDelay(RK_MS_TO_TICKS(300)); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS, RK_MS_TO_TICKS(100));
        kSleep(RK_MS_TO_TICKS(50)); /* let logger runs */
    }
}

#else
#error "Invalid RK0_APP_EXAMPLE selection"
#endif
