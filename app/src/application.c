/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.60.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/



/***
 * @note
 * This file is intentionally written as executable documentation. Select one
 * example with RK0_APP_EXAMPLE, build it, and watch the logger/trace output to
 * see how the primitive behaves under the scheduler.
 *
 * For ARMv6M some exmples will overflow RAM because by default every kernel
 * service is ON.
 *
 *
 */

#define APP_BARRIER_SHARED 0x1
#define APP_TRACE_EXERCISE 0x2
#define APP_BARRIER_PORTS 0x3
#define APP_RENDEZVOUS_CONTROLLER 0x4
#define APP_TASK_EVENTS 0x5
#define APP_PORT_DAC_BACKPRESSURE 0x6
#define APP_CHANNEL_CALL 0x7
#define APP_MBOX_BROADCAST_RECV 0x8
#define APP_HVAC_CHANNEL 0x9
#define APP_RENDEZVOUS_HANDOFF 0xA

#ifndef RK0_APP_EXAMPLE
#define RK0_APP_EXAMPLE APP_MBOX_BROADCAST_RECV
#endif



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
#define EVT_CONTROL_MASK ((RK_EVENT_FLAG)(EVT_SAMPLE_FLAG | EVT_SETPOINT_FLAG))
#define EVT_ALERT_MASK ((RK_EVENT_FLAG)(EVT_WARN_FLAG | EVT_FAULT_FLAG))

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
    RK_ERR err = kCreateTask(&evtControlHandle, EvtControlTask, RK_NO_ARGS,
                             "EvtCtl", evtControlStack, STACKSIZE,
                             EVT_CONTROL_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&evtAlertHandle, EvtAlertTask, RK_NO_ARGS, "EvtAlr",
                      evtAlertStack, STACKSIZE, EVT_ALERT_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&evtSensorHandle, EvtSensorTask, RK_NO_ARGS, "EvtSens",
                      evtSensorStack, STACKSIZE, EVT_SOURCE_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&evtSetpointHandle, EvtSetpointTask, RK_NO_ARGS,
                      "EvtSet", evtSetpointStack, STACKSIZE, EVT_SOURCE_PRIO,
                      RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&evtAlarmHandle, EvtAlarmTask, RK_NO_ARGS, "EvtSrc",
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
        RK_EVENT_FLAG gotFlags = 0UL;
        RK_EVENT_FLAG afterFlags = 0UL;

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
        RK_EVENT_FLAG gotFlags = 0UL;

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
        RK_EVENT_FLAG flags = EVT_WARN_FLAG;

        evtWarnSeq++;
        if ((evtWarnSeq % 4U) == 0U)
        {
            evtFaultSeq++;
            flags |= EVT_FAULT_FLAG;
        }

        RK_ERR err = kEventSet(evtAlertHandle, flags);
        K_ASSERT(err == RK_ERR_SUCCESS);
        logPost("EvtSrc set mask=%lx", (ULONG)flags);

        kSleepPeriodic(RK_MS_TO_TICKS(300));
    }
}

#elif (RK0_APP_EXAMPLE == APP_CHANNEL_CALL)
/*** CHANNEL REQUEST/REPLY CALL ***/
/*
 * Pattern:
 *
 *     high-priority client -> low-priority server -> reply
 *
 * Channel is an objectless synchronous procedure call. The client blocks with
 * request metadata stored in its TCB. The server accepts into stack-local
 * RK_CALL_DATA and adopts the caller's effective priority until kChannelDone().
 *
 * Watch the server log line and `list kipc`: both print the server effective
 * priority while the request is active.
 */

#define LOG_PRIORITY 5U
#if defined(QEMU_MACHINE_MICROBIT)
#define STACKSIZE 144
#else
#define STACKSIZE 176
#endif
#define CH_CLIENT_PRIO 1U
#define CH_MEDIUM_PRIO 2U
#define CH_SERVER_PRIO 4U

typedef struct
{
    UINT seq;
    UINT sample;
    UINT scale;
    UINT reserved;
} ChannelReq;

typedef struct
{
    UINT seq;
    UINT result;
} ChannelResp;

RK_DECLARE_TASK(chServerHandle, ChServerTask, chServerStack, STACKSIZE)
RK_DECLARE_TASK(chClientHandle, ChClientTask, chClientStack, STACKSIZE)
RK_DECLARE_TASK(chMediumHandle, ChMediumTask, chMediumStack, STACKSIZE)

