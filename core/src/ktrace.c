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
/******************************************************************************/
/* COMPONENT: KERNEL TRACE CONSOLE                                            */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kapi.h>
#include <ktrace.h>
#include <stdio.h>

#if (RK_CONF_TRACE == ON)

#define RK_TRACE_RX_EVENT RK_EVENT_32

typedef struct
{
    VOID *objPtr;
    RK_ID objID;
    UINT recordHead;
    UINT recordCount;
    RK_TRACE_RECORD_INFO records[RK_CONF_TRACE_RECORD_DEPTH];
} RK_TRACE_OBJECT_SLOT;

typedef struct
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
} RK_TRACE_NAMED_OBJECT;

static RK_TRACE_OBJECT_SLOT traceObjects[RK_CONF_TRACE_MAX_OBJECTS];
static ULONG tracePrioChanges[RK_NTHREADS];
static UINT traceObjectCount;
static RK_TICK traceTaskTicks[RK_NTHREADS];
static RK_TICK traceWindowTicks;
static RK_TASK_HANDLE traceTaskHandle;
static RK_STACK traceStack[RK_CONF_TRACE_STACKSIZE] K_ALIGN(8);
static CHAR traceLine[RK_CONF_TRACE_LINE_LEN];
static UINT traceLineLen;

static VOID kTraceTask_(VOID *args);
static RK_BOOL kTraceMesgInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_OBJECT_INFO *const outPtr);
static RK_BOOL kTraceSyncInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_SYNC_INFO *const outPtr);
static RK_BOOL kTraceTimerInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_TIMER_INFO *const outPtr);
static RK_TRACE_OBJECT_SLOT *kTraceFindSlot_(VOID const *const objPtr);

INT RK_FUNC_WEAK kTraceUartGetc(CHAR *const chPtr)
{
    K_UNUSE(chPtr);
    return (0);
}

VOID RK_FUNC_WEAK kTraceUartRxEnable(VOID)
{
}

VOID kTraceInputSignalFromISR(VOID)
{
    if (traceTaskHandle != NULL)
    {
        kEventSet(traceTaskHandle, RK_TRACE_RX_EVENT);
    }
}

static VOID kTraceNameCopy_(CHAR *const dstPtr, CHAR const *srcPtr)
{
    UINT i = 0U;

    if (dstPtr == NULL)
    {
        return;
    }

    if (srcPtr == NULL)
    {
        srcPtr = "";
    }

    for (; i < (RK_NAME_SIZE - 1U); i++)
    {
        dstPtr[i] = srcPtr[i];
        if (srcPtr[i] == '\0')
        {
            return;
        }
    }
    dstPtr[i] = '\0';
}

static VOID kTraceNameWithSuffix_(CHAR *const dstPtr, CHAR const *srcPtr,
                                  CHAR const suffix)
{
    UINT i = 0U;

    if ((dstPtr == NULL) || (srcPtr == NULL))
    {
        return;
    }

    for (; (i < (RK_NAME_SIZE - 2U)) && (srcPtr[i] != '\0'); i++)
    {
        dstPtr[i] = srcPtr[i];
    }

    dstPtr[i] = suffix;
    dstPtr[i + 1U] = '\0';
}

static VOID kTraceOwnerNameCopy_(CHAR *const dstPtr,
                                 RK_TCB const *const ownerPtr)
{
    if (ownerPtr == NULL)
    {
        kTraceNameCopy_(dstPtr, "-");
        return;
    }
    kTraceNameCopy_(dstPtr, ownerPtr->taskName);
}

static CHAR *kTraceObjNameBuf_(VOID *const objPtr, RK_ID const objID)
{
    if (objPtr == NULL)
    {
        return (NULL);
    }

    switch (objID)
    {
        case RK_MEMALLOC_KOBJ_ID:
            return (((RK_MEM_PARTITION *)objPtr)->objName);
        case RK_SEMAPHORE_KOBJ_ID:
            return (((RK_SEMAPHORE *)objPtr)->objName);
        case RK_SLEEPQ_KOBJ_ID:
            return (((RK_SLEEP_QUEUE *)objPtr)->objName);
        case RK_MUTEX_KOBJ_ID:
            return (((RK_MUTEX *)objPtr)->objName);
        case RK_MESGQQUEUE_KOBJ_ID:
            return (((RK_MESG_QUEUE *)objPtr)->objName);
        case RK_RENDEZVOUS_KOBJ_ID:
            return (((RK_RENDEZVOUS *)objPtr)->objName);
#if (RK_CONF_CHANNEL == ON)
        case RK_CHANNEL_KOBJ_ID:
            return (((RK_CHANNEL *)objPtr)->objName);
#endif
        case RK_MRM_KOBJ_ID:
            return (((RK_MRM *)objPtr)->objName);
        case RK_TIMER_KOBJ_ID:
            return (((RK_TIMER *)objPtr)->objName);
        default:
            return (NULL);
    }
}

static RK_ID kTraceObjectId_(VOID const *const objPtr)
{
    if (objPtr == NULL)
    {
        return (RK_INVALID_KOBJ);
    }

    return (((RK_TRACE_NAMED_OBJECT const *)objPtr)->objID);
}

static const CHAR *kTraceStatusName_(RK_TASK_STATUS const status)
{
    switch (status)
    {
        case RK_TCB_INITIALISED:
            return ("INIT");
        case RK_READY:
            return ("READY");
        case RK_RUNNING:
            return ("RUN");
        case RK_SLEEPING:
            return ("SLEEP");
        case RK_SLEEPING_EV_FLAG:
            return ("TEV");
        case RK_BLOCKED:
            return ("BLKD");
        case RK_SENDING:
            return ("SEBD");
        case RK_RECEIVING:
            return ("RECV");
        case RK_SLEEPING_DELAY:
            return ("DLY");
        case RK_SLEEPING_RELEASE:
            return ("REL");
        case RK_SLEEPING_UNTIL:
            return ("UNTIL");
        case RK_SLEEPING_SUSPENDED:
            return ("SUSP");
        case RK_PENDING:
            return ("PEND");
        case RK_TASK_TERMINATED:
            return ("TERM");
        default:
            return ("?");
    }
}

