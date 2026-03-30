
#include <kapi.h>
#include <logger.h>

#define LOG_PRIORITY 5
#define STACKSIZE 256

#define HVAC_CHANNEL_DEPTH 4U /* max number of pending requests */
#define HVAC_APDU_MAX_PAYLOAD 8U /* max payload size in bytes */

/* instructions */
#define HVAC_INS_SET_POWER ((BYTE)0x10U) 
#define HVAC_INS_SET_MODE ((BYTE)0x11U)
#define HVAC_INS_SET_TARGET_TEMP ((BYTE)0x12U)
#define HVAC_INS_SET_FAN_PERCENT ((BYTE)0x13U)

/* plant mode */
#define HVAC_MODE_HEAT ((BYTE)0x1U)
#define HVAC_MODE_COOL ((BYTE)0x2U)
#define HVAC_MODE_FAN ((BYTE)0x3U)

/* limits */
#define HVAC_MIN_TEMP_C ((BYTE)16U)
#define HVAC_MAX_TEMP_C ((BYTE)30U)

typedef struct
{
    BYTE instruction;
    BYTE payloadSize;
    BYTE payload[HVAC_APDU_MAX_PAYLOAD];
    USHORT crc;
} HVAC_APDU;

typedef struct
{
    BYTE powerOn;
    BYTE mode;
    BYTE targetTempC;
    BYTE fanPercent;
} HVAC_STATE;

/* control tasks and a single actuator server */
RK_DECLARE_TASK(thermostatHandle, ThermostatTask, thermostatStack, STACKSIZE)
RK_DECLARE_TASK(occupancyHandle, OccupancyTask, occupancyStack, STACKSIZE)
RK_DECLARE_TASK(airQualityHandle, AirQualityTask, airQualityStack, STACKSIZE)
RK_DECLARE_TASK(hvacServerHandle, HvacServerTask, hvacServerStack, STACKSIZE)

/* declare the channel and the buffer to enqueue the requests */
static RK_CHANNEL hvacChannel; 
RK_DECLARE_CHANNEL_BUF(hvacChannelBuf, HVAC_CHANNEL_DEPTH)

/* request pool for the channel */
static RK_MEM_PARTITION hvacReqPartition;
static RK_REQ_BUF hvacReqPool[HVAC_CHANNEL_DEPTH] K_ALIGN(4);