VOID kApplicationInit(VOID)
{
    RK_ERR err = kCreateTask(&chServerHandle, ChServerTask, RK_NO_ARGS,
                             "ChSrv", chServerStack, STACKSIZE,
                             CH_SERVER_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&chMediumHandle, ChMediumTask, RK_NO_ARGS, "ChMid",
                      chMediumStack, STACKSIZE, CH_MEDIUM_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&chClientHandle, ChClientTask, RK_NO_ARGS, "ChCli",
                      chClientStack, STACKSIZE, CH_CLIENT_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID ChClientTask(VOID *args)
{
    RK_UNUSEARGS

    UINT seq = 0U;
    kSleep(RK_MS_TO_TICKS(80));

    while (1)
    {
        ChannelReq req = {0U, 0U, 0U, 0U};
        ChannelResp resp = {0U, 0U};

        seq++;
        req.seq = seq;
        req.sample = 100U + seq;
        req.scale = 3U;

        logPost("ChCli call seq=%u effPrio=%u srvEff=%u",
                req.seq, RK_RUNNING_PRIO, RK_TASK_PRIO(chServerHandle));
        RK_ERR err = kChannelCall(chServerHandle, &req, &resp,
                                  sizeof(req), RK_WAIT_FOREVER);
        if (err == RK_ERR_SUCCESS)
        {
            logPost("ChCli done seq=%u result=%u",
                    resp.seq, resp.result);
        }
        else
        {
            logError("ChCli call err %d", err);
        }

        kSleep(RK_MS_TO_TICKS(320));
    }
}

VOID ChServerTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_CALL_DATA call = {0};
        RK_ERR err = kChannelAccept(&call, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        if ((call.reqPtr != NULL) && (call.respPtr != NULL))
        {
            ChannelReq const *const reqPtr = (ChannelReq const *)call.reqPtr;
            ChannelResp *const respPtr = (ChannelResp *)call.respPtr;

            logPost("ChSrv accept seq=%u effPrio=%u nomPrio=%u",
                    reqPtr->seq, RK_RUNNING_PRIO, RK_RUNNING_NOM_PRIO);

            respPtr->seq = reqPtr->seq;
            respPtr->result = reqPtr->sample * reqPtr->scale;

            kBusyDelay(RK_MS_TO_TICKS(90));
        }

        err = kChannelDone(&call);
        K_ASSERT(err == RK_ERR_SUCCESS);
        logPost("ChSrv done effPrio=%u nomPrio=%u",
                RK_RUNNING_PRIO, RK_RUNNING_NOM_PRIO);
    }
}

VOID ChMediumTask(VOID *args)
{
    RK_UNUSEARGS

    kSleep(RK_MS_TO_TICKS(120));

    while (1)
    {
        logPost("ChMid run effPrio=%u", RK_RUNNING_PRIO);
        kBusyDelay(RK_MS_TO_TICKS(25));
        kSleep(RK_MS_TO_TICKS(120));
    }
}

#elif (RK0_APP_EXAMPLE == APP_HVAC_CHANNEL)
#include <stdlib.h> /* for rand() */
/*** HVAC CHANNEL CONTROL ***/
/*
 * Pattern:
 *
 *     sensors -> supervisor task -> Channel request -> actuator task
 *
 * The supervisor snapshots shared sensor inputs, builds a small APDU, and calls
 * the actuator by Channel. The actuator owns the plant state and validates the
 * request CRC before applying the control instruction.
 */

#define LOG_PRIORITY 5U
#define STACKSIZE 256
#define TEMP_SENSOR_PERIOD 150U
#define OCC_SENSOR_PERIOD 180U
#define SUPERVISOR_PERIOD 100U
/* workarounds to change occupancy faster */
#define OCC_PRESENT_TO_EMPTY_CHANCE_PCT 35U
#define OCC_EMPTY_TO_PRESENT_CHANCE_PCT 45U
#define OCC_MAX_PRESENT_DWELL_SAMPLES 3U
#define OCC_MAX_EMPTY_DWELL_SAMPLES 2U

#define HVAC_APDU_MAX_PAYLOAD 8U /* max payload size in bytes */
#define HVAC_SETPOINT_C ((BYTE)24U)

/* instruction */
#define HVAC_INS_APPLY_CONTROL ((BYTE)0x30U)

/* control payload fields */
#define HVAC_CONTROL_PAYLOAD_SIZE ((BYTE)4U)
#define HVAC_PAYLOAD_SETPOINT_IDX 0U
#define HVAC_PAYLOAD_CURRENT_IDX 1U
#define HVAC_PAYLOAD_FAN_IDX 2U
#define HVAC_PAYLOAD_OCCUPANCY_IDX 3U
/* occupancy is a binary signal, not a people count */
#define HVAC_OCCUPANCY_EMPTY ((BYTE)0U)
#define HVAC_OCCUPANCY_PRESENT ((BYTE)1U)

/* limits */
#define HVAC_MIN_TEMP_C ((BYTE)16U)
#define HVAC_MAX_TEMP_C ((BYTE)35U)

typedef struct
{
    BYTE instruction;
    BYTE payloadSize;
    BYTE payload[HVAC_APDU_MAX_PAYLOAD];
    USHORT crc;
} HVAC_APDU;

typedef struct
{
    BYTE setpointC;
    BYTE currentTempC;
    BYTE fanPercent;
    BYTE occupancy;
    BYTE powerPercent;
} HVAC_STATE;

typedef struct
{
    RK_MUTEX lock;
    BYTE currentTempC;
    BYTE occupancy;
} HVAC_INPUTS;

/* sensing + supervisor + single actuator */
RK_DECLARE_TASK(tempSensorHandle, TempSensorTask, tempSensorStack, STACKSIZE)
RK_DECLARE_TASK(occupancySensorHandle, OccupancySensorTask,
                occupancySensorStack, STACKSIZE)
RK_DECLARE_TASK(supervisorHandle, SupervisorTask, supervisorStack, STACKSIZE)
RK_DECLARE_TASK(hvacActuatorHandle, HvacActuatorTask, hvacActuatorStack,
                STACKSIZE)

static HVAC_INPUTS hvacInputs;

static VOID HvacInputsInit_(VOID)
{
    RK_ERR err = kMutexInit(&hvacInputs.lock, RK_PRIO_NONE);
    K_ASSERT(err == RK_ERR_SUCCESS);

    hvacInputs.currentTempC = HVAC_SETPOINT_C;
    hvacInputs.occupancy = HVAC_OCCUPANCY_PRESENT;
}

static USHORT HvacBuildApduCrc_(HVAC_APDU const *const apduPtr);
static USHORT HvacBuildResponseCrc_(BYTE const instruction,
                                    RK_BOOL const executed,
                                    HVAC_STATE const *const statePtr);
static BYTE HvacComputePowerPercent_(BYTE const setpointC,
                                     BYTE const currentTempC,
                                     BYTE const fanPercent,
                                     BYTE const occupancy);

static RK_BOOL HvacExecuteInstruction_(HVAC_APDU const *const apduPtr,
                                       HVAC_STATE *const statePtr);


static VOID HvacInputsSetTemp_(BYTE const tempC);
static VOID HvacInputsSetOccupancy_(BYTE const occupancy);
static VOID HvacInputsGet_(BYTE *const tempCPtr, BYTE *const occupancyPtr);
static BYTE HvacClampTempC_(INT const value);
static BYTE HvacComputeFanPercent_(BYTE const currentTempC, BYTE const occupancy);

static USHORT HvacCrc16Ccitt_(BYTE const *const dataPtr, UINT const len)
{
    USHORT crc = 0xFFFFU;

    if (dataPtr == NULL)
    {
        return (crc);
    }

    for (UINT idx = 0U; idx < len; idx++)
    {
        crc = (USHORT)(crc ^ (USHORT)((USHORT)dataPtr[idx] << 8U));
        for (UINT bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (USHORT)((USHORT)(crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc = (USHORT)(crc << 1U);
            }
        }
    }

    return (crc);
}

static USHORT HvacBuildApduCrc_(HVAC_APDU const *const apduPtr)
{
    BYTE frame[2U + HVAC_APDU_MAX_PAYLOAD] = {0U};
    UINT len = 0U;

    if (apduPtr == NULL)
    {
        return (0U);
    }

    frame[len] = apduPtr->instruction;
    len++;
    frame[len] = apduPtr->payloadSize;
    len++;

    UINT payloadBytes = (UINT)apduPtr->payloadSize;
    if (payloadBytes > HVAC_APDU_MAX_PAYLOAD)
    {
        payloadBytes = HVAC_APDU_MAX_PAYLOAD;
    }

    for (UINT idx = 0U; idx < payloadBytes; idx++)
    {
        frame[len] = apduPtr->payload[idx];
        len++;
    }

    return (HvacCrc16Ccitt_(frame, len));
}

static USHORT HvacBuildResponseCrc_(BYTE const instruction,
                                    RK_BOOL const executed,
                                    HVAC_STATE const *const statePtr)
{
    BYTE frame[7U] = {0U};
    UINT len = 0U;

    frame[len] = instruction;
    len++;
    frame[len] = (executed != RK_FALSE) ? 1U : 0U;
    len++;

    if (statePtr != NULL)
    {
        frame[len] = statePtr->setpointC;
        len++;
        frame[len] = statePtr->currentTempC;
        len++;
        frame[len] = statePtr->fanPercent;
        len++;
        frame[len] = statePtr->occupancy;
        len++;
        frame[len] = statePtr->powerPercent;
        len++;
    }

    return (HvacCrc16Ccitt_(frame, len));
}

static BYTE HvacComputePowerPercent_(BYTE const setpointC,
                                     BYTE const currentTempC,
                                     BYTE const fanPercent,
                                     BYTE const occupancy)
{
    if ((occupancy != HVAC_OCCUPANCY_PRESENT) || (fanPercent == 0U))
    {
        return (0U);
    }

    if (currentTempC <= setpointC)
    {
        return ((BYTE)((UINT)fanPercent / 4U));
    }

    UINT const deltaC = (UINT)currentTempC - (UINT)setpointC;
    UINT coolingDemand = deltaC * 25U;
    if (coolingDemand > 100U)
    {
        coolingDemand = 100U;
    }

    UINT powerPercent = (coolingDemand + (UINT)fanPercent) / 2U;
    if (powerPercent > 100U)
    {
        powerPercent = 100U;
    }

    return ((BYTE)powerPercent);
}

static RK_BOOL HvacExecuteInstruction_(HVAC_APDU const *const apduPtr,
                                       HVAC_STATE *const statePtr)
{
    if ((apduPtr == NULL) || (statePtr == NULL) ||
        (apduPtr->instruction != HVAC_INS_APPLY_CONTROL) ||
        (apduPtr->payloadSize != HVAC_CONTROL_PAYLOAD_SIZE))
    {
        return (RK_FALSE);
    }

    BYTE const setpointC = apduPtr->payload[HVAC_PAYLOAD_SETPOINT_IDX];
    BYTE const currentTempC = apduPtr->payload[HVAC_PAYLOAD_CURRENT_IDX];
    BYTE const fanPercent = apduPtr->payload[HVAC_PAYLOAD_FAN_IDX];
    BYTE const occupancy = apduPtr->payload[HVAC_PAYLOAD_OCCUPANCY_IDX];

    if ((setpointC < HVAC_MIN_TEMP_C) || (setpointC > HVAC_MAX_TEMP_C) ||
        (currentTempC < HVAC_MIN_TEMP_C) ||
        (currentTempC > HVAC_MAX_TEMP_C) || (fanPercent > 100U) ||
        ((occupancy != HVAC_OCCUPANCY_EMPTY) &&
         (occupancy != HVAC_OCCUPANCY_PRESENT)))
    {
        return (RK_FALSE);
    }

    statePtr->setpointC = setpointC;
    statePtr->currentTempC = currentTempC;
    statePtr->fanPercent = fanPercent;
    statePtr->occupancy = occupancy;
    statePtr->powerPercent =
        HvacComputePowerPercent_(setpointC, currentTempC, fanPercent,
                                 occupancy);

    return (RK_TRUE);
}

static VOID HvacInputsSetTemp_(BYTE const tempC)
{
    RK_ERR err = kMutexLock(&hvacInputs.lock, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    hvacInputs.currentTempC = HvacClampTempC_((INT)tempC);
    err = kMutexUnlock(&hvacInputs.lock);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

static VOID HvacInputsSetOccupancy_(BYTE const occupancy)
{
    RK_ERR err = kMutexLock(&hvacInputs.lock, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    hvacInputs.occupancy = (occupancy == HVAC_OCCUPANCY_PRESENT)
                               ? HVAC_OCCUPANCY_PRESENT
                               : HVAC_OCCUPANCY_EMPTY;
    err = kMutexUnlock(&hvacInputs.lock);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

static VOID HvacInputsGet_(BYTE *const tempCPtr, BYTE *const occupancyPtr)
{
    RK_ERR err = kMutexLock(&hvacInputs.lock, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    if (tempCPtr != NULL)
    {
        *tempCPtr = hvacInputs.currentTempC;
    }
    if (occupancyPtr != NULL)
    {
        *occupancyPtr = hvacInputs.occupancy;
    }

    err = kMutexUnlock(&hvacInputs.lock);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

static BYTE HvacClampTempC_(INT const value)
{
    if (value < (INT)HVAC_MIN_TEMP_C)
    {
        return (HVAC_MIN_TEMP_C);
    }
    if (value > (INT)HVAC_MAX_TEMP_C)
    {
        return (HVAC_MAX_TEMP_C);
    }
    return ((BYTE)value);
}

static BYTE HvacComputeFanPercent_(BYTE const currentTempC,
                                   BYTE const occupancy)
{
    if (occupancy != HVAC_OCCUPANCY_PRESENT)
    {
        return (0U);
    }

    if (currentTempC <= HVAC_SETPOINT_C)
    {
        return (20U);
    }

    UINT const deltaC = (UINT)currentTempC - (UINT)HVAC_SETPOINT_C;
    UINT fanPercent = 20U + (deltaC * 15U);
    if (fanPercent > 100U)
    {
        fanPercent = 100U;
    }

    return ((BYTE)fanPercent);
}

/* Channel request metadata is carried by the caller TCB. */
static RK_ERR HvacControlCall_(BYTE const setpointC,
                               BYTE const currentTempC,
                               BYTE const fanPercent,
                               BYTE const occupancy,
                               RK_TICK const timeout,
                               USHORT *const responseCrcPtr)
{
    HVAC_APDU apdu = {0};

    K_ASSERT(responseCrcPtr != NULL);

    apdu.instruction = HVAC_INS_APPLY_CONTROL;
    apdu.payloadSize = HVAC_CONTROL_PAYLOAD_SIZE;
    apdu.payload[HVAC_PAYLOAD_SETPOINT_IDX] = setpointC;
    apdu.payload[HVAC_PAYLOAD_CURRENT_IDX] = currentTempC;
    apdu.payload[HVAC_PAYLOAD_FAN_IDX] = fanPercent;
    apdu.payload[HVAC_PAYLOAD_OCCUPANCY_IDX] = occupancy;

    apdu.crc = HvacBuildApduCrc_(&apdu);

    return (kChannelCall(hvacActuatorHandle, &apdu, responseCrcPtr,
                         (ULONG)sizeof(HVAC_APDU), timeout));
}

VOID kApplicationInit(VOID)
{
    HvacInputsInit_();

    RK_ERR err = kCreateTask(&supervisorHandle, SupervisorTask, RK_NO_ARGS,
                             "HvacSup", supervisorStack, STACKSIZE,
                             1U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&tempSensorHandle, TempSensorTask, RK_NO_ARGS,
                      "TempS", tempSensorStack, STACKSIZE, 2U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&occupancySensorHandle, OccupancySensorTask,
                      RK_NO_ARGS, "OccS", occupancySensorStack, STACKSIZE,
                      3U, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&hvacActuatorHandle, HvacActuatorTask, RK_NO_ARGS,
                      "HvacAct", hvacActuatorStack, STACKSIZE, 4U,
                      RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

/* Tasks */
/*prio: 4*/
VOID HvacActuatorTask(VOID *args)
{
    RK_UNUSEARGS

    /* single-writer plant model */
    HVAC_STATE hvacState =
    {
        .setpointC = HVAC_SETPOINT_C,
        .currentTempC = HVAC_SETPOINT_C,
        .fanPercent = 20U,
        .occupancy = HVAC_OCCUPANCY_PRESENT,
        .powerPercent = 20U
    };

    while (1)
    {
        RK_CALL_DATA call = {0};
        RK_ERR err = kChannelAccept(&call, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        HVAC_APDU const *apduPtr = (HVAC_APDU const *)call.reqPtr;
        USHORT *responseCrcPtr = (USHORT *)call.respPtr;

        K_ASSERT(apduPtr != NULL);
        K_ASSERT(responseCrcPtr != NULL);

        RK_BOOL valid = (RK_BOOL)(call.size == (ULONG)sizeof(HVAC_APDU));
        RK_BOOL executed = RK_FALSE;

        if (valid != RK_FALSE)
        {
            USHORT expectedCrc = HvacBuildApduCrc_(apduPtr);

            /* verify message is not corrupted */
            valid = (RK_BOOL)(expectedCrc == apduPtr->crc);
        }

        if (valid != RK_FALSE)
        {
            executed = HvacExecuteInstruction_(apduPtr, &hvacState);
        }

        *responseCrcPtr = HvacBuildResponseCrc_(apduPtr->instruction,
                                                executed,
                                                &hvacState);

        if ((valid != RK_FALSE) && (executed != RK_FALSE))
        {
            logPost("[ACTUATOR] SET=%uC CUR=%uC FAN=%u%% OCC=%u PWR=%u%% RESP_CRC=0x%04x",
                    (UINT)hvacState.setpointC,
                    (UINT)hvacState.currentTempC,
                    (UINT)hvacState.fanPercent,
                    (UINT)hvacState.occupancy,
                    (UINT)hvacState.powerPercent,
                    (UINT)(*responseCrcPtr));
        }
        else
        {
            logPost("[ACTUATOR] INVALID INS=0x%02x REQ_CRC=0x%04x RESP_CRC=0x%04x",
                    (UINT)apduPtr->instruction,
                    (UINT)apduPtr->crc,
                    (UINT)(*responseCrcPtr));
        }

        err = kChannelDone(&call);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}
/*prio: 2*/
VOID TempSensorTask(VOID *args)
{
    RK_UNUSEARGS

    BYTE tempC = (BYTE)(HVAC_SETPOINT_C + 7U); /* start above setpoint */

    while (1)
    {
        /* pseudo-random temperature around setpoint with bounded drift */
        INT const randomStep = (INT)((rand() % 3) - 1); /* -1..+1 */
        INT biasStep = 0;
        INT const errorC = (INT)HVAC_SETPOINT_C - (INT)tempC;

        if ((rand() % 100) < 70)
        {
            if (errorC > 0)
            {
                biasStep = 1;
            }
            else if (errorC < 0)
            {
                biasStep = -1;
            }
        }

        tempC = HvacClampTempC_((INT)tempC + randomStep + biasStep);
        HvacInputsSetTemp_(tempC);
        logPost("[TEMP ] SAMPLE=%uC", (UINT)tempC);

        kSleepRelease(TEMP_SENSOR_PERIOD);
    }
}
/* prio: 3 */
VOID OccupancySensorTask(VOID *args)
{
    RK_UNUSEARGS

    BYTE occupancy = HVAC_OCCUPANCY_PRESENT;
    UINT dwellSamples = 0U;

    while (1)
    {
        /*
         * dwell for state changes faster
         */
        dwellSamples++;
        /* use stdlib.h */
        UINT const chance = (UINT)(rand() % 100);
        if (occupancy == HVAC_OCCUPANCY_PRESENT)
        {
            if ((chance < OCC_PRESENT_TO_EMPTY_CHANCE_PCT) ||
                (dwellSamples >= OCC_MAX_PRESENT_DWELL_SAMPLES))
            {
                occupancy = HVAC_OCCUPANCY_EMPTY;
                dwellSamples = 0U;
            }
        }
        else if ((chance < OCC_EMPTY_TO_PRESENT_CHANCE_PCT) ||
                 (dwellSamples >= OCC_MAX_EMPTY_DWELL_SAMPLES))
        {
            occupancy = HVAC_OCCUPANCY_PRESENT;
            dwellSamples = 0U;
        }

        HvacInputsSetOccupancy_(occupancy);
        logPost("[OCCUP] SAMPLE=%u", (UINT)occupancy);

        kSleepRelease(OCC_SENSOR_PERIOD);
    }
}
/* prio: 1 */
VOID SupervisorTask(VOID *args)
{
    RK_UNUSEARGS


    while (1)
    {
        BYTE currentTempC = HVAC_SETPOINT_C;
        BYTE occupancy = HVAC_OCCUPANCY_EMPTY;

        /*
         * The input mutex only protects this snapshot. kChannelCall() rejects
         * callers that still own any mutex, regardless of mutex protocol.
         */
        HvacInputsGet_(&currentTempC, &occupancy);

        BYTE fanPercent = HvacComputeFanPercent_(currentTempC, occupancy);

        USHORT crc = 0U;
        RK_ERR err = HvacControlCall_(HVAC_SETPOINT_C, currentTempC,
                                      fanPercent, occupancy,
                                      SUPERVISOR_PERIOD, &crc);

        if (err == RK_ERR_SUCCESS)
        {
            logPost("[SUPERV] SET=%uC CUR=%uC FAN=%u%% OCC=%u RESP_CRC=0x%04x",
                    (UINT)HVAC_SETPOINT_C,
                    (UINT)currentTempC,
                    (UINT)fanPercent,
                    (UINT)occupancy,
                    (UINT)crc);
        }
        else
        {
            if (err == RK_ERR_TIMEOUT)
            {
                logPost("[SUPERV] TIMEOUT");
            }
            else
            {
                logError("[SUPERV] ERROR %d SET=%uC CUR=%uC FAN=%u%% OCC=%u",
                        err,
                        (UINT)HVAC_SETPOINT_C,
                        (UINT)currentTempC,
                        (UINT)fanPercent,
                        (UINT)occupancy);
            }
        }
        RK_ERR errsl = kSleepRelease(SUPERVISOR_PERIOD);
        K_ASSERT(errsl == RK_ERR_SUCCESS);
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
#define MBOX_PARKED_FLAGS ((RK_EVENT_FLAG)0x111UL)
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

static RK_EVENT_FLAG MboxParkedFlag_(UINT const receiverIdx)
{
    K_ASSERT(receiverIdx < MBOX_RX_COUNT);
    return ((RK_EVENT_FLAG)(RK_EVENT_1 << (receiverIdx * 4U)));
}

static RK_EVENT_FLAG MboxParkBarrierWait_(VOID)
{
    RK_EVENT_FLAG parkedFlags = 0UL;
    RK_EVENT_FLAG const requiredEvent = MBOX_PARKED_FLAGS;

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
        RK_ERR err = kEventSet(mboxTxHandle, MboxParkedFlag_(receiverIdx));
        K_ASSERT(err == RK_ERR_SUCCESS);

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

    RK_ERR err = kMboxInit(&mboxBroadcast, mboxBroadcastBuf,
                           RK_MESGQ_MESG_SIZE(MboxBroadcastMsg));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&mboxBroadcast, "BcMbox");
    K_ASSERT(err == RK_ERR_SUCCESS);

    /*
     * Broadcast has rendezvous-like admission: no blocked broadcast receiver
     * means no message is deposited.
     */
    err = kMboxBroadcast(&mboxBroadcast, &bootMsg, &nRecv);
    K_ASSERT(err == RK_ERR_BUFFER_EMPTY);
    K_ASSERT(nRecv == 0U);

    err = kCreateTask(&mboxTxHandle, MboxTxTask, RK_NO_ARGS, "MboxTx",
                      mboxTxStack, STACKSIZE, MBOX_BCAST_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&mboxRx1Handle, MboxRx1Task, RK_NO_ARGS, "MboxR1",
                      mboxRx1Stack, STACKSIZE, MBOX_RX1_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&mboxRx2Handle, MboxRx2Task, RK_NO_ARGS, "MboxR2",
                      mboxRx2Stack, STACKSIZE, MBOX_RX2_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kCreateTask(&mboxRx3Handle, MboxRx3Task, RK_NO_ARGS, "MboxR3",
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
        RK_EVENT_FLAG const parkedFlags = MboxParkBarrierWait_();

        err = kMboxQueryWaitingReceivers(&mboxBroadcast, &nWaitingReceivers);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(nWaitingReceivers == MBOX_RX_COUNT);

        if (mboxTxSeq != 0U)
        {
            K_ASSERT(mboxRxPass[0] == mboxTxSeq);
            K_ASSERT(mboxRxPass[1] == mboxTxSeq);
            K_ASSERT(mboxRxPass[2] == mboxTxSeq);
        }

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

#elif (RK0_APP_EXAMPLE == APP_RENDEZVOUS_CONTROLLER)
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
 * A ControlFrame is copied stage-to-stage by Rendezvous. A send completes
 * only when the next stage has copied the frame into its own storage, so a fast
 * producer cannot outrun a slow consumer. This is the core lesson: Rendezvous
 * is unbuffered message passing, not a queue and not a request/reply Channel.
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

/* Each pipeline stage owns one task-backed Rendezvous receive endpoint. */
RK_DECLARE_TASK(senseHandle, SenseTask, senseStack, STACKSIZE)
RK_DECLARE_TASK(filterHandle, FilterTask, filterStack, STACKSIZE)
RK_DECLARE_TASK(ctrlHandle, CtrlTask, ctrlStack, STACKSIZE)
RK_DECLARE_TASK(actHandle, ActTask, actStack, STACKSIZE)

/* Receive one copied frame into the running stage's storage. */
static VOID CtrlPipeRecv_(ControlFrame *const framePtr)
{
    ULONG mesgBytes = 0UL;
    RK_ERR err = kRendezvousRecv(framePtr, &mesgBytes, RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
    {
        logError("%s recv err %d", RK_RUNNING_NAME, err);
    }
    if (mesgBytes != sizeof(ControlFrame))
    {
        logError("%s recv bytes %lu", RK_RUNNING_NAME, mesgBytes);
    }
}

/* Copy the frame to the next stage and wait until that copy completes. */
static VOID CtrlPipeSend_(RK_TASK_HANDLE const receiverHandle,
                          ControlFrame const *const framePtr)
{
    RK_ERR err = kRendezvousSend(receiverHandle, framePtr,
                                 sizeof(ControlFrame), RK_WAIT_FOREVER);
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

    /* Bind one receive slot to each stage. */
    err = kRendezvousInit(senseHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kRendezvousInit(filterHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kRendezvousInit(ctrlHandle, sizeof(ControlFrame));
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kRendezvousInit(actHandle, sizeof(ControlFrame));
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

#elif (RK0_APP_EXAMPLE == APP_RENDEZVOUS_HANDOFF)
/*** SYNCHRONOUS RENDEZVOUS HANDOFF ***/
/*
 * Pattern:
 *
 *     high-priority sender -> low-priority owner, while a medium task runs
 *
 * XrSend uses Rendezvous as "send and wait until copied". There is no reply
 * value. A successful send only means XrOwn copied the payload; after a sender
 * timeout, the kernel invalidates that pending message so the receiver cannot
 * consume a stale request.
 *
 * The priorities make priority inversion visible. While XrSend is blocked in
 * kRendezvousSend(), XrOwn inherits the sender priority long enough to copy
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
     * A Rendezvous endpoint is task-backed. Senders target the owner task
     * handle, not a buffered queue object. The endpoint fixes the maximum
     * word-aligned payload size; each send provides the actual size copied.
     */
    err = kRendezvousInit(xrOwnerHandle, sizeof(RendezvousMsg));
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
        RK_ERR err = kRendezvousSend(xrOwnerHandle, &xrReq,
                                     sizeof(RendezvousMsg),
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
        RendezvousMsg req = {0};
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
        RK_ERR err = kRendezvousRecv(&req, &mesgBytes, RK_WAIT_FOREVER);
        if (err == RK_ERR_SUCCESS)
        {
            K_ASSERT(mesgBytes == sizeof(RendezvousMsg));
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
#if defined(QEMU_MACHINE_MICROBIT)
#define STACKSIZE 160
#else
#define STACKSIZE 256
#endif
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

    logInit(LOG_PRIORITY);

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

#elif (RK0_APP_EXAMPLE == APP_PORT_DAC_BACKPRESSURE)
/*** PORT DAC BACKPRESSURE ***/
/*
 * Pattern:
 *
 *     DAC clients -> DAC manager Port -> DAC resource
 *
 * The DAC manager task is the single authority for the simulated DAC. A Port
 * send admits a command to the bounded queue; it does not mean the command has
 * already been applied. Priority inheritance appears only when the queue is
 * full and a sender blocks waiting for capacity.
 */

#define LOG_PRIORITY 5U
#if defined(QEMU_MACHINE_MICROBIT)
#define STACKSIZE 144
#else
#define STACKSIZE 176
#endif
#define DAC_PORT_DEPTH 4U
#define DAC_HIGH_PRIO 1U
#define DAC_MED_PRIO 3U
#define DAC_MGR_PRIO 4U
#define DAC_SERVICE_TICKS RK_MS_TO_TICKS(80)
#define DAC_HIGH_RELEASE_TICKS RK_MS_TO_TICKS(40)

typedef struct
{
    UINT clientId;
    UINT seq;
    UINT channel;
    UINT sample;
} DacReq;

RK_DECLARE_TASK(dacMgrHandle, DacMgrTask, dacMgrStack, STACKSIZE)
RK_DECLARE_TASK(dacHighHandle, DacHighTask, dacHighStack, STACKSIZE)
RK_DECLARE_TASK(dacMediumHandle, DacMediumTask, dacMediumStack, STACKSIZE)

VOID DacMgrTask(VOID *args);
VOID DacHighTask(VOID *args);
VOID DacMediumTask(VOID *args);

static RK_MESG_QUEUE dacMgrPort;
RK_DECLARE_MESG_QUEUE_BUF(dacMgrPortBuf, DacReq, DAC_PORT_DEPTH)

static VOID DacLogQueueDepth_(CHAR const *const tag, UINT const clientId,
                              UINT const seq, RK_TICK const startMs)
{
    UINT qDepth = 0U;
    RK_ERR err = kPortQuery(dacMgrHandle, &qDepth);
    K_ASSERT(err == RK_ERR_SUCCESS);
    logPost("C%u %s s=%u wait=%lu q=%u", clientId, tag, seq,
            (ULONG)(kTickGetMs() - startMs), qDepth);
}

static VOID DacClientSend_(UINT const clientId, UINT const seq,
                           UINT const channel, UINT const sample)
{
    DacReq req = {.clientId = clientId,
                  .seq = seq,
                  .channel = channel,
                  .sample = sample};
    RK_TICK const startMs = kTickGetMs();
    RK_ERR err = kPortSend(dacMgrHandle, &req, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    DacLogQueueDepth_("enq", clientId, seq, startMs);
}

static VOID DacClientJam_(UINT const clientId, UINT const seq,
                          UINT const channel, UINT const sample)
{
    DacReq req = {.clientId = clientId,
                  .seq = seq,
                  .channel = channel,
                  .sample = sample};
    RK_TICK const startMs = kTickGetMs();
    RK_ERR err = kPortJam(dacMgrHandle, &req, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    DacLogQueueDepth_("jam", clientId, seq, startMs);
}

VOID kApplicationInit(VOID)
{
    RK_ERR err = kCreateTask(&dacMgrHandle, DacMgrTask, RK_NO_ARGS, "DacMgr",
                             dacMgrStack, STACKSIZE, DAC_MGR_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&dacHighHandle, DacHighTask, RK_NO_ARGS, "DacHi",
                      dacHighStack, STACKSIZE, DAC_HIGH_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&dacMediumHandle, DacMediumTask, RK_NO_ARGS, "DacMed",
                      dacMediumStack, STACKSIZE, DAC_MED_PRIO, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kPortInit(&dacMgrPort, dacMgrPortBuf, RK_MESGQ_MESG_SIZE(DacReq),
                    DAC_PORT_DEPTH, dacMgrHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTraceNameObject(&dacMgrPort, "DacPort");
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
    err = kTraceInit();
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID DacMgrTask(VOID *args)
{
    RK_UNUSEARGS

    logPost("DAC demo hi=%u med=%u mgr=%u depth=%u",
            DAC_HIGH_PRIO, DAC_MED_PRIO, DAC_MGR_PRIO, DAC_PORT_DEPTH);

    while (1)
    {
        DacReq req = {0};
        RK_PRIO const pBefore = kTaskGetPrio(dacMgrHandle);
        RK_ERR err = kPortRecv(&req, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        RK_PRIO const pAfter = kTaskGetPrio(dacMgrHandle);
        UINT qDepth = 0U;
        err = kPortQuery(dacMgrHandle, &qDepth);
        K_ASSERT(err == RK_ERR_SUCCESS);

        logPost("MGR d%u_%u ch%u=%u p=%u->%u q=%u",
                req.clientId, req.seq, req.channel, req.sample,
                pBefore, pAfter, qDepth);

        /* Simulated active service time; the manager keeps running. */
        kBusyDelay(DAC_SERVICE_TICKS);
        kSleep(1);
    }
}

VOID DacHighTask(VOID *args)
{
    RK_UNUSEARGS
 while (1)
    {
    kSleepDelay(DAC_HIGH_RELEASE_TICKS);
    logPost("C1 req jam s=2");
    DacClientJam_(1U, 2U, 0U, 1002U);


        kSleepDelay(RK_MS_TO_TICKS(1000));

    }
}

VOID DacMediumTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {

    for (UINT seq = 4U; seq <= 8U; seq++)
    {
        DacClientSend_(2U, seq, 1U, 700U + seq);
    }

   kSleepDelay(RK_MS_TO_TICKS(1000));
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

RK_DECLARE_MESG_QUEUE_BUF(traceQBuf, TraceMsg, TRACE_Q_DEPTH)
RK_DECLARE_MEM_POOL(TraceMsg, traceMemPool, 2U)
static RK_MRM_BUF traceMrmPool[TRACE_MRM_BUFS] K_ALIGN(4);
static ULONG traceMrmData[TRACE_MRM_BUFS][TRACE_MRM_WORDS] K_ALIGN(4);

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
    err = kMutexInit(&traceMutex, RK_PRIO_INHERITANCE);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kSleepQueueInit(&traceSleepq);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMesgQueueInit(&traceQ, traceQBuf, RK_MESGQ_MESG_SIZE(TraceMsg),
                         TRACE_Q_DEPTH);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kTimerInit(&traceTimer, 0, RK_MS_TO_TICKS(250),
                     TraceTimerCbk, RK_NO_ARGS, RK_OPT_TIMER_RELOAD);
    K_ASSERT(err == RK_ERR_SUCCESS);
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

        kChannelCall(traceRxHandle, &msg, &response, sizeof(msg),
                     RK_WAIT_FOREVER);

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
        RK_CALL_DATA call = {0};

        kMesgQueueRecv(&traceQ, &msg, RK_MS_TO_TICKS(120));
        kSemaphorePend(&traceSema, RK_MS_TO_TICKS(70));

        RK_MRM_BUF *mrmBufPtr = kMRMGet(&traceMrm, &mrmMsg);
        if (mrmBufPtr != NULL)
        {
            kMRMUnget(&traceMrm, mrmBufPtr);
        }

        if (kChannelAccept(&call, RK_MS_TO_TICKS(80)) == RK_ERR_SUCCESS)
        {
            /* Channel is request/reply; contrast this with one-way Rendezvous. */
            if ((call.reqPtr != NULL) && (call.respPtr != NULL))
            {
                TraceMsg const *reqMsgPtr = (TraceMsg const *)call.reqPtr;
                UINT *respPtr = (UINT *)call.respPtr;
                *respPtr = reqMsgPtr->seq + 100U;
            }
            kChannelDone(&call);
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
    kMutexInit(&barPtr->lock, RK_PRIO_INHERITANCE);
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