static const CHAR *kTraceEventOptName_(RK_OPTION const opt)
{
    switch (opt)
    {
        case RK_EVENT_FLAGS_ANY:
            return ("ANY");
        case RK_EVENT_FLAGS_ALL:
            return ("ALL");
        default:
            return ("-");
    }
}

static const CHAR *kTraceObjName_(RK_ID const objID)
{
    switch (objID)
    {
        case RK_MEMALLOC_KOBJ_ID:
            return ("mem");
        case RK_SLEEPQ_KOBJ_ID:
            return ("sleepq");
        case RK_MRM_KOBJ_ID:
            return ("mrm");
        case RK_MESGQQUEUE_KOBJ_ID:
            return ("mesgq");
        case RK_CHANNEL_KOBJ_ID:
            return ("chan");
        case RK_RENDEZVOUS_KOBJ_ID:
            return ("rdvz");
        case RK_SEMAPHORE_KOBJ_ID:
            return ("sema");
        case RK_MUTEX_KOBJ_ID:
            return ("mutex");
        case RK_TIMER_KOBJ_ID:
            return ("timer");
        default:
            return ("?");
    }
}

static const CHAR *kTraceOpName_(RK_TRACE_OP const op)
{
    switch (op)
    {
        case RK_TRACE_OP_INIT:
            return ("init");
        case RK_TRACE_OP_NAME:
            return ("name");
        case RK_TRACE_OP_QUERY:
            return ("query");
        case RK_TRACE_OP_ALLOC:
            return ("alloc");
        case RK_TRACE_OP_FREE:
            return ("free");
        case RK_TRACE_OP_SEND:
            return ("send");
        case RK_TRACE_OP_RECV:
            return ("recv");
        case RK_TRACE_OP_JAM:
            return ("jam");
        case RK_TRACE_OP_POST:
            return ("post");
        case RK_TRACE_OP_PEND:
            return ("pend");
        case RK_TRACE_OP_BLOCK:
            return ("block");
        case RK_TRACE_OP_WAKE:
            return ("wake");
        case RK_TRACE_OP_TIMEOUT:
            return ("timeout");
        case RK_TRACE_OP_RESET:
            return ("reset");
        case RK_TRACE_OP_LOCK:
            return ("lock");
        case RK_TRACE_OP_UNLOCK:
            return ("unlock");
        case RK_TRACE_OP_CALL:
            return ("call");
        case RK_TRACE_OP_ACCEPT:
            return ("accept");
        case RK_TRACE_OP_DONE:
            return ("done");
        case RK_TRACE_OP_RESERVE:
            return ("reserve");
        case RK_TRACE_OP_PUBLISH:
            return ("publish");
        case RK_TRACE_OP_GET:
            return ("get");
        case RK_TRACE_OP_UNGET:
            return ("unget");
        case RK_TRACE_OP_CANCEL:
            return ("cancel");
        case RK_TRACE_OP_RELOAD:
            return ("reload");
        case RK_TRACE_OP_EXPIRE:
            return ("expire");
        default:
            return ("?");
    }
}

static RK_STACK kTraceStackFree_(RK_TCB const *const taskPtr)
{
    RK_STACK freeWords = 0U;

    if ((taskPtr == NULL) || (taskPtr->stackBufPtr == NULL))
    {
        return (0U);
    }

    for (RK_STACK i = 1U; i < taskPtr->stackSize; i++)
    {
        if (taskPtr->stackBufPtr[i] != RK_STACK_PATTERN)
        {
            break;
        }
        freeWords++;
    }
    return (freeWords);
}

static RK_TRACE_OBJECT_SLOT *kTraceFindSlot_(VOID const *const objPtr)
{
    if (objPtr == NULL)
    {
        return (NULL);
    }

    for (UINT i = 0U; i < traceObjectCount; i++)
    {
        if (traceObjects[i].objPtr == objPtr)
        {
            return (&traceObjects[i]);
        }
    }
    return (NULL);
}

static VOID kTraceRecordSlot_(RK_TRACE_OBJECT_SLOT *const slotPtr,
                              RK_TRACE_OP const op,
                              RK_ERR const result, ULONG const value)
{
    RK_TRACE_RECORD_INFO *recordPtr = NULL;

    if ((slotPtr == NULL) || (RK_CONF_TRACE_RECORD_DEPTH == 0U))
    {
        return;
    }

    recordPtr = &slotPtr->records[slotPtr->recordHead];
    recordPtr->tick = RK_gRunTime.globalTick;
    recordPtr->value = value;
    recordPtr->result = (SHORT)result;
    recordPtr->op = (BYTE)op;
    recordPtr->actorPid = (RK_gRunPtr != NULL) ? RK_gRunPtr->pid : UINT8_MAX;

    slotPtr->recordHead =
        (slotPtr->recordHead + 1U) % RK_CONF_TRACE_RECORD_DEPTH;
    if (slotPtr->recordCount < RK_CONF_TRACE_RECORD_DEPTH)
    {
        slotPtr->recordCount++;
    }
}

static RK_TICK kTraceTimerRemaining_(RK_TIMER const *const timerPtr)
{
    RK_TICK remaining = 0UL;
    RK_TIMEOUT_NODE const *nodePtr = NULL;

    if (timerPtr == NULL)
    {
        return (0UL);
    }

    nodePtr = &timerPtr->timeoutNode;
    while (nodePtr != NULL)
    {
        remaining += nodePtr->dtick;
        if (nodePtr->prevPtr == NULL)
        {
            break;
        }
        nodePtr = nodePtr->prevPtr;
    }
    return (remaining + timerPtr->phase);
}

VOID kTraceTick(VOID)
{
    RK_TCB const *runPtr = RK_gRunPtr;
    if ((runPtr != NULL) && (runPtr->pid < RK_NTHREADS))
    {
        traceTaskTicks[runPtr->pid]++;
        traceWindowTicks++;
    }
}

