/*
 * Message-passing telemetry demo
 * --------------------------------
 * This example is intentionally busy: everything is expressed in terms of
 * queues, ports and mailboxes so it exercises RK0's rendezvous semantics.
 *
 *   • SensorFastTask / SensorSlowTask push samples into a shared queue. The
 *     samples are copied into the queue, so producers never contend on shared
 *     state and remain strictly asynchronous.
 *   • TelemetryTask owns that queue as well as the RPC port. When it dequeues
 *     a request it inherits the client's priority (QNX style), services the
 *     call, then invokes kPortReplyDone() which drops it back to its base
 *     priority via kPortServerDone().
 *   • Three requesters use kPortSendRecv() with their own mailboxes. Because
 *     the kernel helpfully stamps the sender handle in the message meta, the
 *     server can log who it is servicing and demonstrate priority adoption.
 *   • The logger itself is a mailbox + queue pipeline; logPost() is safe to
 *     call from any context aware of RK_CR_* guards.
 *
 * Observing the output makes the benefits visible: high-priority RPC clients
 * temporarily drag the server up, lower-priority maintenance calls are
 * deferred, and everything is done without shared-memory locks.
 */

#include <application.h>
#include <logger.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SENSOR_QUEUE_DEPTH      16U
#define SENSOR_STACK_WORDS      256
#define REQUESTER_STACK_WORDS   256
#define TELEMETRY_STACK_WORDS   256
#define RPC_MSG_WORDS            4U
#define RPC_PORT_DEPTH           4U
#define TELEMETRY_PRIO           4U

/* you can enum in your code */
#define   RPC_GET_GLOBAL_AVG    1U
#define   RPC_GET_SENSOR_AVG    2U
#define   RPC_RESET_FILTERS     3U

/* APPLICATION DATA */


/* MESSAGE TYPES */

typedef struct
{
    UINT sensorId;
    UINT reading;
    UINT sampleNo;
    UINT _pad; /* ensure 4-word size to match queue rounding */
} SensorSample_t;

_Static_assert(K_TYPE_WORD_COUNT(SensorSample_t) == K_MESGQ_MESG_SIZE(SensorSample_t),
               "SensorSample_t must be padded to queue word size");

typedef struct
{
    RK_PORT_MSG_META meta;
    UINT op;
    UINT arg;
} TelemetryRPC_t K_ALIGN(4);

_Static_assert(K_TYPE_WORD_COUNT(TelemetryRPC_t) == RPC_MSG_WORDS,
               "RPC_MSG_WORDS does not match TelemetryRPC_t size");


typedef struct
{
    ULONG avgScaled[3];       
    ULONG globalAvgScaled;
    UINT   lastSample[3];
    UINT   lastSeq[3];
    UINT   samplesSeen;
    UINT   resetCount;
    UINT   rpcServed;
} TelemetryState_t;

static TelemetryState_t telemetryState;
static const CHAR *const sensorNames[] = { "N/A", "Sensor A", "Sensor B" };

/* DECLARE KERNEL OBJECTS */

static RK_MESG_QUEUE sensorQueue;
RK_DECLARE_MESG_QUEUE_BUF(sensorQueueBuf, SensorSample_t, SENSOR_QUEUE_DEPTH)

static RK_PORT telemetryPort;
RK_DECLARE_PORT_BUF(telemetryPortBuf, RPC_MSG_WORDS, RPC_PORT_DEPTH)


/* HELPERS */
static volatile BOOL telemetryPortReady = FALSE;

static inline UINT avgFromScaled(ULONG value)
{
    return (value >> 3U);
}

static VOID telemetryReset(VOID)
{
    for (UINT i = 0U; i < ARRAY_SIZE(telemetryState.avgScaled); ++i)
    {
        telemetryState.avgScaled[i] = 0U;
        telemetryState.lastSample[i] = 0U;
        telemetryState.lastSeq[i] = 0U;
    }
    telemetryState.globalAvgScaled = 0U;
    telemetryState.samplesSeen = 0U;
    telemetryState.resetCount++;
    logPost("[SERVER] telemetry filters reset (count=%u)", telemetryState.resetCount);
}

