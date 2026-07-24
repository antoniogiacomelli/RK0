/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/*
 * This file is intentionally written as executable documentation. Select one
 * example with RK0_APP_EXAMPLE, build it, and watch the logger/trace output to
 * see how the primitive behaves under the scheduler.
 */

#define APP_BARRIER_PORTS 0
#define APP_BARRIER_SHARED 1
#define APP_TRACE_EXERCISE 2
#define APP_RENDEZVOUS_HANDOFF 3
#define APP_RENDEZVOUS_CONTROLLER 4

/*
 * Default: a same-priority controller pipeline using RK_RENDEZVOUS.
 *
 * Useful trace observations:
 * - top: Sense/Filt/Ctrl/Act have the same priority and their RUNS advance.
 * - top: PCHG stays at 0 for these tasks; this is cooperation, not PI.
 * - hist SensIn/FiltIn/CtrlIn/ActIn: each frame handoff is unbuffered.
 */
#define RK0_APP_EXAMPLE APP_RENDEZVOUS_CONTROLLER

#include <kapi.h>
/* Configure the application logger facility here */
#include <logger.h>
#include <qemu_uart.h>
#include <stdio.h>
int main(void)
{

    kCoreInit();

    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

#define LOG_BARRIER_ENTER(c, t, name)                                          \
    logPost("[BARRIER: %u/%u]: %s ENTERED  ", (c), (t), (name))
#define LOG_BARRIER_BLOCK(c, t, name)                                          \
    logPost("[BARRIER: %u/%u]: %s WAITING  ", (c), (t), (name))
#define LOG_BARRIER_WAKE(c, t, name)                                           \
    logPost("[BARRIER: %u/%u]: %s WAKING ALL TASKS ", (c), (t), (name))

#if (RK0_APP_EXAMPLE == APP_RENDEZVOUS_CONTROLLER)
/*** SAME-PRIORITY RENDEZVOUS CONTROLLER PIPELINE ***/
/*
 * Pattern:
 *
 *     Sense -> Filt -> Ctrl -> Act -> Sense
 *
 * The application models a small sampled controller. Sense produces a raw
 * millivolt sample, Filt smooths it, Ctrl computes a duty-cycle command, and
 * Act applies the command. All four tasks run at CTRL_PIPE_PRIO.
 *
 * There is one ControlFrame. Ownership of that frame moves through the pipeline
 * by RK_RENDEZVOUS. A send completes only when the next stage has received the
 * pointer, so a fast producer cannot outrun a slow consumer. This is the core
 * lesson: Rendezvous is unbuffered message passing, not a queue and not a
 * request/reply Channel.
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

/* Each pipeline stage owns one Rendezvous receive endpoint. */
RK_DECLARE_TASK(senseHandle, SenseTask, senseStack, STACKSIZE)
RK_DECLARE_TASK(filterHandle, FilterTask, filterStack, STACKSIZE)
RK_DECLARE_TASK(ctrlHandle, CtrlTask, ctrlStack, STACKSIZE)
RK_DECLARE_TASK(actHandle, ActTask, actStack, STACKSIZE)

static RK_RENDEZVOUS senseRendezvous;
static RK_RENDEZVOUS filterRendezvous;
static RK_RENDEZVOUS ctrlRendezvous;
static RK_RENDEZVOUS actRendezvous;
static ControlFrame controlFrame;

/* Receive the frame that is now owned by the running stage. */
static ControlFrame *CtrlPipeRecv_(VOID)
{
    VOID *framePtr = NULL;
    RK_ERR err = kRendezvousRecv(&framePtr, RK_WAIT_FOREVER);
    if ((err != RK_ERR_SUCCESS) || (framePtr == NULL))
    {
        logError("%s recv err %d", RK_RUNNING_NAME, err);
    }
    return ((ControlFrame *)framePtr);
}

/* Hand the frame to the next stage and wait until that stage receives it. */
static VOID CtrlPipeSend_(RK_TASK_HANDLE const receiverHandle,
                          ControlFrame *const framePtr)
{
    RK_ERR err = kRendezvousSend(receiverHandle, framePtr, RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
    {
        logError("%s send err %d", RK_RUNNING_NAME, err);
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
     * sequencing through Rendezvous, so priority inheritance should not be part
     * of the normal trace output for these stages.
     */
    RK_ERR err = kCreateTask(&senseHandle, SenseTask, RK_NO_ARGS, "Sense",
                             senseStack, STACKSIZE, CTRL_PIPE_PRIO,
                             RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&filterHandle, FilterTask, RK_NO_ARGS, "Filt",
                      filterStack, STACKSIZE, CTRL_PIPE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&ctrlHandle, CtrlTask, RK_NO_ARGS, "Ctrl",
                      ctrlStack, STACKSIZE, CTRL_PIPE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&actHandle, ActTask, RK_NO_ARGS, "Act",
                      actStack, STACKSIZE, CTRL_PIPE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* Bind one named receive slot to each stage. */
    err = kRendezvousInit(&senseRendezvous, senseHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kRendezvousInit(&filterRendezvous, filterHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kRendezvousInit(&ctrlRendezvous, ctrlHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kRendezvousInit(&actRendezvous, actHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* Short names keep trace list/hist output readable on UART. */
    err = kTraceNameObject(&senseRendezvous, "SensIn");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&filterRendezvous, "FiltIn");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&ctrlRendezvous, "CtrlIn");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&actRendezvous, "ActIn");
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID SenseTask(VOID *args)
{
    RK_UNUSEARGS

    /*
     * Sense owns the static frame only for bootstrapping. After the first send,
     * the frame is owned by exactly one pipeline stage at a time and comes back
     * to Sense from Act through senseRendezvous.
     */
    ControlFrame *framePtr = &controlFrame;
    RK_BOOL firstCycle = RK_TRUE;

    framePtr->seq = 0U;
    framePtr->setpointMv = 1200;
    framePtr->rawMv = 0;
    framePtr->filteredMv = 0;
    framePtr->errorMv = 0;
    framePtr->dutyPermille = 0U;

    logPost("CTRL pipe prio=%u sp=%d", CTRL_PIPE_PRIO,
            framePtr->setpointMv);

    while (1)
    {
        /* Wait for Act to return the previous frame before sampling again. */
        if (firstCycle == RK_FALSE)
        {
            framePtr = CtrlPipeRecv_();
        }
        firstCycle = RK_FALSE;

        framePtr->seq++;
        /* Deterministic pseudo-sensor input: enough variation to show control. */
        framePtr->rawMv =
            960 + (INT)((framePtr->seq * 37U) % 420U);
        logPost("Sense n=%u raw=%d", framePtr->seq, framePtr->rawMv);
        CtrlPipeSend_(filterHandle, framePtr);
    }
}

VOID FilterTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        ControlFrame *const framePtr = CtrlPipeRecv_();
        /* Simple IIR filter: 75% previous filtered value, 25% new sample. */
        if (framePtr->seq == 1U)
        {
            framePtr->filteredMv = framePtr->rawMv;
        }
        else
        {
            framePtr->filteredMv =
                ((framePtr->filteredMv * 3) + framePtr->rawMv) / 4;
        }
        logPost("Filt n=%u avg=%d", framePtr->seq, framePtr->filteredMv);
        CtrlPipeSend_(ctrlHandle, framePtr);
    }
}

VOID CtrlTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        ControlFrame *const framePtr = CtrlPipeRecv_();
        /* Proportional-only control law around a 50% nominal duty cycle. */
        framePtr->errorMv = framePtr->setpointMv - framePtr->filteredMv;
        framePtr->dutyPermille =
            CtrlClampDuty_(500 + (framePtr->errorMv / 2));
        logPost("Ctrl n=%u err=%d duty=%u", framePtr->seq,
                framePtr->errorMv, framePtr->dutyPermille);
        CtrlPipeSend_(actHandle, framePtr);
    }
}

VOID ActTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        ControlFrame *const framePtr = CtrlPipeRecv_();
        logPost("Act n=%u duty=%u", framePtr->seq,
                framePtr->dutyPermille);
        /*
         * The actuator stage closes the timing loop. kSleepPeriodic() is used
         * here because this is a real periodic release; kSleep() would add the
         * stage execution time to every cycle and drift. Until Act returns the
         * frame to Sense, no new sample can enter the pipeline.
         */
        kSleepPeriodic(CTRL_PIPE_PERIOD_TICKS);
        CtrlPipeSend_(senseHandle, framePtr);
    }
}