VOID kTraceRegisterObject(VOID *const objPtr, RK_ID const objID)
{
    CHAR *objNamePtr = NULL;
    RK_TRACE_OBJECT_SLOT *slotPtr = NULL;

    if ((objPtr == NULL) || (objID == RK_INVALID_KOBJ))
    {
        return;
    }

    RK_CR_AREA
    RK_CR_ENTER
    for (UINT i = 0U; i < traceObjectCount; i++)
    {
        if (traceObjects[i].objPtr == objPtr)
        {
            traceObjects[i].objID = objID;
            RK_CR_EXIT
            return;
        }
    }

    if (traceObjectCount < RK_CONF_TRACE_MAX_OBJECTS)
    {
        slotPtr = &traceObjects[traceObjectCount];
        slotPtr->objPtr = objPtr;
        slotPtr->objID = objID;
        slotPtr->recordHead = 0U;
        slotPtr->recordCount = 0U;
        traceObjectCount++;
    }
    else
    {
        RK_CR_EXIT
        return;
    }

    objNamePtr = kTraceObjNameBuf_(objPtr, objID);
    if ((objNamePtr != NULL) && (objNamePtr[0] == '\0'))
    {
        kTraceNameCopy_(objNamePtr, kTraceObjName_(objID));
    }
    kTraceRecordSlot_(slotPtr, RK_TRACE_OP_INIT, RK_ERR_SUCCESS, 0UL);
    RK_CR_EXIT
}