static VOID telemetryIntegrate(SensorSample_t const *samplePtr)
{
    UINT id = samplePtr->sensorId;
    if (id >= ARRAY_SIZE(telemetryState.avgScaled))
    {
        return;
    }

    ULONG scaled = telemetryState.avgScaled[id];
    ULONG inputScaled = (ULONG)samplePtr->reading << 3U;
    if (scaled == 0U)
    {
        scaled = inputScaled;
    }
    else
    {
        scaled = scaled - (scaled >> 3U) + inputScaled;
    }
    telemetryState.avgScaled[id] = scaled;
    telemetryState.lastSample[id] = samplePtr->reading;
    telemetryState.lastSeq[id] = samplePtr->sampleNo;

    ULONG gScaled = telemetryState.globalAvgScaled;
    if (gScaled == 0U)
    {
        gScaled = inputScaled;
    }
    else
    {
        gScaled = gScaled - (gScaled >> 3U) + inputScaled;
    }
    telemetryState.globalAvgScaled = gScaled;

    telemetryState.samplesSeen++;

    if ((telemetryState.samplesSeen & 0x7U) == 0U)
    {
        logPost("[COLLECT] %s sample=%u avg=%u global=%u",
                sensorNames[id], samplePtr->reading,
                avgFromScaled(scaled), avgFromScaled(gScaled));
    }
}


/* SENSOR PRODUCERS */

typedef struct
{
    const CHAR *namePtr;
    UINT sensorId;
    UINT base;
    UINT swing;
    UINT step;
    UINT intervalMs;
    UINT logEvery;
    RK_PRIO priority;
} SensorConfig_t;

static const SensorConfig_t sensorFastCfg =
{
    .namePtr = "SensorFast",
    .sensorId = 1U,
    .base = 900U,
    .swing = 120U,
    .step = 11U,
    .intervalMs = 40U,
    .logEvery = 8U,
    .priority = 3U
};

static const SensorConfig_t sensorSlowCfg =
{
    .namePtr = "SensorSlow",
    .sensorId = 2U,
    .base = 540U,
    .swing = 200U,
    .step = 7U,
    .intervalMs = 75U,
    .logEvery = 6U,
    .priority = 5U
};


VOID SensorFastTask(VOID *args)
{
    RK_UNUSEARGS

    static UINT seq = 0U;
    static UINT phase = 0U;
    SensorConfig_t const *const cfgPtr = &sensorFastCfg;
    while (1)
    {
        UINT wave = (cfgPtr->swing * phase) / 32U;
        UINT reading = cfgPtr->base + wave - (cfgPtr->swing / 4U);
        SensorSample_t sample;
        sample.sensorId = cfgPtr->sensorId;
        sample.reading  = reading;
        sample.sampleNo = seq;
        kassert(!kMesgQueueSend(&sensorQueue, &sample, RK_WAIT_FOREVER));
        if ((seq % cfgPtr->logEvery) == 0U)
        {
            logPost("[PRODUCER] %s pushed=%u", cfgPtr->namePtr, reading);
        }
        seq++;
        phase = (phase + cfgPtr->step) & 0x1FU;
        kSleep(cfgPtr->intervalMs);
    }
}


VOID SensorSlowTask(VOID *args)
{
    RK_UNUSEARGS

    static UINT seq = 0U;
    static UINT phase = 0U;
    SensorConfig_t const *const cfgPtr = &sensorSlowCfg;
    while (1)
    {
        UINT wave = (cfgPtr->swing * phase) / 32U;
        UINT reading = cfgPtr->base + wave - (cfgPtr->swing / 4U);
        SensorSample_t sample;
        sample.sensorId = cfgPtr->sensorId;
        sample.reading  = reading;
        sample.sampleNo = seq;
        kassert(!kMesgQueueSend(&sensorQueue, &sample, RK_WAIT_FOREVER));
        if ((seq % cfgPtr->logEvery) == 0U)
        {
            logPost("[PRODUCER] %s pushed=%u", cfgPtr->namePtr, reading);
        }
        seq++;
        phase = (phase + cfgPtr->step) & 0x1FU;
        kSleep(cfgPtr->intervalMs);
    }
}