#elif (RK0_APP_EXAMPLE == APP_RENDEZVOUS_HANDOFF)
/*** SYNCHRONOUS RENDEZVOUS HANDOFF ***/
/*
 * Pattern:
 *
 *     high-priority sender -> low-priority owner, while a medium task runs
 *
 * XrSend uses Rendezvous as "send and wait until received". There is no reply
 * value. A successful send only means XrOwn took the pointer; after a sender
 * timeout, the kernel invalidates that pending message so the receiver cannot
 * consume a stale request.
 *
 * The priorities make priority inversion visible. While XrSend is blocked in
 * kRendezvousSend(), XrOwn inherits the sender priority long enough to receive
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
} RendezvousMsg;

RK_DECLARE_TASK(xrSenderHandle, XrSenderTask, xrSenderStack, STACKSIZE)
RK_DECLARE_TASK(xrOwnerHandle, XrOwnerTask, xrOwnerStack, STACKSIZE)
RK_DECLARE_TASK(xrMediumHandle, XrMediumTask, xrMediumStack, STACKSIZE)

static RK_RENDEZVOUS xrRendezvous;
static RendezvousMsg xrReq;

VOID kApplicationInit(VOID)
{
    /* Create the owner first; the Rendezvous endpoint is bound to this task. */
    RK_ERR err = kCreateTask(&xrOwnerHandle, XrOwnerTask, RK_NO_ARGS,
                             "XrOwn", xrOwnerStack, STACKSIZE,
                             XR_SERVER_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&xrSenderHandle, XrSenderTask, RK_NO_ARGS,
                      "XrSend", xrSenderStack, STACKSIZE,
                      XR_CLIENT_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&xrMediumHandle, XrMediumTask, RK_NO_ARGS,
                      "XrMid", xrMediumStack, STACKSIZE,
                      XR_MEDIUM_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * A Rendezvous endpoint has one receiving owner. Senders target the owner
     * task handle, not a buffered queue object.
     */
    err = kRendezvousInit(&xrRendezvous, xrOwnerHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&xrRendezvous, "XrSync");
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
         * Send means wait for receipt. If this timeout fires, XrOwn must not
         * receive this pointer later; the request is no longer valid.
         */
        RK_ERR err = kRendezvousSend(xrOwnerHandle, &xrReq,
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
        VOID *reqPtr = NULL;
        /*
         * This log normally appears when XrOwn has inherited XrSend's priority.
         * The handoff itself remains one-way: XrOwn receives the pointer only.
         */
        if (RK_RUNNING_PRIO != RK_RUNNING_NOM_PRIO)
        {
            logPost("XR boost eff=%u nom=%u take",
                    RK_RUNNING_PRIO, RK_RUNNING_NOM_PRIO);
        }
        RK_ERR err = kRendezvousRecv(&reqPtr, RK_WAIT_FOREVER);
        if ((err == RK_ERR_SUCCESS) && (reqPtr != NULL))
        {
            RendezvousMsg const *req = (RendezvousMsg const *)reqPtr;
            logPost("XR recv s=%u eff=%u nom=%u",
                    req->seq, RK_RUNNING_PRIO, RK_RUNNING_NOM_PRIO);
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

#elif (RK0_APP_EXAMPLE == APP_BARRIER_PORTS)
/*** SYNCH BARRIER USING PORTS ***/
/*
 * Pattern:
 *
 *     workers -> Barrier server Port -> per-task event release
 *
 * The server task is the coordination authority. Workers do not inspect or
 * mutate shared barrier state directly; they post an arrival record to the
 * server-owned Port and then wait for their own release event.
 */

#define LOG_PRIORITY 5
#define STACKSIZE 256
#define BARRIER_TASK_COUNT 3U
#define BARRIER_PORT_DEPTH 3U
#define BARRIER_RELEASE_EVENT RK_EVENT_31


typedef struct
{
    /* The server needs the caller handle so it can release that task later. */
    RK_TASK_HANDLE sender;
} BarrierReq;

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(barrierHandle, BarrierServer, stackB, STACKSIZE)

static RK_MESG_QUEUE barrierPort;
RK_DECLARE_MESG_QUEUE_BUF(barrierPortBuf, BarrierReq, BARRIER_PORT_DEPTH)

/* Send one arrival record to the server and wait for the server's release. */
static inline VOID BarrierWaitPort(RK_TICK timeout)
{
    BarrierReq req = {.sender = kTaskGetRunningHandle()};
    RK_ERR err = kEventClear(NULL, BARRIER_RELEASE_EVENT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * Non-blocking send is enough here because the Port depth matches the
     * number of workers. A real application can choose a blocking timeout if
     * losing the barrier round is not acceptable.
     */
    err = kPortSend(barrierHandle, &req, RK_NO_WAIT);
    if (err != RK_ERR_SUCCESS)
    {
        logError("%s CALL ERROR %d", RK_RUNNING_NAME, err);
    }

    err = kEventGet(BARRIER_RELEASE_EVENT, RK_EVENT_ANY, NULL, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        if (err == RK_ERR_TIMEOUT)
        {
            logError("%s RELEASE TIMEOUT", RK_RUNNING_NAME);
        }
        else
        {
            logError("%s RELEASE ERROR %d", RK_RUNNING_NAME, err);
        }
    }
}

static VOID BarrierReleaseWaiters(RK_TASK_HANDLE const *const waiters,
                                UINT const nWaiters)
{
    /* Each worker waits on its own task event, so release is explicit per task. */
    for (UINT i = 0U; i < nWaiters; ++i)
    {
        RK_ERR err = kEventSet(waiters[i], BARRIER_RELEASE_EVENT);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

VOID BarrierServer(VOID *args)
{
    RK_UNUSEARGS

    /* Only callers from the current round are stored while they wait. */
    RK_TASK_HANDLE waiters[BARRIER_TASK_COUNT - 1U];
    UINT waitingCount = 0U;
    static CHAR name[RK_OBJ_MAX_NAME_LEN] = {0};

    while (1)
    {
        BarrierReq req = {0};
        RK_ERR err = kPortRecv(&req, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        const UINT arrived = waitingCount + 1U;
        K_ASSERT(req.sender != NULL);
        kTaskGetName(req.sender, name);
        LOG_BARRIER_ENTER(arrived, BARRIER_TASK_COUNT, name);

        if (arrived == BARRIER_TASK_COUNT)
        {
            LOG_BARRIER_WAKE(arrived, BARRIER_TASK_COUNT, name);
            /*
             * Keep the round transition atomic with respect to scheduling so
             * every waiter sees the same release before another round starts.
             */
            kSchLock();
            BarrierReleaseWaiters(waiters, waitingCount);
            err = kEventSet(req.sender, BARRIER_RELEASE_EVENT);
            K_ASSERT(err == RK_ERR_SUCCESS);
            waitingCount = 0U;
            kSchUnlock();
        }
        else
        {
            LOG_BARRIER_BLOCK(arrived, BARRIER_TASK_COUNT, name);
            K_ASSERT(waitingCount < (BARRIER_TASK_COUNT - 1U));
            waiters[waitingCount++] = req.sender;
        }
    }
}

VOID kApplicationInit(VOID)
{
    /* The Barrier task owns the Port and all barrier bookkeeping. */
    RK_ERR err = kCreateTask(&barrierHandle, BarrierServer, RK_NO_ARGS,
                             "Barrier", stackB, STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kPortInit(&barrierPort, barrierPortBuf, RK_MESGQ_MESG_SIZE(BarrierReq),
                    BARRIER_PORT_DEPTH, barrierHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1,
                      STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2,
                      STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3,
                      STACKSIZE, 4, RK_PREEMPT);

    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTraceNameObject(&barrierPort, "BarPort");
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
    logInit(LOG_PRIORITY);
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 running");
        kBusyDelay(RK_MS_TO_TICKS(100)); /* simulate work */
        BarrierWaitPort(RK_MS_TO_TICKS(600));
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 running");
        kBusyDelay(RK_MS_TO_TICKS(200));
        BarrierWaitPort(RK_MS_TO_TICKS(400));
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 running");
        kBusyDelay(RK_MS_TO_TICKS(300));
        BarrierWaitPort(RK_MS_TO_TICKS(100));
        kSleep(RK_MS_TO_TICKS(50));
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
 * semaphore, mutex, sleep queue, message queue, timer, memory partition, MRM,
 * and Channel so trace "list", "top", and "hist" commands have interesting
 * data to display.
 *
 * The Channel call is deliberately outside the mutex ownership window. RK0
 * rejects blocking Channel/Port/Rendezvous operations while the caller owns a
 * mutex, because that can split ownership of one resource across two authority
 * models and break priority inheritance.
 */

#define LOG_PRIORITY 5
#define STACKSIZE 192
#define TRACE_Q_DEPTH 2U
#define TRACE_MRM_BUFS 2U
#define TRACE_MRM_WORDS 2U
#define TRACE_REQ_DEPTH 2U

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
static RK_SLEEP_QUEUE traceSleepq;
static RK_MESG_QUEUE traceQ;
static RK_TIMER traceTimer;
static RK_MEM_PARTITION traceMem;
static RK_MRM traceMrm;
static RK_CHANNEL traceChannel;
static RK_MEM_PARTITION traceReqMem;

RK_DECLARE_MESG_QUEUE_BUF(traceQBuf, TraceMsg, TRACE_Q_DEPTH)
RK_DECLARE_MEM_POOL(TraceMsg, traceMemPool, 2U)
static RK_MRM_BUF traceMrmPool[TRACE_MRM_BUFS] K_ALIGN(4);
static ULONG traceMrmData[TRACE_MRM_BUFS][TRACE_MRM_WORDS] K_ALIGN(4);
RK_DECLARE_CHANNEL_BUF(traceChannelBuf, TRACE_REQ_DEPTH)
RK_DECLARE_MEM_POOL(RK_REQ_BUF, traceReqPool, TRACE_REQ_DEPTH)

static volatile UINT traceTimerTicks;

static VOID TraceTimerCbk(VOID *args)
{
    RK_UNUSEARGS

    traceTimerTicks++;
    /* Timer callbacks should only do bounded work; wake TrRx through a sema. */
    kSemaphorePost(&traceSema);
}

static VOID TraceNameObjects(VOID)
{
    RK_ERR err = kTraceNameObject(&traceSema, "TrSema");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceMutex, "TrMutex");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceSleepq, "TrSleep");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceQ, "TrQueue");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceTimer, "TrTimer");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceMem, "TrMem");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceMrm, "TrMrm");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceChannel, "TrChan");
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&traceReqMem, "TrReq");
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID kApplicationInit(VOID)
{
    TraceMsg bootMsg = {.seq = 0U, .value = 0x1000UL};
    RK_MRM_BUF *bootBufPtr = NULL;
    RK_ERR err = RK_ERR_SUCCESS;

    err = kCreateTask(&traceRxHandle, TraceRxTask, RK_NO_ARGS, "TrRx",
                      traceRxStack, STACKSIZE, 1U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&traceTxHandle, TraceTxTask, RK_NO_ARGS, "TrTx",
                      traceTxStack, STACKSIZE, 2U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&traceWaitHandle, TraceWaitTask, RK_NO_ARGS, "TrWait",
                      traceWaitStack, STACKSIZE, 3U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* Initialize a small set of objects so each trace object class is visible. */
    err = kSemaphoreInit(&traceSema, 0U, 3U);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMutexInit(&traceMutex, RK_INHERIT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSleepQueueInit(&traceSleepq);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMesgQueueInit(&traceQ, traceQBuf, RK_MESGQ_MESG_SIZE(TraceMsg),
                         TRACE_Q_DEPTH);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTimerInit(&traceTimer, 0, RK_MS_TO_TICKS(250),
                     TraceTimerCbk, RK_NO_ARGS, RK_TIMER_RELOAD);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMemPartitionInit(&traceMem, traceMemPool, sizeof(TraceMsg), 2U);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMRMInit(&traceMrm, traceMrmPool, traceMrmData, TRACE_MRM_BUFS,
                   TRACE_MRM_WORDS);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMemPartitionInit(&traceReqMem, traceReqPool, sizeof(RK_REQ_BUF),
                            TRACE_REQ_DEPTH);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kChannelInit(&traceChannel, traceChannelBuf, TRACE_REQ_DEPTH,
                       traceRxHandle, &traceReqMem);
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
        RK_REQ_BUF *reqPtr = NULL;
        UINT response = 0U;

        seq++;
        msg.seq = seq;
        msg.value = 0xA500UL + seq;

        /*
         * This short mutex section exists so TrWait can contend on TrMutex and
         * trace can show mutex ownership. Do not call Channel/Port/Rendezvous
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
        kSemaphorePost(&traceSema);
        kSleepQueueWake(&traceSleepq, 1U, NULL);

        mrmBufPtr = kMRMReserve(&traceMrm);
        if (mrmBufPtr != NULL)
        {
            mrmBufPtr->nUsers = 0U;
            kMRMPublish(&traceMrm, mrmBufPtr, &msg);
        }

        reqPtr = (RK_REQ_BUF *)kMemPartitionAlloc(&traceReqMem);
        if (reqPtr != NULL)
        {
            reqPtr->size = sizeof(msg);
            reqPtr->reqPtr = &msg;
            reqPtr->respPtr = &response;
            kChannelCall(traceRxHandle, reqPtr, RK_WAIT_FOREVER);
        }

        if (memMsgPtr != NULL)
        {
            kMutexLock(&traceMutex, RK_WAIT_FOREVER);
            kMemPartitionFree(&traceMem, memMsgPtr);
            kMutexUnlock(&traceMutex);
        }

        logPost("TraceTx seq=%u timer=%u resp=%u", seq, traceTimerTicks,
                response);
        kSleep(RK_MS_TO_TICKS(300));
    }
}

VOID TraceRxTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        TraceMsg msg = {0U, 0UL};
        TraceMsg mrmMsg = {0U, 0UL};
        RK_REQ_BUF *reqPtr = NULL;

        kMesgQueueRecv(&traceQ, &msg, RK_MS_TO_TICKS(120));
        kSemaphorePend(&traceSema, RK_MS_TO_TICKS(70));

        RK_MRM_BUF *mrmBufPtr = kMRMGet(&traceMrm, &mrmMsg);
        if (mrmBufPtr != NULL)
        {
            kMRMUnget(&traceMrm, mrmBufPtr);
        }

        if (kChannelAccept(&traceChannel, &reqPtr, RK_MS_TO_TICKS(80)) ==
            RK_ERR_SUCCESS)
        {
            /* Channel is request/reply; contrast this with one-way Rendezvous. */
            if ((reqPtr != NULL) && (reqPtr->reqPtr != NULL) &&
                (reqPtr->respPtr != NULL))
            {
                TraceMsg const *reqMsgPtr = (TraceMsg const *)reqPtr->reqPtr;
                UINT *respPtr = (UINT *)reqPtr->respPtr;
                *respPtr = reqMsgPtr->seq + 100U;
            }
            kChannelDone(reqPtr);
        }

        logPost("TraceRx qseq=%u mrm=%u", msg.seq, mrmMsg.seq);
        kSleep(RK_MS_TO_TICKS(90));
    }
}

VOID TraceWaitTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        kSleepQueueWait(&traceSleepq, RK_MS_TO_TICKS(220));
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
 *     workers share one monitor: mutex + condition sleep queue
 *
 * This is the direct shared-state version of the barrier. It contrasts with
 * APP_BARRIER_PORTS: here the Barrier_t monitor is the single authority, and
 * all access to count/round happens under BarLock.
 */


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
    kMutexInit(&barPtr->lock, RK_INHERIT);
    kSleepQueueInit(&barPtr->cond);
    barPtr->count = 0;
    barPtr->round = 0;
}

VOID BarrierWait(Barrier_t *const barPtr, UINT const nTasks, RK_TICK timeout)
{
    UINT myRound = 0;
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