RK_ERR kTraceObjectNameSet(VOID *const objPtr, CHAR const *const namePtr)
{
    RK_ID objID = RK_INVALID_KOBJ;
    CHAR *objNamePtr = NULL;

    if ((objPtr == NULL) || (namePtr == NULL))
    {
        return (RK_ERR_OBJ_NULL);
    }

    RK_CR_AREA
    RK_CR_ENTER
    objID = kTraceObjectId_(objPtr);
    objNamePtr = kTraceObjNameBuf_(objPtr, objID);
    if (objNamePtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    kTraceNameCopy_(objNamePtr, namePtr);
    kTraceRecordSlot_(kTraceFindSlot_(objPtr), RK_TRACE_OP_NAME,
                      RK_ERR_SUCCESS, 0UL);
#if (RK_CONF_MRM == ON)
    if (objID == RK_MRM_KOBJ_ID)
    {
        RK_MRM *const mrmPtr = (RK_MRM *)objPtr;
        kTraceNameWithSuffix_(mrmPtr->mrmMem.objName, namePtr, 'B');
        kTraceRecordSlot_(kTraceFindSlot_(&mrmPtr->mrmMem), RK_TRACE_OP_NAME,
                          RK_ERR_SUCCESS, 0UL);
        kTraceNameWithSuffix_(mrmPtr->mrmDataMem.objName, namePtr, 'D');
        kTraceRecordSlot_(kTraceFindSlot_(&mrmPtr->mrmDataMem),
                          RK_TRACE_OP_NAME, RK_ERR_SUCCESS, 0UL);
    }
#endif
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

VOID kTraceRecordObject(VOID *const objPtr, RK_TRACE_OP const op,
                        RK_ERR const result, ULONG const value)
{
    RK_TRACE_OBJECT_SLOT *slotPtr = NULL;

    if (objPtr == NULL)
    {
        return;
    }

    RK_CR_AREA
    RK_CR_ENTER
    slotPtr = kTraceFindSlot_(objPtr);
    kTraceRecordSlot_(slotPtr, op, result, value);
    RK_CR_EXIT
}

VOID kTraceRecordTaskPrio(RK_TASK_HANDLE const taskHandle,
                          RK_PRIO const oldPriority,
                          RK_PRIO const newPriority)
{
    if ((taskHandle == NULL) || (taskHandle->pid >= RK_NTHREADS) ||
        (taskHandle->init != RK_TRUE) || (oldPriority == newPriority))
    {
        return;
    }

    RK_CR_AREA
    RK_CR_ENTER
    tracePrioChanges[taskHandle->pid]++;
    RK_CR_EXIT
}

UINT kTraceTaskSnapshot(RK_TRACE_TASK_INFO *const infoPtr, UINT const maxInfo)
{
    UINT count = 0U;

    if ((infoPtr == NULL) || (maxInfo == 0U))
    {
        return (0U);
    }

    RK_CR_AREA
    RK_CR_ENTER
    for (UINT i = 0U; (i < RK_NTHREADS) && (count < maxInfo); i++)
    {
        RK_TCB const *taskPtr = &RK_gTcbs[i];
        if (taskPtr->init != RK_TRUE)
        {
            continue;
        }

        RK_TRACE_TASK_INFO *outPtr = &infoPtr[count];
        outPtr->taskHandle = (RK_TASK_HANDLE)taskPtr;
        outPtr->pid = taskPtr->pid;
        for (UINT n = 0U; n < RK_OBJ_MAX_NAME_LEN; n++)
        {
            outPtr->name[n] = taskPtr->taskName[n];
        }
        outPtr->status = taskPtr->status;
        outPtr->priority = taskPtr->priority;
        outPtr->prioNominal = taskPtr->prioNominal;
        outPtr->runCnt = taskPtr->runCnt;
        outPtr->prioChanges = tracePrioChanges[i];
#if (RK_CONF_MUTEX == ON)
        outPtr->ownedMutexes = taskPtr->ownedMutexList.size;
#else
        outPtr->ownedMutexes = 0UL;
#endif
        outPtr->eventCurr = taskPtr->flagsCurr;
        outPtr->eventReq = 0UL;
        outPtr->eventOpt = 0U;
        if (taskPtr->status == RK_SLEEPING_EV_FLAG)
        {
            outPtr->eventReq = taskPtr->flagsReq;
            outPtr->eventOpt = taskPtr->flagsOpt;
        }
        outPtr->cpuTicks = traceTaskTicks[i];
        outPtr->cpuPct = 0U;
        if (traceWindowTicks > 0UL)
        {
            outPtr->cpuPct =
                (UINT)((traceTaskTicks[i] * 100UL) / traceWindowTicks);
        }
        outPtr->stackFreeWords = kTraceStackFree_(taskPtr);
        outPtr->stackSizeWords = taskPtr->stackSize;
        count++;
    }
    RK_CR_EXIT
    return (count);
}

UINT kTraceMesgSnapshot(RK_TRACE_OBJECT_INFO *const infoPtr,
                        UINT const maxInfo)
{
    UINT count = 0U;

    if ((infoPtr == NULL) || (maxInfo == 0U))
    {
        return (0U);
    }

    RK_CR_AREA
    RK_CR_ENTER
    for (UINT i = 0U; (i < traceObjectCount) && (count < maxInfo); i++)
    {
        if (kTraceMesgInfoFromSlot_(&traceObjects[i], &infoPtr[count]) ==
            RK_TRUE)
        {
            count++;
        }
    }
    RK_CR_EXIT
    return (count);
}

UINT kTraceSemaSnapshot(RK_TRACE_SYNC_INFO *const infoPtr, UINT const maxInfo)
{
    UINT count = 0U;

    if ((infoPtr == NULL) || (maxInfo == 0U))
    {
        return (0U);
    }

    RK_CR_AREA
    RK_CR_ENTER
    for (UINT i = 0U; (i < traceObjectCount) && (count < maxInfo); i++)
    {
        if (kTraceSyncInfoFromSlot_(&traceObjects[i], &infoPtr[count]) ==
            RK_TRUE)
        {
            count++;
        }
    }
    RK_CR_EXIT
    return (count);
}

UINT kTraceTimerSnapshot(RK_TRACE_TIMER_INFO *const infoPtr,
                         UINT const maxInfo)
{
    UINT count = 0U;

    if ((infoPtr == NULL) || (maxInfo == 0U))
    {
        return (0U);
    }

    RK_CR_AREA
    RK_CR_ENTER
    for (UINT i = 0U; (i < traceObjectCount) && (count < maxInfo); i++)
    {
        if (kTraceTimerInfoFromSlot_(&traceObjects[i], &infoPtr[count]) ==
            RK_TRUE)
        {
            count++;
        }
    }
    RK_CR_EXIT
    return (count);
}

UINT kTraceRecordSnapshot(VOID *const objPtr,
                          RK_TRACE_RECORD_INFO *const infoPtr,
                          UINT const maxInfo)
{
    UINT count = 0U;
    RK_TRACE_OBJECT_SLOT const *slotPtr = NULL;

    if ((objPtr == NULL) || (infoPtr == NULL) || (maxInfo == 0U))
    {
        return (0U);
    }

    RK_CR_AREA
    RK_CR_ENTER
    slotPtr = kTraceFindSlot_(objPtr);
    if (slotPtr != NULL)
    {
        UINT const toCopy =
            (slotPtr->recordCount < maxInfo) ? slotPtr->recordCount : maxInfo;
        UINT start = 0U;
        if (slotPtr->recordCount == RK_CONF_TRACE_RECORD_DEPTH)
        {
            start = slotPtr->recordHead;
        }

        for (UINT i = 0U; i < toCopy; i++)
        {
            UINT const idx = (start + i) % RK_CONF_TRACE_RECORD_DEPTH;
            infoPtr[i] = slotPtr->records[idx];
        }
        count = toCopy;
    }
    RK_CR_EXIT
    return (count);
}

static RK_BOOL kTraceStrEq_(CHAR const *aPtr, CHAR const *bPtr)
{
    while ((*aPtr != '\0') && (*bPtr != '\0') && (*aPtr == *bPtr))
    {
        aPtr++;
        bPtr++;
    }
    return ((*aPtr == '\0') && (*bPtr == '\0')) ? RK_TRUE : RK_FALSE;
}

static RK_BOOL kTraceStrStarts_(CHAR const *strPtr, CHAR const *prefixPtr)
{
    while (*prefixPtr != '\0')
    {
        if (*strPtr != *prefixPtr)
        {
            return (RK_FALSE);
        }
        strPtr++;
        prefixPtr++;
    }
    return (RK_TRUE);
}

static CHAR const *kTraceSkipSpaces_(CHAR const *strPtr)
{
    while ((strPtr != NULL) && (*strPtr == ' '))
    {
        strPtr++;
    }
    return (strPtr);
}

static CHAR const *kTraceActorName_(RK_PID const pid)
{
    if ((pid < RK_NTHREADS) && (RK_gTcbs[pid].init == RK_TRUE))
    {
        return (RK_gTcbs[pid].taskName);
    }
    return ("-");
}

static CHAR const *kTraceSlotObjName_(RK_TRACE_OBJECT_SLOT const *const slotPtr)
{
    CHAR *namePtr = NULL;

    if (slotPtr == NULL)
    {
        return ("?");
    }

    namePtr = kTraceObjNameBuf_(slotPtr->objPtr, slotPtr->objID);
    if ((namePtr == NULL) || (namePtr[0] == '\0'))
    {
        return (kTraceObjName_(slotPtr->objID));
    }
    return (namePtr);
}

static RK_TRACE_OBJECT_SLOT *kTraceFindSlotByName_(CHAR const *const namePtr)
{
    if ((namePtr == NULL) || (namePtr[0] == '\0'))
    {
        return (NULL);
    }

    for (UINT i = 0U; i < traceObjectCount; i++)
    {
        if (kTraceStrEq_(kTraceSlotObjName_(&traceObjects[i]), namePtr) ==
            RK_TRUE)
        {
            return (&traceObjects[i]);
        }
    }
    return (NULL);
}

static RK_BOOL kTraceMesgInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_OBJECT_INFO *const outPtr)
{
    if ((slotPtr == NULL) || (outPtr == NULL))
    {
        return (RK_FALSE);
    }

    if (slotPtr->objID == RK_MESGQQUEUE_KOBJ_ID)
    {
        RK_MESG_QUEUE const *objPtr = (RK_MESG_QUEUE const *)slotPtr->objPtr;
        if ((objPtr == NULL) || (objPtr->init != RK_TRUE))
        {
            return (RK_FALSE);
        }
        outPtr->objID = slotPtr->objID;
        kTraceNameCopy_(outPtr->objName, objPtr->objName);
        outPtr->objPtr = objPtr;
        kTraceOwnerNameCopy_(outPtr->ownerName, objPtr->ownerTask);
        outPtr->ownerPtr = objPtr->ownerTask;
        outPtr->buffered = objPtr->ringBuf.nFull;
        outPtr->capacity = objPtr->ringBuf.maxBuf;
        outPtr->waitingSenders = objPtr->waitingSenders.size;
        outPtr->waitingReceivers = objPtr->waitingReceivers.size;
        outPtr->waitingRequesters = 0UL;
        outPtr->active = 0U;
        return (RK_TRUE);
    }
    if (slotPtr->objID == RK_RENDEZVOUS_KOBJ_ID)
    {
        RK_RENDEZVOUS const *objPtr = (RK_RENDEZVOUS const *)slotPtr->objPtr;
        if ((objPtr == NULL) || (objPtr->init != RK_TRUE))
        {
            return (RK_FALSE);
        }
        outPtr->objID = slotPtr->objID;
        kTraceNameCopy_(outPtr->objName, objPtr->objName);
        outPtr->objPtr = objPtr;
        kTraceOwnerNameCopy_(outPtr->ownerName, objPtr->ownerTask);
        outPtr->ownerPtr = objPtr->ownerTask;
        outPtr->buffered = (objPtr->inboxMesgPtr != NULL) ? 1UL : 0UL;
        outPtr->capacity = 1UL;
        outPtr->waitingSenders = objPtr->waitingSenders.size;
        outPtr->waitingReceivers =
            (objPtr->rendezvousRecvStorePtr != NULL) ? 1UL : 0UL;
        outPtr->waitingRequesters = 0UL;
        outPtr->active = (objPtr->rendezvousPeerPtr != NULL) ? 1U : 0U;
        return (RK_TRUE);
    }
#if (RK_CONF_CHANNEL == ON)
    if (slotPtr->objID == RK_CHANNEL_KOBJ_ID)
    {
        RK_CHANNEL const *objPtr = (RK_CHANNEL const *)slotPtr->objPtr;
        if ((objPtr == NULL) || (objPtr->init != RK_TRUE))
        {
            return (RK_FALSE);
        }
        outPtr->objID = slotPtr->objID;
        kTraceNameCopy_(outPtr->objName, objPtr->objName);
        outPtr->objPtr = objPtr;
        kTraceOwnerNameCopy_(outPtr->ownerName, objPtr->serverTask);
        outPtr->ownerPtr = objPtr->serverTask;
        outPtr->buffered = objPtr->ringBuf.nFull;
        outPtr->capacity = objPtr->ringBuf.maxBuf;
        outPtr->waitingSenders = 0UL;
        outPtr->waitingReceivers = objPtr->waitingReceivers.size;
        outPtr->waitingRequesters = objPtr->waitingRequesters.size;
        outPtr->active = (objPtr->activeReqPtr != NULL) ? 1U : 0U;
        return (RK_TRUE);
    }
#endif
    return (RK_FALSE);
}

static RK_BOOL kTraceSyncInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_SYNC_INFO *const outPtr)
{
    if ((slotPtr == NULL) || (outPtr == NULL))
    {
        return (RK_FALSE);
    }

    if (slotPtr->objID == RK_SEMAPHORE_KOBJ_ID)
    {
        RK_SEMAPHORE const *objPtr = (RK_SEMAPHORE const *)slotPtr->objPtr;
        if ((objPtr == NULL) || (objPtr->init != RK_TRUE))
        {
            return (RK_FALSE);
        }
        outPtr->objID = slotPtr->objID;
        kTraceNameCopy_(outPtr->objName, objPtr->objName);
        outPtr->objPtr = objPtr;
        kTraceOwnerNameCopy_(outPtr->ownerName, NULL);
        outPtr->ownerPtr = NULL;
        outPtr->locked = 0U;
        outPtr->value = objPtr->value;
        outPtr->maxValue = objPtr->maxValue;
        outPtr->prioInh = 0U;
        outPtr->waitingTasks = objPtr->waitingQueue.size;
        return (RK_TRUE);
    }
    if (slotPtr->objID == RK_MUTEX_KOBJ_ID)
    {
        RK_MUTEX const *objPtr = (RK_MUTEX const *)slotPtr->objPtr;
        if ((objPtr == NULL) || (objPtr->init != RK_TRUE))
        {
            return (RK_FALSE);
        }
        outPtr->objID = slotPtr->objID;
        kTraceNameCopy_(outPtr->objName, objPtr->objName);
        outPtr->objPtr = objPtr;
        kTraceOwnerNameCopy_(outPtr->ownerName, objPtr->ownerPtr);
        outPtr->ownerPtr = objPtr->ownerPtr;
        outPtr->locked = objPtr->lock;
        outPtr->value = (objPtr->lock == RK_FALSE) ? 1U : 0U;
        outPtr->maxValue = 1U;
        outPtr->prioInh = objPtr->prioInh;
        outPtr->waitingTasks = objPtr->waitingQueue.size;
        return (RK_TRUE);
    }
    return (RK_FALSE);
}