/* PROCEDURE CALLS */

typedef struct
{
    const CHAR *namePtr;
    UINT op;
    UINT arg;
    UINT periodMs;
    RK_PRIO priority;
} RequesterConfig_t;

static const RequesterConfig_t clientCriticalCfg =
{
    .namePtr = "CtrlFast",
    .op = RPC_GET_GLOBAL_AVG,
    .arg = 0U,
    .periodMs = 120U,
    .priority = 1U
};

static const RequesterConfig_t clientObserverCfg =
{
    .namePtr = "Observer",
    .op = RPC_GET_SENSOR_AVG,
    .arg = 2U,
    .periodMs = 200U,
    .priority = 6U
};

static const RequesterConfig_t maintenanceCfg =
{
    .namePtr = "Maint",
    .op = RPC_RESET_FILTERS,
    .arg = 0U,
    .periodMs = 800U,
    .priority = 7U
};

VOID RequesterTaskRun(RequesterConfig_t const *cfgPtr)
{
    static RK_MAILBOX replyBox = {0};
    static UINT seq = 0U;
    while (1)
    {
        TelemetryRPC_t req =
        {
            .op = cfgPtr->op,
            .arg = cfgPtr->arg
        };
        UINT reply = 0U;
        kassert(!kPortSendRecv(&telemetryPort, (ULONG *)&req, &replyBox, &reply,
                               RK_WAIT_FOREVER));
        if (cfgPtr->op == RPC_GET_SENSOR_AVG)
        {
            logPost("[CLIENT] %s seq=%u sensor%u avg=%u",
                    cfgPtr->namePtr, seq, cfgPtr->arg, reply);
        }
        else if (cfgPtr->op == RPC_GET_GLOBAL_AVG)
        {
            logPost("[CLIENT] %s seq=%u global=%u",
                    cfgPtr->namePtr, seq, reply);
        }
        else if (cfgPtr->op == RPC_RESET_FILTERS)
        {
            logPost("[CLIENT] %s seq=%u resetAck=%u",
                    cfgPtr->namePtr, seq, reply);
        }
        seq++;
        kSleep(cfgPtr->periodMs);
    }
}

VOID ClientCriticalTask(VOID *args)
{
    RK_UNUSEARGS
    RequesterTaskRun(&clientCriticalCfg);
}

VOID ClientObserverTask(VOID *args)
{
    RK_UNUSEARGS
    RequesterTaskRun(&clientObserverCfg);
}

VOID MaintenanceTask(VOID *args)
{
    RK_UNUSEARGS
    RequesterTaskRun(&maintenanceCfg);
}


VOID TelemetryTask(VOID *args)
{
    RK_UNUSEARGS
    telemetryReset();
    while (telemetryPortReady == FALSE)
    {
        kSleep(5U);
    }

    while (1)
    {
        SensorSample_t sample;
        RK_ERR sampleErr = kMesgQueueRecv(&sensorQueue, &sample, 20U);
        if (sampleErr == RK_ERR_SUCCESS)
        {
            telemetryIntegrate(&sample);
        }

        TelemetryRPC_t rpc;

        while (kPortRecv(&telemetryPort, &rpc, RK_NO_WAIT) == RK_ERR_SUCCESS)
        {
            RK_TASK_HANDLE callerPtr = rpc.meta.senderHandle;
            const CHAR *callerNamePtr = callerPtr ? RK_TASK_NAME(callerPtr) : "<anon>";
            UINT callerPrio = callerPtr ? RK_TASK_PRIO(callerPtr) : 0U;
            UINT adoptedPrio = RK_RUNNING_PRIO;

            UINT reply = 0U;
            switch (rpc.op)
            {
                case RPC_GET_GLOBAL_AVG:
                    reply = avgFromScaled(telemetryState.globalAvgScaled);
                    break;
                case RPC_GET_SENSOR_AVG:
                    if (rpc.arg < ARRAY_SIZE(telemetryState.avgScaled))
                    {
                        reply = avgFromScaled(telemetryState.avgScaled[rpc.arg]);
                    }
                    break;
                case RPC_RESET_FILTERS:
                    telemetryReset();
                    reply = telemetryState.resetCount;
                    break;
                default:
                    reply = 0xFFFFFFFFU;
                    break;
            }

            telemetryState.rpcServed++;
            logPost("[SERVER] call%u from %s op=%u arg=%u adopted=%u",
                    telemetryState.rpcServed, callerNamePtr, rpc.op,
                    rpc.arg, adoptedPrio);

            kassert(!kPortReplyDone(&telemetryPort, (ULONG *)&rpc, reply));

            UINT demotedPrio = RK_RUNNING_PRIO;
            logPost("[SERVER] done%u -> %u (caller=%u) restore=%u",
                    telemetryState.rpcServed, reply, callerPrio, demotedPrio);
        }
    }
}