/* crc computation */
static USHORT HvacCrc16Ccitt_(BYTE const *const dataPtr, UINT const len)
{
    USHORT crc = (USHORT)0xFFFFU;

    for (UINT i = 0U; i < len; ++i)
    {
        crc ^= (USHORT)((USHORT)dataPtr[i] << 8U);

        for (UINT bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & (USHORT)0x8000U) != 0U)
            {
                crc = (USHORT)((USHORT)(crc << 1U) ^ (USHORT)0x1021U);
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
    BYTE frame[2U + HVAC_APDU_MAX_PAYLOAD];
    UINT const payloadSize = (UINT)apduPtr->payloadSize;

    K_ASSERT(payloadSize <= HVAC_APDU_MAX_PAYLOAD);

    frame[0] = apduPtr->instruction;
    frame[1] = apduPtr->payloadSize;

    for (UINT i = 0U; i < payloadSize; ++i)
    {
        frame[2U + i] = apduPtr->payload[i];
    }

    return (HvacCrc16Ccitt_(frame, 2U + payloadSize));
}

static USHORT HvacBuildResponseCrc_(BYTE const instruction,
                                    RK_BOOL const executed,
                                    HVAC_STATE const *const statePtr)
{
    /* Response: CRC over execution status + state. */
    BYTE responseFrame[6U];

    responseFrame[0] = instruction;
    responseFrame[1] = (BYTE)((executed != RK_FALSE) ? 1U : 0U);
    responseFrame[2] = statePtr->powerOn;
    responseFrame[3] = statePtr->mode;
    responseFrame[4] = statePtr->targetTempC;
    responseFrame[5] = statePtr->fanPercent;

    return (HvacCrc16Ccitt_(responseFrame, 6U));
}

static RK_BOOL HvacExecuteInstruction_(HVAC_APDU const *const apduPtr,
                                       HVAC_STATE *const statePtr)
{
    /* command dispatcher: validates limits before actuation. */
    UINT const payloadSize = (UINT)apduPtr->payloadSize;

    if (payloadSize > HVAC_APDU_MAX_PAYLOAD)
    {
        return (RK_FALSE);
    }

    switch (apduPtr->instruction)
    {
    case HVAC_INS_SET_POWER:
        if ((payloadSize != 1U) || (apduPtr->payload[0] > 1U))
        {
            return (RK_FALSE);
        }
        statePtr->powerOn = apduPtr->payload[0];
        return (RK_TRUE);

    case HVAC_INS_SET_MODE:
        if (payloadSize != 1U)
        {
            return (RK_FALSE);
        }
        if ((apduPtr->payload[0] != HVAC_MODE_HEAT) &&
            (apduPtr->payload[0] != HVAC_MODE_COOL) &&
            (apduPtr->payload[0] != HVAC_MODE_FAN))
        {
            return (RK_FALSE);
        }
        statePtr->mode = apduPtr->payload[0];
        return (RK_TRUE);

    case HVAC_INS_SET_TARGET_TEMP:
        if (payloadSize != 1U)
        {
            return (RK_FALSE);
        }
        if ((apduPtr->payload[0] < HVAC_MIN_TEMP_C) ||
            (apduPtr->payload[0] > HVAC_MAX_TEMP_C))
        {
            return (RK_FALSE);
        }
        statePtr->targetTempC = apduPtr->payload[0];
        return (RK_TRUE);

    case HVAC_INS_SET_FAN_PERCENT:
        if ((payloadSize != 1U) || (apduPtr->payload[0] > 100U))
        {
            return (RK_FALSE);
        }
        statePtr->fanPercent = apduPtr->payload[0];
        return (RK_TRUE);

    default:
        return (RK_FALSE);
    }
}

static USHORT HvacChannelCall_(BYTE const instruction,
                               BYTE const *const payloadPtr,
                               BYTE const payloadSize)
{
    /* Synchronous command transaction:
     * allocate request envelope, send APDU, block until server completes.
     */
    HVAC_APDU apdu = {0};
    USHORT responseCrc = 0U;

    RK_REQ_BUF *reqBuf =
        (RK_REQ_BUF *)kMemPartitionAlloc(&hvacReqPartition);
    K_ASSERT(reqBuf != NULL);

    K_ASSERT((UINT)payloadSize <= HVAC_APDU_MAX_PAYLOAD);

    apdu.instruction = instruction;
    apdu.payloadSize = payloadSize;

    for (UINT i = 0U; i < (UINT)payloadSize; ++i)
    {
        apdu.payload[i] = payloadPtr[i];
    }

    apdu.crc = HvacBuildApduCrc_(&apdu);

    reqBuf->size = (ULONG)sizeof(HVAC_APDU);
    reqBuf->reqPtr = &apdu;
    reqBuf->respPtr = &responseCrc;

    RK_ERR err = kChannelCall(hvacServerHandle, reqBuf, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    return (responseCrc);
}

VOID HvacServerTask(VOID *args)
{
    RK_UNUSEARGS

    /* Single-writer plant model: only this task mutates HVAC_STATE. */
    HVAC_STATE hvacState = 
    {
        .powerOn = 0U,
        .mode = HVAC_MODE_COOL,
        .targetTempC = 22U,
        .fanPercent = 40U
    };

    while (1)
    {
        /*
          SERVER EXECUTION FLOW: 
          1) Accept command 
          2) validate APDU 
          3) apply instruction
          4) emit response CRC 
          5) complete channel call.
         */
        RK_REQ_BUF *reqBuf = NULL;
        RK_ERR err = kChannelAccept(&hvacChannel, &reqBuf, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        HVAC_APDU const *apduPtr = (HVAC_APDU const *)reqBuf->reqPtr;
        USHORT *responseCrcPtr = (USHORT *)reqBuf->respPtr;

        K_ASSERT(apduPtr != NULL);
        K_ASSERT(responseCrcPtr != NULL);

        RK_BOOL valid = (RK_BOOL)(reqBuf->size == (ULONG)sizeof(HVAC_APDU));
        RK_BOOL executed = RK_FALSE;

        if (valid != RK_FALSE)
        {
            USHORT expectedCrc = HvacBuildApduCrc_(apduPtr);
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
            logPost("[HVAC-SRV] INS=0x%02x OK PWR=%u MODE=%u T=%uC FAN=%u%% RESP_CRC=0x%04x",
                    (UINT)apduPtr->instruction,
                    (UINT)hvacState.powerOn,
                    (UINT)hvacState.mode,
                    (UINT)hvacState.targetTempC,
                    (UINT)hvacState.fanPercent,
                    (UINT)(*responseCrcPtr));
        }
        else
        {
            logPost("[HVAC-SRV] INS=0x%02x INVALID REQ_CRC=0x%04x RESP_CRC=0x%04x",
                    (UINT)apduPtr->instruction,
                    (UINT)apduPtr->crc,
                    (UINT)(*responseCrcPtr));
        }

        err = kChannelDone(reqBuf);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

VOID ThermostatTask(VOID *args)
{
    RK_UNUSEARGS

    /*  chooses mode and target temperature setpoint. */
    BYTE payload[1U] = {0U};

    while (1)
    {
        payload[0] = HVAC_MODE_HEAT;
        USHORT crcMode = HvacChannelCall_(HVAC_INS_SET_MODE, payload, 1U);
        logPost("[THERMO] SET_MODE=%u RESP_CRC=0x%04x", (UINT)payload[0],
                (UINT)crcMode);

        payload[0] = 24U;
        USHORT crcTemp =
            HvacChannelCall_(HVAC_INS_SET_TARGET_TEMP, payload, 1U);
        logPost("[THERMO] SET_TEMP=%uC RESP_CRC=0x%04x", (UINT)payload[0],
                (UINT)crcTemp);

        kSleep(80U);

        payload[0] = HVAC_MODE_COOL;
        crcMode = HvacChannelCall_(HVAC_INS_SET_MODE, payload, 1U);
        logPost("[THERMO] SET_MODE=%u RESP_CRC=0x%04x", (UINT)payload[0],
                (UINT)crcMode);

        payload[0] = 20U;
        crcTemp = HvacChannelCall_(HVAC_INS_SET_TARGET_TEMP, payload, 1U);
        logPost("[THERMO] SET_TEMP=%uC RESP_CRC=0x%04x", (UINT)payload[0],
                (UINT)crcTemp);

        kSleep(80U);
    }
}

VOID OccupancyTask(VOID *args)
{
    RK_UNUSEARGS

    /* Presence loop: enables/disables HVAC when occupancy changes. 
      (number of people on the room)
    */
    BYTE payload[1U] = {1U};

    while (1)
    {
        USHORT crc = HvacChannelCall_(HVAC_INS_SET_POWER, payload, 1U);
        logPost("[OCCUP] SET_POWER=%u RESP_CRC=0x%04x", (UINT)payload[0],
                (UINT)crc);

        payload[0] = (BYTE)((payload[0] == 0U) ? 1U : 0U);
        kSleep(140U);
    }
}

VOID AirQualityTask(VOID *args)
{
    RK_UNUSEARGS

    /* Air-quality loop: adjusts fan throughput demand. */
    BYTE payload[1U] = {30U};

    while (1)
    {
        USHORT crc = HvacChannelCall_(HVAC_INS_SET_FAN_PERCENT, payload, 1U);
        logPost("[AIRQ ] SET_FAN=%u%% RESP_CRC=0x%04x", (UINT)payload[0],
                (UINT)crc);

        payload[0] = (BYTE)(payload[0] + 20U);
        if (payload[0] > 90U)
        {
            payload[0] = 30U;
        }

        kSleep(110U);
    }
}

VOID kApplicationInit(VOID)
{
    RK_ERR err = kCreateTask(&hvacServerHandle, HvacServerTask, RK_NO_ARGS,
                             "HvacSrv", hvacServerStack, STACKSIZE, 4,
                             RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kMemPartitionInit(&hvacReqPartition, hvacReqPool,
                            sizeof(RK_REQ_BUF), HVAC_CHANNEL_DEPTH);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kChannelInit(&hvacChannel, hvacChannelBuf, HVAC_CHANNEL_DEPTH,
                       hvacServerHandle, &hvacReqPartition);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&thermostatHandle, ThermostatTask, RK_NO_ARGS,
                      "Thermo", thermostatStack, STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&occupancyHandle, OccupancyTask, RK_NO_ARGS,
                      "Occpnc", occupancyStack, STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&airQualityHandle, AirQualityTask, RK_NO_ARGS,
                      "AirQal", airQualityStack, STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
}

int main(void)
{
    kCoreInit();
    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}