static RK_BOOL kTraceTimerInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_TIMER_INFO *const outPtr)
{
    if ((slotPtr == NULL) || (outPtr == NULL) ||
        (slotPtr->objID != RK_TIMER_KOBJ_ID))
    {
        return (RK_FALSE);
    }

    RK_TIMER const *timerPtr = (RK_TIMER const *)slotPtr->objPtr;
    if ((timerPtr == NULL) || (timerPtr->init != RK_TRUE))
    {
        return (RK_FALSE);
    }

    outPtr->objID = slotPtr->objID;
    kTraceNameCopy_(outPtr->objName, timerPtr->objName);
    outPtr->objPtr = timerPtr;
    outPtr->active = kTimeoutNodeIsArmed(&timerPtr->timeoutNode);
    outPtr->reload = timerPtr->reload;
    outPtr->phase = timerPtr->phase;
    outPtr->period = timerPtr->period;
    outPtr->remainingTicks = kTraceTimerRemaining_(timerPtr);
    outPtr->argsPtr = timerPtr->argsPtr;
    return (RK_TRUE);
}

static VOID kTracePrintHelp_(VOID)
{
    printf("\r\nktrace commands:\r\n");
    printf("  top\r\n");
    printf("  list kobjects\r\n");
    printf("  list kmesg\r\n");
    printf("  list ksema\r\n");
    printf("  list kmem\r\n");
    printf("  list ksleepq\r\n");
    printf("  list kmrm\r\n");
    printf("  list ktimers\r\n");
    printf("  list ktimerq\r\n");
    printf("  hist [object-name]\r\n");
    printf("  help\r\n");
}