/* TASKS DECLARATION */

RK_DECLARE_TASK(telemetryHandle, TelemetryTask, telemetryStack, TELEMETRY_STACK_WORDS)
RK_DECLARE_TASK(sensorFastHandle, SensorFastTask, sensorFastStack, SENSOR_STACK_WORDS)
RK_DECLARE_TASK(sensorSlowHandle, SensorSlowTask, sensorSlowStack, SENSOR_STACK_WORDS)
RK_DECLARE_TASK(clientCriticalHandle, ClientCriticalTask, clientCriticalStack, REQUESTER_STACK_WORDS)
RK_DECLARE_TASK(clientObserverHandle, ClientObserverTask, clientObserverStack, REQUESTER_STACK_WORDS)
RK_DECLARE_TASK(maintenanceHandle, MaintenanceTask, maintenanceStack, REQUESTER_STACK_WORDS)

/* APPLICATION INIT */

VOID kApplicationInit(VOID)
{

    kassert(!kMesgQueueInit(&sensorQueue, sensorQueueBuf,
                             K_MESGQ_MESG_SIZE(SensorSample_t),
                             SENSOR_QUEUE_DEPTH));

    kassert(!kCreateTask(&telemetryHandle, TelemetryTask, RK_NO_ARGS,
                         "Server", telemetryStack, TELEMETRY_STACK_WORDS,
                         TELEMETRY_PRIO, RK_PREEMPT));

    kassert(!kPortInit(&telemetryPort, telemetryPortBuf, RPC_MSG_WORDS,
                       RPC_PORT_DEPTH, telemetryHandle));
    telemetryPortReady = TRUE;

    kassert(!kCreateTask(&sensorFastHandle, SensorFastTask, RK_NO_ARGS,
                         "SensFast", sensorFastStack, SENSOR_STACK_WORDS,
                         sensorFastCfg.priority, RK_PREEMPT));
    kassert(!kCreateTask(&sensorSlowHandle, SensorSlowTask, RK_NO_ARGS,
                         "SensSlow", sensorSlowStack, SENSOR_STACK_WORDS,
                         sensorSlowCfg.priority, RK_PREEMPT));

    kassert(!kCreateTask(&clientCriticalHandle, ClientCriticalTask,
                         RK_NO_ARGS,
                         "CtrlFast", clientCriticalStack, REQUESTER_STACK_WORDS,
                         clientCriticalCfg.priority, RK_PREEMPT));

    kassert(!kCreateTask(&clientObserverHandle, ClientObserverTask,
                         RK_NO_ARGS,
                         "Observer", clientObserverStack, REQUESTER_STACK_WORDS,
                         clientObserverCfg.priority, RK_PREEMPT));

    kassert(!kCreateTask(&maintenanceHandle, MaintenanceTask,
                         RK_NO_ARGS,
                         "Maint", maintenanceStack, REQUESTER_STACK_WORDS,
                         maintenanceCfg.priority, RK_PREEMPT));
    logInit();

}