static VOID kTracePrintTop_(VOID)
{
    printf("\r\nPID NAME     ST     PRIO NOM RUNS  PCHG CPU%% TICKS OWNMTX STACK     EVCUR    EVREQ    EVOP\r\n");
    for (UINT i = 0U; i < RK_NTHREADS; i++)
    {
        RK_BOOL valid = RK_FALSE;
        RK_TRACE_TASK_INFO info;

        RK_CR_AREA
        RK_CR_ENTER
        RK_TCB const *taskPtr = &RK_gTcbs[i];
        if (taskPtr->init == RK_TRUE)
        {
            info.taskHandle = (RK_TASK_HANDLE)taskPtr;
            info.pid = taskPtr->pid;
            for (UINT n = 0U; n < RK_OBJ_MAX_NAME_LEN; n++)
            {
                info.name[n] = taskPtr->taskName[n];
            }
            info.status = taskPtr->status;
            info.priority = taskPtr->priority;
            info.prioNominal = taskPtr->prioNominal;
            info.runCnt = taskPtr->runCnt;
            info.prioChanges = tracePrioChanges[i];
#if (RK_CONF_MUTEX == ON)
            info.ownedMutexes = taskPtr->ownedMutexList.size;
#else
            info.ownedMutexes = 0UL;
#endif
            info.eventCurr = taskPtr->flagsCurr;
            info.eventReq = 0UL;
            info.eventOpt = 0U;
            if (taskPtr->status == RK_SLEEPING_EV_FLAG)
            {
                info.eventReq = taskPtr->flagsReq;
                info.eventOpt = taskPtr->flagsOpt;
            }
            info.cpuTicks = traceTaskTicks[i];
            info.cpuPct = 0U;
            if (traceWindowTicks > 0UL)
            {
                info.cpuPct =
                    (UINT)((traceTaskTicks[i] * 100UL) / traceWindowTicks);
            }
            info.stackFreeWords = kTraceStackFree_(taskPtr);
            info.stackSizeWords = taskPtr->stackSize;
            valid = RK_TRUE;
        }
        RK_CR_EXIT

        if (valid == RK_FALSE)
        {
            continue;
        }

        printf("%3u %-8s %-6s %4u %3u %5lu %4lu %3u %5lu %6lu %4u/%-4u %08lx %08lx %-4s\r\n",
               info.pid, info.name, kTraceStatusName_(info.status),
               info.priority, info.prioNominal, info.runCnt,
               info.prioChanges, info.cpuPct, info.cpuTicks, info.ownedMutexes,
               info.stackFreeWords, info.stackSizeWords,
               (unsigned long)info.eventCurr, (unsigned long)info.eventReq,
               kTraceEventOptName_(info.eventOpt));
    }
}

static VOID kTracePrintKmesg_(VOID)
{
    printf("\r\nTYPE  NAME     OWNER    BUF/CAP SEND RECV REQ ACTIVE\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_TRACE_OBJECT_INFO info;
        RK_BOOL valid = RK_FALSE;

        RK_CR_AREA
        RK_CR_ENTER
        if (i < traceObjectCount)
        {
            valid = kTraceMesgInfoFromSlot_(&traceObjects[i], &info);
        }
        RK_CR_EXIT

        if ((i >= traceObjectCount) || (valid == RK_FALSE))
        {
            continue;
        }

        printf("%-5s %-8s %-8s %3lu/%-3lu %4lu %4lu %3lu %6u\r\n",
               kTraceObjName_(info.objID), info.objName,
               info.ownerName, info.buffered, info.capacity,
               info.waitingSenders, info.waitingReceivers,
               info.waitingRequesters, info.active);
    }
}

static VOID kTracePrintKobjects_(VOID)
{
    printf("\r\nTYPE  NAME     EVENTS LASTOP\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_ID objID = RK_INVALID_KOBJ;
        UINT count = 0U;
        RK_TRACE_OP op = 0U;
        CHAR const *namePtr = "?";

        RK_CR_AREA
        RK_CR_ENTER
        if (i < traceObjectCount)
        {
            RK_TRACE_OBJECT_SLOT const *slotPtr = &traceObjects[i];
            objID = slotPtr->objID;
            count = slotPtr->recordCount;
            namePtr = kTraceSlotObjName_(slotPtr);
            if (slotPtr->recordCount > 0U)
            {
                UINT idx = (slotPtr->recordHead + RK_CONF_TRACE_RECORD_DEPTH -
                            1U) %
                           RK_CONF_TRACE_RECORD_DEPTH;
                op = (RK_TRACE_OP)slotPtr->records[idx].op;
            }
        }
        RK_CR_EXIT

        if (i >= traceObjectCount)
        {
            continue;
        }

        printf("%-5s %-8s %6u %-8s\r\n",
               kTraceObjName_(objID), namePtr, count, kTraceOpName_(op));
    }
}

static VOID kTracePrintKmem_(VOID)
{
    printf("\r\nNAME     BLKSZ FREE/MAX POOL\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_MEM_PARTITION const *objPtr = NULL;
        CHAR name[RK_NAME_SIZE];
        ULONG blkSize = 0UL;
        ULONG freeBlocks = 0UL;
        ULONG maxBlocks = 0UL;
        VOID *poolPtr = NULL;

        name[0] = '\0';
        RK_CR_AREA
        RK_CR_ENTER
        if ((i < traceObjectCount) &&
            (traceObjects[i].objID == RK_MEMALLOC_KOBJ_ID))
        {
            objPtr = (RK_MEM_PARTITION const *)traceObjects[i].objPtr;
            if ((objPtr != NULL) && (objPtr->init == RK_TRUE))
            {
                kTraceNameCopy_(name, objPtr->objName);
                blkSize = objPtr->blkSize;
                freeBlocks = objPtr->nFreeBlocks;
                maxBlocks = objPtr->nMaxBlocks;
                poolPtr = objPtr->poolPtr;
            }
        }
        RK_CR_EXIT

        if (objPtr == NULL)
        {
            continue;
        }

        printf("%-8s %5lu %4lu/%-4lu %p\r\n",
               name, blkSize, freeBlocks, maxBlocks, poolPtr);
    }
}

static VOID kTracePrintKsema_(VOID)
{
    printf("\r\nTYPE  NAME     OWNER    LOCK VAL MAX PI WAIT\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_TRACE_SYNC_INFO info;
        RK_BOOL valid = RK_FALSE;

        RK_CR_AREA
        RK_CR_ENTER
        if (i < traceObjectCount)
        {
            valid = kTraceSyncInfoFromSlot_(&traceObjects[i], &info);
        }
        RK_CR_EXIT

        if ((i >= traceObjectCount) || (valid == RK_FALSE))
        {
            continue;
        }

        printf("%-5s %-8s %-8s %4u %3u %3u %2u %4lu\r\n",
               kTraceObjName_(info.objID), info.objName,
               info.ownerName, info.locked, info.value,
               info.maxValue, info.prioInh, info.waitingTasks);
    }
}

static VOID kTracePrintKsleepq_(VOID)
{
    printf("\r\nNAME     WAIT\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_SLEEP_QUEUE const *objPtr = NULL;
        CHAR name[RK_NAME_SIZE];
        ULONG waiting = 0UL;

        name[0] = '\0';
        RK_CR_AREA
        RK_CR_ENTER
        if ((i < traceObjectCount) &&
            (traceObjects[i].objID == RK_SLEEPQ_KOBJ_ID))
        {
            objPtr = (RK_SLEEP_QUEUE const *)traceObjects[i].objPtr;
            if ((objPtr != NULL) && (objPtr->init == RK_TRUE))
            {
                kTraceNameCopy_(name, objPtr->objName);
                waiting = objPtr->waitingQueue.size;
            }
        }
        RK_CR_EXIT

        if (objPtr == NULL)
        {
            continue;
        }

        printf("%-8s %4lu\r\n", name, waiting);
    }
}

static VOID kTracePrintKmrm_(VOID)
{
    printf("\r\nNAME     WORDS CUR BUF   DATA\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_MRM const *objPtr = NULL;
        CHAR name[RK_NAME_SIZE];
        ULONG words = 0UL;
        UINT current = 0U;
        ULONG bufFree = 0UL;
        ULONG bufMax = 0UL;
        ULONG dataFree = 0UL;
        ULONG dataMax = 0UL;

        name[0] = '\0';
        RK_CR_AREA
        RK_CR_ENTER
        if ((i < traceObjectCount) &&
            (traceObjects[i].objID == RK_MRM_KOBJ_ID))
        {
            objPtr = (RK_MRM const *)traceObjects[i].objPtr;
            if ((objPtr != NULL) && (objPtr->init == RK_TRUE))
            {
                kTraceNameCopy_(name, objPtr->objName);
                words = objPtr->size;
                current = (objPtr->currBufPtr != NULL) ? 1U : 0U;
                bufFree = objPtr->mrmMem.nFreeBlocks;
                bufMax = objPtr->mrmMem.nMaxBlocks;
                dataFree = objPtr->mrmDataMem.nFreeBlocks;
                dataMax = objPtr->mrmDataMem.nMaxBlocks;
            }
        }
        RK_CR_EXIT

        if (objPtr == NULL)
        {
            continue;
        }

        printf("%-8s %5lu %3u %3lu/%-3lu %3lu/%-3lu\r\n",
               name, words, current, bufFree, bufMax, dataFree, dataMax);
    }
}

static VOID kTracePrintKtimers_(VOID)
{
    printf("\r\nNAME     ACT RLD PHASE PERIOD REMAIN ARGS\r\n");
    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_TRACE_TIMER_INFO info;
        RK_BOOL valid = RK_FALSE;

        RK_CR_AREA
        RK_CR_ENTER
        if (i < traceObjectCount)
        {
            valid = kTraceTimerInfoFromSlot_(&traceObjects[i], &info);
        }
        RK_CR_EXIT

        if ((i >= traceObjectCount) || (valid == RK_FALSE))
        {
            continue;
        }

        printf("%-8s %3u %3u %5lu %6lu %6lu %p\r\n",
               info.objName, info.active, info.reload,
               info.phase, info.period, info.remainingTicks,
               info.argsPtr);
    }
}

static VOID kTracePrintKtimerq_(VOID)
{
    RK_TICK acc = 0UL;
    UINT idx = 0U;

    printf("\r\nIDX NAME     DELTA ACCUM PHASE PERIOD NEXT (TICKS)\r\n");

    RK_CR_AREA
    RK_CR_ENTER
    RK_TIMEOUT_NODE const *nodePtr =
        (RK_TIMEOUT_NODE const *)RK_gTimerListHeadPtr;
    while (nodePtr != NULL)
    {
        RK_TIMER const *timerPtr =
            K_GET_CONTAINER_ADDR(nodePtr, RK_TIMER, timeoutNode);
        acc += nodePtr->dtick;
        printf("%3u %-8s %5lu %5lu %5lu %6lu %lu\r\n",
               idx, timerPtr->objName, nodePtr->dtick, acc,
               timerPtr->phase, timerPtr->period, timerPtr->nextTime);
        nodePtr = nodePtr->nextPtr;
        idx++;
    }
    RK_CR_EXIT
}

static VOID kTracePrintHistSlot_(RK_TRACE_OBJECT_SLOT const *const slotPtr)
{
    if (slotPtr == NULL)
    {
        return;
    }

    UINT count = 0U;
    UINT start = 0U;
    CHAR const *namePtr = NULL;
    RK_ID objID = RK_INVALID_KOBJ;

    RK_CR_AREA
    RK_CR_ENTER
    count = slotPtr->recordCount;
    if (slotPtr->recordCount == RK_CONF_TRACE_RECORD_DEPTH)
    {
        start = slotPtr->recordHead;
    }
    namePtr = kTraceSlotObjName_(slotPtr);
    objID = slotPtr->objID;
    RK_CR_EXIT

    printf("\r\nhistory %s/%s\r\n", kTraceObjName_(objID), namePtr);
    printf("TICK     TASK     OP       RET VAL\r\n");

    for (UINT i = 0U; i < count; i++)
    {
        RK_TRACE_RECORD_INFO record;
        RK_CR_ENTER
        UINT const idx = (start + i) % RK_CONF_TRACE_RECORD_DEPTH;
        record = slotPtr->records[idx];
        RK_CR_EXIT

        printf("%8lu %-8s %-8s %4d %lu\r\n",
               record.tick, kTraceActorName_(record.actorPid),
               kTraceOpName_((RK_TRACE_OP)record.op),
               record.result, record.value);
    }
}

static VOID kTracePrintHist_(CHAR const *namePtr)
{
    namePtr = kTraceSkipSpaces_(namePtr);
    if ((namePtr != NULL) && (namePtr[0] != '\0'))
    {
        RK_TRACE_OBJECT_SLOT const *slotPtr = NULL;
        RK_CR_AREA
        RK_CR_ENTER
        slotPtr = kTraceFindSlotByName_(namePtr);
        RK_CR_EXIT
        if (slotPtr == NULL)
        {
            printf("\r\nktrace: object '%s' not found\r\n", namePtr);
            return;
        }
        kTracePrintHistSlot_(slotPtr);
        return;
    }

    for (UINT i = 0U; i < RK_CONF_TRACE_MAX_OBJECTS; i++)
    {
        RK_TRACE_OBJECT_SLOT const *slotPtr = NULL;
        RK_CR_AREA
        RK_CR_ENTER
        if (i < traceObjectCount)
        {
            slotPtr = &traceObjects[i];
        }
        RK_CR_EXIT
        if (slotPtr != NULL)
        {
            kTracePrintHistSlot_(slotPtr);
        }
    }
}

static VOID kTraceExec_(CHAR const *linePtr)
{
    if (kTraceStrEq_(linePtr, "top") == RK_TRUE)
    {
        kTracePrintTop_();
    }
    else if (kTraceStrEq_(linePtr, "list kobjects") == RK_TRUE)
    {
        kTracePrintKobjects_();
    }
    else if (kTraceStrEq_(linePtr, "list kmesg") == RK_TRUE)
    {
        kTracePrintKmesg_();
    }
    else if (kTraceStrEq_(linePtr, "list ksema") == RK_TRUE)
    {
        kTracePrintKsema_();
    }
    else if (kTraceStrEq_(linePtr, "list kmem") == RK_TRUE)
    {
        kTracePrintKmem_();
    }
    else if (kTraceStrEq_(linePtr, "list ksleepq") == RK_TRUE)
    {
        kTracePrintKsleepq_();
    }
    else if (kTraceStrEq_(linePtr, "list kmrm") == RK_TRUE)
    {
        kTracePrintKmrm_();
    }
    else if (kTraceStrEq_(linePtr, "list ktimers") == RK_TRUE)
    {
        kTracePrintKtimers_();
    }
    else if (kTraceStrEq_(linePtr, "list ktimerq") == RK_TRUE)
    {
        kTracePrintKtimerq_();
    }
    else if (kTraceStrStarts_(linePtr, "hist") == RK_TRUE)
    {
        kTracePrintHist_(linePtr + 4);
    }
    else if ((kTraceStrEq_(linePtr, "help") == RK_TRUE) ||
             (kTraceStrEq_(linePtr, "?") == RK_TRUE))
    {
        kTracePrintHelp_();
    }
    else if (linePtr[0] != '\0')
    {
        printf("\r\nktrace: unknown command '%s'\r\n", linePtr);
        kTracePrintHelp_();
    }
    printf("\r\nktrace> ");
}

VOID kTracePoll(VOID)
{
    CHAR ch = '\0';

    while (kTraceUartGetc(&ch) > 0)
    {
        if ((ch == '\r') || (ch == '\n'))
        {
            traceLine[traceLineLen] = '\0';
            kTraceExec_(traceLine);
            traceLineLen = 0U;
        }
        else if ((ch == '\b') || (ch == 0x7FU))
        {
            if (traceLineLen > 0U)
            {
                traceLineLen--;
            }
        }
        else if ((ch >= ' ') && (ch <= '~'))
        {
            if (traceLineLen < (RK_CONF_TRACE_LINE_LEN - 1U))
            {
                traceLine[traceLineLen] = ch;
                traceLineLen++;
            }
        }
        else
        {
            /* no-op */
        }
    }
}

static VOID kTraceTask_(VOID *args)
{
    RK_UNUSEARGS

    printf("\r\nktrace> ");
    while (1)
    {
        kTracePoll();
        kEventGet(RK_TRACE_RX_EVENT, RK_EVENT_ANY, NULL,
                        RK_WAIT_FOREVER);
    }
}

RK_ERR kTraceInit(VOID)
{
    if (traceTaskHandle != NULL)
    {
        return (RK_ERR_SUCCESS);
    }

    RK_ERR err = kTaskInit(&traceTaskHandle, kTraceTask_, RK_NO_ARGS,
                           "KTrace", traceStack, RK_CONF_TRACE_STACKSIZE,
                           RK_CONF_TRACE_PRIO, RK_PREEMPT);
    if (err == RK_ERR_SUCCESS)
    {
        kTraceUartRxEnable();
    }
    return (err);
}

#endif /* RK_CONF_TRACE */
