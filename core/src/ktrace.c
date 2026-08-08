/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                          */
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
#define RK_TRACE_OVERFLOW_EVENT RK_EVENT_31

#if ((RK_CONF_TRACE_FRAME_STDOUT == ON) ||                                  \
     (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U))
#define RK_TRACE_FRAME_MAX_LEN 64U
#define RK_TRACE_FRAME_MAGIC_0 ((BYTE)'R')
#define RK_TRACE_FRAME_MAGIC_1 ((BYTE)'K')
#define RK_TRACE_FRAME_MAGIC_2 ((BYTE)'T')
#define RK_TRACE_FRAME_MAGIC_3 ((BYTE)'R')
#define RK_TRACE_FRAME_VERSION 1U
#endif

#if ((RK_CONF_CHANNEL == ON) || (RK_CONF_RENDEZVOUS == ON))
#define RK_TRACE_HAS_IPC (ON)
#else
#define RK_TRACE_HAS_IPC (OFF)
#endif

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

typedef struct
{
    RK_BOOL valid;
    RK_PID pid;
    CHAR taskName[RK_NAME_SIZE];
    RK_TASK_STATUS status;
    CHAR const *ipcNamePtr;
    CHAR serverName[RK_NAME_SIZE];
    RK_BOOL serverValid;
    RK_PRIO serverPriority;
    RK_PRIO serverNominal;
    CHAR peerName[RK_NAME_SIZE];
    RK_BOOL peerValid;
    RK_PRIO taskPriority;
    RK_PRIO taskNominal;
    CHAR const *stateNamePtr;
    ULONG bytes;
    ULONG callWaiters;
    ULONG acceptWaiters;
    ULONG sendWaiters;
    UINT active;
} RK_TRACE_IPC_ROW;

static RK_TRACE_OBJECT_SLOT traceObjects[RK_CONF_TRACE_MAX_OBJECTS];
static ULONG tracePrioChanges[RK_NTHREADS];
#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
static RK_TRACE_PRIO_RECORD_INFO
    tracePrioRecords[RK_NTHREADS][RK_CONF_TRACE_RECORD_DEPTH];
static UINT tracePrioRecordHead[RK_NTHREADS];
static UINT tracePrioRecordCount[RK_NTHREADS];
#endif
static UINT traceObjectCount;
static RK_TICK traceTaskTicks[RK_NTHREADS];
static ULONG traceTaskCycles[RK_NTHREADS];
static RK_TICK traceWindowTicks;
static RK_TASK_HANDLE traceTaskHandle;
static RK_STACK traceStack[RK_CONF_TRACE_STACKSIZE] K_ALIGN(8);
static CHAR traceLine[RK_CONF_TRACE_LINE_LEN];
static UINT traceLineLen;
#if (RK_CONF_TRACE_OVERFLOW_BACKLOG > 0U)
static RK_TRACE_OVERFLOW_INFO
    traceOverflowBacklog[RK_CONF_TRACE_OVERFLOW_BACKLOG];
static UINT traceOverflowHead;
static UINT traceOverflowTail;
static UINT traceOverflowCount;
static ULONG traceOverflowDropped;
static ULONG traceOverflowSequence;
#endif
#if (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U)
static BYTE
    traceFrameBuffer[RK_CONF_TRACE_FRAME_BUFFER_DEPTH][RK_TRACE_FRAME_MAX_LEN];
static BYTE traceFrameLen[RK_CONF_TRACE_FRAME_BUFFER_DEPTH];
static UINT traceFrameHead;
static UINT traceFrameCount;
static ULONG traceFrameDropped;
#endif

static VOID kTraceTask_(VOID *args);
#if (RK_CONF_MESG_QUEUE == ON)
static RK_BOOL kTraceMesgInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_OBJECT_INFO *const outPtr);
#endif
#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
static RK_BOOL kTraceSyncInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_SYNC_INFO *const outPtr);
#endif
#if (RK_CONF_CALLOUT_TIMER == ON)
static RK_BOOL kTraceTimerInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_TIMER_INFO *const outPtr);
#endif
static RK_TRACE_OBJECT_SLOT *kTraceFindSlot_(VOID const *const objPtr);
static CHAR const *kTraceActorName_(RK_PID const pid);
static CHAR const *kTraceSlotObjName_(RK_TRACE_OBJECT_SLOT const *const slotPtr);
static RK_BOOL kTraceRecordSlot_(RK_TRACE_OBJECT_SLOT *const slotPtr,
                                 RK_TRACE_OP const op,
                                 RK_ERR const result, ULONG const value);
static RK_BOOL kTraceOverflowEnqueueObject_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_RECORD_INFO const *const recordPtr);
#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
static RK_BOOL kTraceOverflowEnqueueTaskPrio_(
    RK_TCB const *const taskPtr,
    RK_TRACE_PRIO_RECORD_INFO const *const recordPtr);
#endif
static RK_BOOL kTraceOverflowEnqueueTaskOverrun_(
    RK_TCB const *const taskPtr,
    RK_TRACE_OVERRUN_RECORD_INFO const *const recordPtr);
static VOID kTraceOverflowSignal_(VOID);
static VOID kTraceOverflowDrain_(VOID);
#if ((RK_CONF_TRACE_FRAME_STDOUT == ON) ||                                  \
     (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U))
static RK_BOOL kTraceOverflowFrameBuild_(
    RK_TRACE_OVERFLOW_INFO const *const infoPtr, BYTE *const framePtr,
    UINT *const lenPtr);
#if (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U)
static VOID kTraceFrameBufferStore_(BYTE const *const framePtr,
                                    UINT const len);
static VOID kTraceFrameBufferPrintAndClear_(VOID);
#endif
#endif

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

#if (RK_CONF_MRM == ON)
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
#endif

#if ((RK_CONF_MESG_QUEUE == ON) || (RK_CONF_SEMAPHORE == ON) ||              \
     (RK_CONF_MUTEX == ON))
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
#endif

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
#if (RK_CONF_SEMAPHORE == ON)
        case RK_SEMAPHORE_KOBJ_ID:
            return (((RK_SEMAPHORE *)objPtr)->objName);
#endif
#if (RK_CONF_SLEEP_QUEUE == ON)
        case RK_SLEEPQ_KOBJ_ID:
            return (((RK_SLEEP_QUEUE *)objPtr)->objName);
#endif
#if (RK_CONF_MUTEX == ON)
        case RK_MUTEX_KOBJ_ID:
            return (((RK_MUTEX *)objPtr)->objName);
#endif
#if (RK_CONF_MESG_QUEUE == ON)
        case RK_MESGQQUEUE_KOBJ_ID:
            return (((RK_MESG_QUEUE *)objPtr)->objName);
#endif
#if (RK_CONF_MRM == ON)
        case RK_MRM_KOBJ_ID:
            return (((RK_MRM *)objPtr)->objName);
#endif
#if (RK_CONF_CALLOUT_TIMER == ON)
        case RK_TIMER_KOBJ_ID:
            return (((RK_TIMER *)objPtr)->objName);
#endif
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
            return ("RDY");
        case RK_RUNNING:
            return ("RUN");
        case RK_SLEEPING:
            return ("SLEEP");
        case RK_SLEEPING_EV_FLAG:
            return ("EVFLAG");
        case RK_BLOCKED:
            return ("BLKD");
        case RK_SENDING:
            return ("SEND");
        case RK_RECEIVING:
            return ("RECV");
        case RK_SLEEPING_DELAY:
            return ("SLPDLY");
        case RK_SLEEPING_RELEASE:
            return ("SLPREL");
        case RK_SLEEPING_UNTIL:
            return ("SLPUNTIL");
        case RK_SLEEPQ_BLOCKED:
            return ("SLPQBLK");
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
        case RK_OPT_EVENT_ANY:
            return ("ANY");
        case RK_OPT_EVENT_ALL:
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
#if (RK_CONF_SLEEP_QUEUE == ON)
        case RK_SLEEPQ_KOBJ_ID:
            return ("sleepq");
#endif
#if (RK_CONF_MRM == ON)
        case RK_MRM_KOBJ_ID:
            return ("mrm");
#endif
#if (RK_CONF_MESG_QUEUE == ON)
        case RK_MESGQQUEUE_KOBJ_ID:
            return ("mesgq");
#endif
#if (RK_CONF_SEMAPHORE == ON)
        case RK_SEMAPHORE_KOBJ_ID:
            return ("sema");
#endif
#if (RK_CONF_MUTEX == ON)
        case RK_MUTEX_KOBJ_ID:
            return ("mutex");
#endif
#if (RK_CONF_CALLOUT_TIMER == ON)
        case RK_TIMER_KOBJ_ID:
            return ("timer");
#endif
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
        case RK_TRACE_OP_SEND_BLOCK:
            return ("sendblk");
        case RK_TRACE_OP_RECV_BLOCK:
            return ("recvblk");
        case RK_TRACE_OP_JAM_BLOCK:
            return ("jamblk");
        case RK_TRACE_OP_PEND_BLOCK:
            return ("pendblk");
        case RK_TRACE_OP_LOCK_BLOCK:
            return ("lockblk");
        case RK_TRACE_OP_WAIT:
            return ("wait");
        case RK_TRACE_OP_WAIT_BLOCK:
            return ("waitblk");
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
        case RK_TRACE_OP_OVERRUN:
            return ("overrun");
        default:
            return ("?");
    }
}

static RK_STACK kTraceStackFirstUsedIndex_(RK_TCB const *const taskPtr)
{
    if ((taskPtr == NULL) || (taskPtr->stackBufPtr == NULL))
    {
        return (0U);
    }

    for (RK_STACK i = 1U; i < taskPtr->stackSize; i++)
    {
        if (taskPtr->stackBufPtr[i] != RK_STACK_PATTERN)
        {
            return (i);
        }
    }
    return (taskPtr->stackSize);
}

static RK_STACK kTraceStackFreeFromIndex_(RK_STACK const firstUsedIndex)
{
    if (firstUsedIndex == 0U)
    {
        return (0U);
    }
    return (firstUsedIndex - 1U);
}

static VOID const *kTraceStackWordPtr_(
    RK_TCB const *const taskPtr,
    RK_STACK const wordIndex)
{
    if ((taskPtr == NULL) || (taskPtr->stackBufPtr == NULL) ||
        (wordIndex >= taskPtr->stackSize))
    {
        return (NULL);
    }
    return ((VOID const *)&taskPtr->stackBufPtr[wordIndex]);
}

static VOID const *kTraceStackLastPtr_(RK_TCB const *const taskPtr)
{
    if ((taskPtr == NULL) || (taskPtr->stackSize == 0U))
    {
        return (NULL);
    }
    return (kTraceStackWordPtr_(taskPtr, taskPtr->stackSize - 1U));
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

static RK_BOOL kTraceRecordSlot_(RK_TRACE_OBJECT_SLOT *const slotPtr,
                                 RK_TRACE_OP const op,
                                 RK_ERR const result, ULONG const value)
{
    RK_TRACE_RECORD_INFO *recordPtr = NULL;
    RK_BOOL overflowQueued = RK_FALSE;

    if ((slotPtr == NULL) || (RK_CONF_TRACE_RECORD_DEPTH == 0U))
    {
        return (RK_FALSE);
    }

    recordPtr = &slotPtr->records[slotPtr->recordHead];
    if (slotPtr->recordCount == RK_CONF_TRACE_RECORD_DEPTH)
    {
        overflowQueued = kTraceOverflowEnqueueObject_(slotPtr, recordPtr);
    }

    recordPtr->tick = RK_gRunTime.globalTick;
    recordPtr->actorPid = (RK_gRunPtr != NULL) ? RK_gRunPtr->pid : UINT8_MAX;
    if (recordPtr->actorPid < RK_NTHREADS)
    {
        recordPtr->actorCycle = traceTaskCycles[recordPtr->actorPid];
        traceTaskCycles[recordPtr->actorPid]++;
    }
    else
    {
        recordPtr->actorCycle = 0UL;
    }
    recordPtr->value = value;
    recordPtr->result = (SHORT)result;
    recordPtr->op = (BYTE)op;

    slotPtr->recordHead =
        (slotPtr->recordHead + 1U) % RK_CONF_TRACE_RECORD_DEPTH;
    if (slotPtr->recordCount < RK_CONF_TRACE_RECORD_DEPTH)
    {
        slotPtr->recordCount++;
    }
    return (overflowQueued);
}

#if (RK_CONF_CALLOUT_TIMER == ON)
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
    return (remaining);
}
#endif

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
    RK_BOOL overflowQueued = RK_FALSE;

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
    overflowQueued =
        kTraceRecordSlot_(slotPtr, RK_TRACE_OP_INIT, RK_ERR_SUCCESS, 0UL);
    RK_CR_EXIT
    if (overflowQueued == RK_TRUE)
    {
        kTraceOverflowSignal_();
    }
}

RK_ERR kTraceObjectNameSet(VOID *const objPtr, CHAR const *const namePtr)
{
    RK_ID objID = RK_INVALID_KOBJ;
    CHAR *objNamePtr = NULL;
    RK_BOOL overflowQueued = RK_FALSE;

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
    overflowQueued =
        kTraceRecordSlot_(kTraceFindSlot_(objPtr), RK_TRACE_OP_NAME,
                          RK_ERR_SUCCESS, 0UL);
#if (RK_CONF_MRM == ON)
    if (objID == RK_MRM_KOBJ_ID)
    {
        RK_MRM *const mrmPtr = (RK_MRM *)objPtr;
        kTraceNameWithSuffix_(mrmPtr->mrmMem.objName, namePtr, 'B');
        overflowQueued |=
            kTraceRecordSlot_(kTraceFindSlot_(&mrmPtr->mrmMem),
                              RK_TRACE_OP_NAME, RK_ERR_SUCCESS, 0UL);
        kTraceNameWithSuffix_(mrmPtr->mrmDataMem.objName, namePtr, 'D');
        overflowQueued |=
            kTraceRecordSlot_(kTraceFindSlot_(&mrmPtr->mrmDataMem),
                              RK_TRACE_OP_NAME, RK_ERR_SUCCESS, 0UL);
    }
#endif
    RK_CR_EXIT
    if (overflowQueued == RK_TRUE)
    {
        kTraceOverflowSignal_();
    }
    return (RK_ERR_SUCCESS);
}

VOID kTraceRecordObject(VOID *const objPtr, RK_TRACE_OP const op,
                        RK_ERR const result, ULONG const value)
{
    RK_TRACE_OBJECT_SLOT *slotPtr = NULL;
    RK_BOOL overflowQueued = RK_FALSE;

    if (objPtr == NULL)
    {
        return;
    }

    RK_CR_AREA
    RK_CR_ENTER
    slotPtr = kTraceFindSlot_(objPtr);
    overflowQueued = kTraceRecordSlot_(slotPtr, op, result, value);
    RK_CR_EXIT
    if (overflowQueued == RK_TRUE)
    {
        kTraceOverflowSignal_();
    }
}

VOID kTraceRecordTaskPrio(RK_TASK_HANDLE const taskHandle,
                          RK_PRIO const oldPriority,
                          RK_PRIO const newPriority)
{
#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
    RK_BOOL overflowQueued = RK_FALSE;
#endif

    if ((taskHandle == NULL) || (taskHandle->pid >= RK_NTHREADS) ||
        (taskHandle->init != RK_TRUE) || (oldPriority == newPriority))
    {
        return;
    }

    RK_CR_AREA
    RK_CR_ENTER
    RK_PID const pid = taskHandle->pid;
    tracePrioChanges[pid]++;
#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
    RK_TRACE_PRIO_RECORD_INFO *const recordPtr =
        &tracePrioRecords[pid][tracePrioRecordHead[pid]];

    if (tracePrioRecordCount[pid] == RK_CONF_TRACE_RECORD_DEPTH)
    {
        overflowQueued = kTraceOverflowEnqueueTaskPrio_(taskHandle, recordPtr);
    }

    recordPtr->tick = RK_gRunTime.globalTick;
    recordPtr->actorPid =
        (RK_gRunPtr != NULL) ? RK_gRunPtr->pid : UINT8_MAX;
    if (recordPtr->actorPid < RK_NTHREADS)
    {
        recordPtr->actorCycle = traceTaskCycles[recordPtr->actorPid];
        traceTaskCycles[recordPtr->actorPid]++;
    }
    else
    {
        recordPtr->actorCycle = 0UL;
    }
    recordPtr->oldPriority = oldPriority;
    recordPtr->newPriority = newPriority;
    recordPtr->nominalPriority = taskHandle->prioNominal;
    tracePrioRecordHead[pid] =
        (tracePrioRecordHead[pid] + 1U) % RK_CONF_TRACE_RECORD_DEPTH;
    if (tracePrioRecordCount[pid] < RK_CONF_TRACE_RECORD_DEPTH)
    {
        tracePrioRecordCount[pid]++;
    }
#endif
    RK_CR_EXIT
#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
    if (overflowQueued == RK_TRUE)
    {
        kTraceOverflowSignal_();
    }
#endif
}

VOID kTraceRecordTaskOverrun(RK_TRACE_OVERRUN_KIND const kind,
                             RK_TICK const period, RK_TICK const lateBy,
                             ULONG const skipped)
{
    RK_TRACE_OVERRUN_RECORD_INFO record;
    RK_BOOL queued = RK_FALSE;

    if ((RK_gRunPtr == NULL) || (RK_gRunPtr->pid >= RK_NTHREADS))
    {
        return;
    }

    RK_CR_AREA
    RK_CR_ENTER
    RK_PID const pid = RK_gRunPtr->pid;
    record.tick = RK_gRunTime.globalTick;
    record.actorCycle = traceTaskCycles[pid];
    traceTaskCycles[pid]++;
    record.actorPid = pid;
    record.overrunKind = kind;
    record.period = period;
    record.lateBy = lateBy;
    record.skipped = skipped;
    record.total = RK_gRunPtr->overrunCount;
    queued = kTraceOverflowEnqueueTaskOverrun_(RK_gRunPtr, &record);
    RK_CR_EXIT

    if (queued == RK_TRUE)
    {
        kTraceOverflowSignal_();
    }
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
        outPtr->overrunCount = taskPtr->overrunCount;
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
        RK_STACK const firstUsedIndex = kTraceStackFirstUsedIndex_(taskPtr);
        outPtr->stackFreeWords = kTraceStackFreeFromIndex_(firstUsedIndex);
        outPtr->stackSizeWords = taskPtr->stackSize;
        outPtr->stackFirstPtr = kTraceStackWordPtr_(taskPtr, 0U);
        outPtr->stackLastPtr = kTraceStackLastPtr_(taskPtr);
        outPtr->stackLowWaterPtr = kTraceStackWordPtr_(taskPtr, firstUsedIndex);
        count++;
    }
    RK_CR_EXIT
    return (count);
}

UINT kTraceMesgSnapshot(RK_TRACE_OBJECT_INFO *const infoPtr,
                        UINT const maxInfo)
{
#if (RK_CONF_MESG_QUEUE == ON)
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
#else
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
#endif
}

UINT kTraceSemaSnapshot(RK_TRACE_SYNC_INFO *const infoPtr, UINT const maxInfo)
{
#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
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
#else
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
#endif
}

UINT kTraceTimerSnapshot(RK_TRACE_TIMER_INFO *const infoPtr,
                         UINT const maxInfo)
{
#if (RK_CONF_CALLOUT_TIMER == ON)
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
#else
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
#endif
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

UINT kTraceTaskPrioSnapshot(RK_TASK_HANDLE const taskHandle,
                            RK_TRACE_PRIO_RECORD_INFO *const infoPtr,
                            UINT const maxInfo)
{
#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
    UINT count = 0U;

    if ((taskHandle == NULL) || (infoPtr == NULL) || (maxInfo == 0U) ||
        (taskHandle->pid >= RK_NTHREADS) || (taskHandle->init != RK_TRUE))
    {
        return (0U);
    }

    RK_CR_AREA
    RK_CR_ENTER
    RK_PID const pid = taskHandle->pid;
    UINT const toCopy =
        (tracePrioRecordCount[pid] < maxInfo) ? tracePrioRecordCount[pid] :
                                                maxInfo;
    UINT start = 0U;
    if (tracePrioRecordCount[pid] == RK_CONF_TRACE_RECORD_DEPTH)
    {
        start = tracePrioRecordHead[pid];
    }

    for (UINT i = 0U; i < toCopy; i++)
    {
        UINT const idx = (start + i) % RK_CONF_TRACE_RECORD_DEPTH;
        infoPtr[i] = tracePrioRecords[pid][idx];
    }
    count = toCopy;
    RK_CR_EXIT
    return (count);
#else
    K_UNUSE(taskHandle);
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
#endif
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

#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
static RK_BOOL kTraceParsePid_(CHAR const *strPtr, RK_PID *const pidPtr)
{
    UINT value = 0U;

    if ((strPtr == NULL) || (strPtr[0] == '\0') || (pidPtr == NULL))
    {
        return (RK_FALSE);
    }

    while (*strPtr != '\0')
    {
        if ((*strPtr < '0') || (*strPtr > '9'))
        {
            return (RK_FALSE);
        }
        value = (value * 10U) + (UINT)(*strPtr - '0');
        if (value >= RK_NTHREADS)
        {
            return (RK_FALSE);
        }
        strPtr++;
    }

    *pidPtr = (RK_PID)value;
    return (RK_TRUE);
}

static RK_TCB const *kTraceFindTaskByNameOrPid_(CHAR const *const namePtr)
{
    RK_PID pid = 0U;

    if ((namePtr == NULL) || (namePtr[0] == '\0'))
    {
        K_UNUSE(pid);
        return (NULL);
    }

    if (kTraceParsePid_(namePtr, &pid) == RK_TRUE)
    {
        if ((pid < RK_NTHREADS) && (RK_gTcbs[pid].init == RK_TRUE))
        {
            return (&RK_gTcbs[pid]);
        }

        K_UNUSE(pid);
        return (NULL);
    }

    K_UNUSE(pid);

    for (UINT i = 0U; i < RK_NTHREADS; i++)
    {
        if ((RK_gTcbs[i].init == RK_TRUE) &&
            (kTraceStrEq_(RK_gTcbs[i].taskName, namePtr) == RK_TRUE))
        {
            return (&RK_gTcbs[i]);
        }
    }

    return (NULL);
}
#endif

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

static RK_BOOL kTraceOverflowEnqueue_(
    RK_TRACE_OVERFLOW_INFO const *const infoPtr)
{
#if (RK_CONF_TRACE_OVERFLOW_BACKLOG > 0U)
    RK_TRACE_OVERFLOW_INFO *outPtr = NULL;

    if (infoPtr == NULL)
    {
        return (RK_FALSE);
    }

    if (traceOverflowCount >= RK_CONF_TRACE_OVERFLOW_BACKLOG)
    {
        traceOverflowDropped++;
        return (RK_FALSE);
    }

    outPtr = &traceOverflowBacklog[traceOverflowHead];
    *outPtr = *infoPtr;
    outPtr->sequence = traceOverflowSequence;
    traceOverflowSequence++;
    outPtr->dropped = traceOverflowDropped;
    traceOverflowDropped = 0UL;
    traceOverflowHead =
        (traceOverflowHead + 1U) % RK_CONF_TRACE_OVERFLOW_BACKLOG;
    traceOverflowCount++;
    return (RK_TRUE);
#else
    K_UNUSE(infoPtr);
    return (RK_FALSE);
#endif
}

static RK_BOOL kTraceOverflowEnqueueObject_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_RECORD_INFO const *const recordPtr)
{
    RK_TRACE_OVERFLOW_INFO info;

    if ((slotPtr == NULL) || (recordPtr == NULL))
    {
        return (RK_FALSE);
    }

    info.kind = RK_TRACE_OVERFLOW_OBJECT;
    info.sequence = 0UL;
    info.objID = slotPtr->objID;
    info.pid = UINT8_MAX;
    kTraceNameCopy_(info.name, kTraceSlotObjName_(slotPtr));
    info.subjectPtr = slotPtr->objPtr;
    info.dropped = 0UL;
    info.objectRecord = *recordPtr;
    info.prioRecord.tick = 0UL;
    info.prioRecord.actorCycle = 0UL;
    info.prioRecord.actorPid = UINT8_MAX;
    info.prioRecord.oldPriority = 0U;
    info.prioRecord.newPriority = 0U;
    info.prioRecord.nominalPriority = 0U;
    info.overrunRecord.tick = 0UL;
    info.overrunRecord.actorCycle = 0UL;
    info.overrunRecord.actorPid = UINT8_MAX;
    info.overrunRecord.overrunKind = RK_TRACE_OVERRUN_RELEASE;
    info.overrunRecord.period = 0UL;
    info.overrunRecord.lateBy = 0UL;
    info.overrunRecord.skipped = 0UL;
    info.overrunRecord.total = 0UL;

    return (kTraceOverflowEnqueue_(&info));
}

#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
static RK_BOOL kTraceOverflowEnqueueTaskPrio_(
    RK_TCB const *const taskPtr,
    RK_TRACE_PRIO_RECORD_INFO const *const recordPtr)
{
    RK_TRACE_OVERFLOW_INFO info;

    if ((taskPtr == NULL) || (recordPtr == NULL))
    {
        return (RK_FALSE);
    }

    info.kind = RK_TRACE_OVERFLOW_TASK_PRIO;
    info.sequence = 0UL;
    info.objID = RK_INVALID_KOBJ;
    info.pid = taskPtr->pid;
    kTraceNameCopy_(info.name, taskPtr->taskName);
    info.subjectPtr = taskPtr;
    info.dropped = 0UL;
    info.objectRecord.tick = 0UL;
    info.objectRecord.actorCycle = 0UL;
    info.objectRecord.value = 0UL;
    info.objectRecord.result = 0;
    info.objectRecord.op = 0U;
    info.objectRecord.actorPid = UINT8_MAX;
    info.prioRecord = *recordPtr;
    info.overrunRecord.tick = 0UL;
    info.overrunRecord.actorCycle = 0UL;
    info.overrunRecord.actorPid = UINT8_MAX;
    info.overrunRecord.overrunKind = RK_TRACE_OVERRUN_RELEASE;
    info.overrunRecord.period = 0UL;
    info.overrunRecord.lateBy = 0UL;
    info.overrunRecord.skipped = 0UL;
    info.overrunRecord.total = 0UL;

    return (kTraceOverflowEnqueue_(&info));
}
#endif

static RK_BOOL kTraceOverflowEnqueueTaskOverrun_(
    RK_TCB const *const taskPtr,
    RK_TRACE_OVERRUN_RECORD_INFO const *const recordPtr)
{
    RK_TRACE_OVERFLOW_INFO info;

    if ((taskPtr == NULL) || (recordPtr == NULL))
    {
        return (RK_FALSE);
    }

    info.kind = RK_TRACE_OVERFLOW_TASK_OVERRUN;
    info.sequence = 0UL;
    info.objID = RK_INVALID_KOBJ;
    info.pid = taskPtr->pid;
    kTraceNameCopy_(info.name, taskPtr->taskName);
    info.subjectPtr = taskPtr;
    info.dropped = 0UL;
    info.objectRecord.tick = 0UL;
    info.objectRecord.actorCycle = 0UL;
    info.objectRecord.value = 0UL;
    info.objectRecord.result = 0;
    info.objectRecord.op = 0U;
    info.objectRecord.actorPid = UINT8_MAX;
    info.prioRecord.tick = 0UL;
    info.prioRecord.actorCycle = 0UL;
    info.prioRecord.actorPid = UINT8_MAX;
    info.prioRecord.oldPriority = 0U;
    info.prioRecord.newPriority = 0U;
    info.prioRecord.nominalPriority = 0U;
    info.overrunRecord = *recordPtr;

    return (kTraceOverflowEnqueue_(&info));
}

static VOID kTraceOverflowSignal_(VOID)
{
    if (traceTaskHandle != NULL)
    {
        kEventSet(traceTaskHandle, RK_TRACE_OVERFLOW_EVENT);
    }
}

static RK_BOOL kTraceOverflowDequeue_(RK_TRACE_OVERFLOW_INFO *const infoPtr)
{
#if (RK_CONF_TRACE_OVERFLOW_BACKLOG > 0U)
    if (infoPtr == NULL)
    {
        return (RK_FALSE);
    }

    RK_CR_AREA
    RK_CR_ENTER
    if (traceOverflowCount == 0U)
    {
        RK_CR_EXIT
        return (RK_FALSE);
    }

    *infoPtr = traceOverflowBacklog[traceOverflowTail];
    traceOverflowTail =
        (traceOverflowTail + 1U) % RK_CONF_TRACE_OVERFLOW_BACKLOG;
    traceOverflowCount--;
    RK_CR_EXIT
    return (RK_TRUE);
#else
    K_UNUSE(infoPtr);
    return (RK_FALSE);
#endif
}

static VOID kTraceOverflowDrain_(VOID)
{
    RK_TRACE_OVERFLOW_INFO info;

    while (kTraceOverflowDequeue_(&info) == RK_TRUE)
    {
        kTraceOverflowPersist(&info);
    }
}

#if ((RK_CONF_TRACE_FRAME_STDOUT == ON) ||                                  \
     (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U))
static UINT kTraceFramePutU8_(BYTE *const bufPtr, UINT const offset,
                              BYTE const value)
{
    bufPtr[offset] = value;
    return (offset + 1U);
}

static UINT kTraceFramePutU16_(BYTE *const bufPtr, UINT const offset,
                               UINT const value)
{
    bufPtr[offset] = (BYTE)(value & 0xFFU);
    bufPtr[offset + 1U] = (BYTE)((value >> 8U) & 0xFFU);
    return (offset + 2U);
}

static UINT kTraceFramePutU32_(BYTE *const bufPtr, UINT const offset,
                               ULONG const value)
{
    bufPtr[offset] = (BYTE)(value & 0xFFUL);
    bufPtr[offset + 1U] = (BYTE)((value >> 8U) & 0xFFUL);
    bufPtr[offset + 2U] = (BYTE)((value >> 16U) & 0xFFUL);
    bufPtr[offset + 3U] = (BYTE)((value >> 24U) & 0xFFUL);
    return (offset + 4U);
}

static UINT kTraceFramePutName_(BYTE *const bufPtr, UINT offset,
                                CHAR const *const namePtr)
{
    for (UINT i = 0U; i < RK_NAME_SIZE; i++)
    {
        offset = kTraceFramePutU8_(bufPtr, offset, (BYTE)namePtr[i]);
    }
    return (offset);
}

static UINT kTraceFramePutActorName_(BYTE *const bufPtr, UINT const offset,
                                     RK_PID const pid)
{
    return (kTraceFramePutName_(bufPtr, offset, kTraceActorName_(pid)));
}

static UINT kTraceFrameChecksum_(BYTE const *const bufPtr, UINT const len)
{
    UINT sum = 0U;

    for (UINT i = 0U; i < len; i++)
    {
        sum = (sum + (UINT)bufPtr[i]) & 0xFFFFU;
    }
    return (sum);
}

static VOID kTraceFrameLinePrint_(BYTE const *const bufPtr, UINT const len)
{
    printf("\r\nKTRACE_FRAME ");
    for (UINT i = 0U; i < len; i++)
    {
        printf("%02x", (unsigned int)bufPtr[i]);
    }
    printf("\r\n");
}

static RK_BOOL kTraceOverflowFrameBuild_(
    RK_TRACE_OVERFLOW_INFO const *const infoPtr, BYTE *const framePtr,
    UINT *const lenPtr)
{
    UINT offset = 0U;
    UINT checksum = 0U;

    if ((infoPtr == NULL) || (framePtr == NULL) || (lenPtr == NULL))
    {
        return (RK_FALSE);
    }

    offset = kTraceFramePutU8_(framePtr, offset, RK_TRACE_FRAME_MAGIC_0);
    offset = kTraceFramePutU8_(framePtr, offset, RK_TRACE_FRAME_MAGIC_1);
    offset = kTraceFramePutU8_(framePtr, offset, RK_TRACE_FRAME_MAGIC_2);
    offset = kTraceFramePutU8_(framePtr, offset, RK_TRACE_FRAME_MAGIC_3);
    offset = kTraceFramePutU8_(framePtr, offset, RK_TRACE_FRAME_VERSION);
    offset = kTraceFramePutU8_(framePtr, offset, (BYTE)infoPtr->kind);
    offset = kTraceFramePutU16_(framePtr, offset, 0U);
    offset = kTraceFramePutU32_(framePtr, offset, infoPtr->sequence);
    offset = kTraceFramePutU32_(framePtr, offset, infoPtr->dropped);
    offset =
        kTraceFramePutU32_(framePtr, offset, (ULONG)infoPtr->subjectPtr);
    offset = kTraceFramePutName_(framePtr, offset, infoPtr->name);

    if (infoPtr->kind == RK_TRACE_OVERFLOW_OBJECT)
    {
        RK_TRACE_RECORD_INFO const *const recordPtr = &infoPtr->objectRecord;
        offset = kTraceFramePutU32_(framePtr, offset, infoPtr->objID);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->tick);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->actorCycle);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->value);
        offset =
            kTraceFramePutU16_(framePtr, offset, (UINT)recordPtr->result);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->op);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->actorPid);
        offset =
            kTraceFramePutActorName_(framePtr, offset, recordPtr->actorPid);
    }
    else if (infoPtr->kind == RK_TRACE_OVERFLOW_TASK_PRIO)
    {
        RK_TRACE_PRIO_RECORD_INFO const *const recordPtr =
            &infoPtr->prioRecord;
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->tick);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->actorCycle);
        offset = kTraceFramePutU8_(framePtr, offset, infoPtr->pid);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->actorPid);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->oldPriority);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->newPriority);
        offset =
            kTraceFramePutU8_(framePtr, offset, recordPtr->nominalPriority);
        offset =
            kTraceFramePutActorName_(framePtr, offset, recordPtr->actorPid);
    }
    else if (infoPtr->kind == RK_TRACE_OVERFLOW_TASK_OVERRUN)
    {
        RK_TRACE_OVERRUN_RECORD_INFO const *const recordPtr =
            &infoPtr->overrunRecord;
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->tick);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->actorCycle);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->actorPid);
        offset = kTraceFramePutU8_(framePtr, offset, recordPtr->overrunKind);
        offset =
            kTraceFramePutActorName_(framePtr, offset, recordPtr->actorPid);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->period);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->lateBy);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->skipped);
        offset = kTraceFramePutU32_(framePtr, offset, recordPtr->total);
    }
    else
    {
        return (RK_FALSE);
    }

    kTraceFramePutU16_(framePtr, 6U, offset + 2U);
    checksum = kTraceFrameChecksum_(framePtr, offset);
    offset = kTraceFramePutU16_(framePtr, offset, checksum);
    *lenPtr = offset;
    return (RK_TRUE);
}

#if (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U)
static VOID kTraceFrameBufferStore_(BYTE const *const framePtr, UINT const len)
{
    if ((framePtr == NULL) || (len == 0U) || (len > RK_TRACE_FRAME_MAX_LEN))
    {
        return;
    }

    if (traceFrameCount == RK_CONF_TRACE_FRAME_BUFFER_DEPTH)
    {
        traceFrameDropped++;
    }
    else
    {
        traceFrameCount++;
    }

    for (UINT i = 0U; i < len; i++)
    {
        traceFrameBuffer[traceFrameHead][i] = framePtr[i];
    }
    traceFrameLen[traceFrameHead] = (BYTE)len;
    traceFrameHead =
        (traceFrameHead + 1U) % RK_CONF_TRACE_FRAME_BUFFER_DEPTH;
}

static VOID kTraceFrameBufferPrintAndClear_(VOID)
{
    UINT start = 0U;
    UINT count = traceFrameCount;

    if (count == RK_CONF_TRACE_FRAME_BUFFER_DEPTH)
    {
        start = traceFrameHead;
    }

    printf("\r\nKTRACE_DUMP frames=%u dropped=%lu\r\n",
           count, traceFrameDropped);
    for (UINT i = 0U; i < count; i++)
    {
        UINT const idx = (start + i) % RK_CONF_TRACE_FRAME_BUFFER_DEPTH;
        kTraceFrameLinePrint_(traceFrameBuffer[idx], traceFrameLen[idx]);
    }

    traceFrameHead = 0U;
    traceFrameCount = 0U;
    traceFrameDropped = 0UL;
}
#endif
#endif

VOID RK_FUNC_WEAK kTraceOverflowPersist(
    RK_TRACE_OVERFLOW_INFO const *const infoPtr)
{
    if (infoPtr == NULL)
    {
        return;
    }

#if ((RK_CONF_TRACE_FRAME_STDOUT == ON) ||                                  \
     (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U))
    BYTE frame[RK_TRACE_FRAME_MAX_LEN];
    UINT len = 0U;

    if (kTraceOverflowFrameBuild_(infoPtr, frame, &len) == RK_FALSE)
    {
        return;
    }
#if (RK_CONF_TRACE_FRAME_STDOUT == ON)
    kTraceFrameLinePrint_(frame, len);
#elif (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U)
    kTraceFrameBufferStore_(frame, len);
#endif
#else
    K_UNUSE(infoPtr);
#endif
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

#if (RK_CONF_MESG_QUEUE == ON)
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
    return (RK_FALSE);
}
#endif

#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
static RK_BOOL kTraceSyncInfoFromSlot_(
    RK_TRACE_OBJECT_SLOT const *const slotPtr,
    RK_TRACE_SYNC_INFO *const outPtr)
{
    if ((slotPtr == NULL) || (outPtr == NULL))
    {
        return (RK_FALSE);
    }

#if (RK_CONF_SEMAPHORE == ON)
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
        outPtr->protocol = 0U;
        outPtr->waitingTasks = objPtr->waitingQueue.size;
        return (RK_TRUE);
    }
#endif
#if (RK_CONF_MUTEX == ON)
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
        outPtr->protocol = objPtr->protocol;
        outPtr->waitingTasks = objPtr->waitingQueue.size;
        return (RK_TRUE);
    }
#endif
    return (RK_FALSE);
}
#endif

#if (RK_CONF_CALLOUT_TIMER == ON)
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
#endif

#if (RK_TRACE_HAS_IPC == ON)
static RK_BOOL kTraceTaskIsValid_(RK_TCB const *const taskPtr)
{
    return (((taskPtr != NULL) && (taskPtr->init == RK_TRUE) &&
             (taskPtr->pid < RK_NTHREADS))
                ? RK_TRUE
                : RK_FALSE);
}

static VOID kTraceIpcRowInit_(RK_TRACE_IPC_ROW *const rowPtr)
{
    if (rowPtr == NULL)
    {
        return;
    }

    rowPtr->valid = RK_FALSE;
    rowPtr->pid = UINT8_MAX;
    kTraceNameCopy_(rowPtr->taskName, "-");
    rowPtr->status = RK_TCB_INITIALISED;
    rowPtr->ipcNamePtr = "-";
    kTraceNameCopy_(rowPtr->serverName, "-");
    rowPtr->serverValid = RK_FALSE;
    rowPtr->serverPriority = 0U;
    rowPtr->serverNominal = 0U;
    kTraceNameCopy_(rowPtr->peerName, "-");
    rowPtr->peerValid = RK_FALSE;
    rowPtr->taskPriority = 0U;
    rowPtr->taskNominal = 0U;
    rowPtr->stateNamePtr = "-";
    rowPtr->bytes = 0UL;
    rowPtr->callWaiters = 0UL;
    rowPtr->acceptWaiters = 0UL;
    rowPtr->sendWaiters = 0UL;
    rowPtr->active = 0U;
}

static VOID kTraceIpcRowTaskSet_(RK_TRACE_IPC_ROW *const rowPtr,
                                 RK_TCB const *const taskPtr)
{
    if ((rowPtr == NULL) || (kTraceTaskIsValid_(taskPtr) == RK_FALSE))
    {
        return;
    }

    rowPtr->valid = RK_TRUE;
    rowPtr->pid = taskPtr->pid;
    kTraceNameCopy_(rowPtr->taskName, taskPtr->taskName);
    rowPtr->status = taskPtr->status;
    rowPtr->taskPriority = taskPtr->priority;
    rowPtr->taskNominal = taskPtr->prioNominal;
}

static VOID kTraceIpcRowServerSet_(RK_TRACE_IPC_ROW *const rowPtr,
                                   RK_TCB const *const serverPtr)
{
    if ((rowPtr == NULL) || (kTraceTaskIsValid_(serverPtr) == RK_FALSE))
    {
        return;
    }

    rowPtr->serverValid = RK_TRUE;
    kTraceNameCopy_(rowPtr->serverName, serverPtr->taskName);
    rowPtr->serverPriority = serverPtr->priority;
    rowPtr->serverNominal = serverPtr->prioNominal;
}

static VOID kTraceIpcRowPeerSet_(RK_TRACE_IPC_ROW *const rowPtr,
                                 RK_TCB const *const peerPtr)
{
    if ((rowPtr == NULL) || (kTraceTaskIsValid_(peerPtr) == RK_FALSE))
    {
        return;
    }

    rowPtr->peerValid = RK_TRUE;
    kTraceNameCopy_(rowPtr->peerName, peerPtr->taskName);
}

#if (RK_CONF_CHANNEL == ON)
static CHAR const *kTraceCallStateName_(RK_CALL_STATE const state)
{
    switch (state)
    {
        case RK_CALL_IDLE:
            return ("idle");
        case RK_CALL_QUEUED:
            return ("queued");
        case RK_CALL_ACTIVE:
            return ("active");
        case RK_CALL_ABANDONED:
            return ("abandon");
        default:
            return ("?");
    }
}

static RK_TCB const *kTraceChannelPeekCaller_(RK_TCB const *const serverPtr)
{
    RK_NODE const *nodePtr = NULL;

    if (serverPtr == NULL)
    {
        return (NULL);
    }

    nodePtr = serverPtr->channelCallers.listDummy.nextPtr;
    while (nodePtr != &serverPtr->channelCallers.listDummy)
    {
        RK_TCB const *callerPtr = K_GET_TCB_ADDR(nodePtr);

        if ((callerPtr != NULL) &&
            (callerPtr->channelServerPtr == serverPtr) &&
            (callerPtr->channelState == RK_CALL_QUEUED))
        {
            return (callerPtr);
        }

        nodePtr = nodePtr->nextPtr;
        RK_BARRIER
    }

    return (NULL);
}

static RK_BOOL kTraceFillChannelCallerRow_(
    RK_TCB const *const taskPtr,
    RK_TRACE_IPC_ROW *const rowPtr)
{
    if ((rowPtr == NULL) || (kTraceTaskIsValid_(taskPtr) == RK_FALSE) ||
        ((taskPtr->channelServerPtr == NULL) &&
         (taskPtr->channelState == RK_CALL_IDLE)))
    {
        return (RK_FALSE);
    }

    kTraceIpcRowInit_(rowPtr);
    kTraceIpcRowTaskSet_(rowPtr, taskPtr);
    rowPtr->ipcNamePtr = "chan-call";
    rowPtr->stateNamePtr = kTraceCallStateName_(taskPtr->channelState);
    rowPtr->bytes = taskPtr->channelReqSize;
    kTraceIpcRowServerSet_(rowPtr, taskPtr->channelServerPtr);

    if (taskPtr->channelServerPtr != NULL)
    {
        RK_TCB const *const serverPtr = taskPtr->channelServerPtr;
        rowPtr->callWaiters = serverPtr->channelCallers.size;
        rowPtr->acceptWaiters = serverPtr->channelAcceptWaiters.size;
        rowPtr->active =
            (serverPtr->channelActiveCallerPtr == taskPtr) ? 1U : 0U;
    }

    return (RK_TRUE);
}

static RK_BOOL kTraceFillChannelServerRow_(
    RK_TCB const *const taskPtr,
    RK_TRACE_IPC_ROW *const rowPtr)
{
    RK_TCB const *callerPtr = NULL;

    if ((rowPtr == NULL) || (kTraceTaskIsValid_(taskPtr) == RK_FALSE) ||
        ((taskPtr->channelActiveCallerPtr == NULL) &&
         (taskPtr->channelCallers.size == 0UL) &&
         (taskPtr->channelAcceptWaiters.size == 0UL)))
    {
        return (RK_FALSE);
    }

    kTraceIpcRowInit_(rowPtr);
    kTraceIpcRowTaskSet_(rowPtr, taskPtr);
    kTraceIpcRowServerSet_(rowPtr, taskPtr);
    rowPtr->ipcNamePtr = "chan-srv";
    rowPtr->callWaiters = taskPtr->channelCallers.size;
    rowPtr->acceptWaiters = taskPtr->channelAcceptWaiters.size;

    callerPtr = taskPtr->channelActiveCallerPtr;
    if (callerPtr != NULL)
    {
        rowPtr->stateNamePtr = "active";
        rowPtr->bytes = callerPtr->channelReqSize;
        rowPtr->active = 1U;
        kTraceIpcRowPeerSet_(rowPtr, callerPtr);
        return (RK_TRUE);
    }

    callerPtr = kTraceChannelPeekCaller_(taskPtr);
    if (callerPtr != NULL)
    {
        rowPtr->stateNamePtr = "queued";
        rowPtr->bytes = callerPtr->channelReqSize;
        kTraceIpcRowPeerSet_(rowPtr, callerPtr);
        return (RK_TRUE);
    }

    rowPtr->stateNamePtr =
        (taskPtr->channelAcceptWaiters.size > 0UL) ? "accept" : "idle";
    return (RK_TRUE);
}
#endif

#if (RK_CONF_RENDEZVOUS == ON)
static RK_BOOL kTraceFillRendezvousSenderRow_(
    RK_TCB const *const taskPtr,
    RK_TRACE_IPC_ROW *const rowPtr)
{
    RK_TCB const *receiverPtr = NULL;

    if ((rowPtr == NULL) || (kTraceTaskIsValid_(taskPtr) == RK_FALSE) ||
        ((taskPtr->rendezvousReceiverPtr == NULL) &&
         (taskPtr->rendezvousMesgPtr == NULL)))
    {
        return (RK_FALSE);
    }

    receiverPtr = taskPtr->rendezvousReceiverPtr;
    kTraceIpcRowInit_(rowPtr);
    kTraceIpcRowTaskSet_(rowPtr, taskPtr);
    kTraceIpcRowServerSet_(rowPtr, receiverPtr);
    rowPtr->ipcNamePtr = "rdvz-send";
    rowPtr->stateNamePtr = "queued";

    if (receiverPtr != NULL)
    {
        rowPtr->bytes = receiverPtr->rendezvousMesgBytes;
        rowPtr->sendWaiters = receiverPtr->rendezvousSenders.size;
        if (receiverPtr->rendezvousPendingSenderPtr == taskPtr)
        {
            rowPtr->stateNamePtr = "pending";
            rowPtr->active = 1U;
        }
    }

    return (RK_TRUE);
}

static RK_BOOL kTraceFillRendezvousReceiverRow_(
    RK_TCB const *const taskPtr,
    RK_TRACE_IPC_ROW *const rowPtr)
{
    if ((rowPtr == NULL) || (kTraceTaskIsValid_(taskPtr) == RK_FALSE) ||
        (taskPtr->rendezvousMesgBytes == 0UL))
    {
        return (RK_FALSE);
    }

    kTraceIpcRowInit_(rowPtr);
    kTraceIpcRowTaskSet_(rowPtr, taskPtr);
    kTraceIpcRowServerSet_(rowPtr, taskPtr);
    rowPtr->ipcNamePtr = "rdvz-recv";
    rowPtr->bytes = taskPtr->rendezvousMesgBytes;
    rowPtr->sendWaiters = taskPtr->rendezvousSenders.size;

    if (taskPtr->rendezvousPendingSenderPtr != NULL)
    {
        rowPtr->stateNamePtr = "pending";
        rowPtr->active = 1U;
        kTraceIpcRowPeerSet_(rowPtr, taskPtr->rendezvousPendingSenderPtr);
    }
    else if (taskPtr->rendezvousRecvBufPtr != NULL)
    {
        rowPtr->stateNamePtr = "recvwait";
    }
    else if (taskPtr->rendezvousSenders.size > 0UL)
    {
        RK_TCB const *const senderPtr =
            K_GET_TCB_ADDR(taskPtr->rendezvousSenders.listDummy.nextPtr);
        rowPtr->stateNamePtr = "queued";
        kTraceIpcRowPeerSet_(rowPtr, senderPtr);
    }
    else
    {
        rowPtr->stateNamePtr = "idle";
    }

    return (RK_TRUE);
}
#endif

static VOID kTracePrintKipcHeader_(VOID)
{
    printf("\r\nPID TASK     ST     IPC       SERVER   SVEF SVNOM PEER     TPR TNOM STATE    BYTES CALLQ ACCQ SENDQ ACT\r\n");
}

static VOID kTracePrintKipcRow_(RK_TRACE_IPC_ROW const *const rowPtr)
{
    if ((rowPtr == NULL) || (rowPtr->valid == RK_FALSE))
    {
        return;
    }

    if (rowPtr->serverValid == RK_TRUE)
    {
        printf("%3u %-8s %-6s %-9s %-8s %4u %5u %-8s %3u %4u %-8s %5lu %5lu %4lu %5lu %3u\r\n",
               rowPtr->pid, rowPtr->taskName, kTraceStatusName_(rowPtr->status),
               rowPtr->ipcNamePtr, rowPtr->serverName,
               rowPtr->serverPriority, rowPtr->serverNominal,
               rowPtr->peerName, rowPtr->taskPriority,
               rowPtr->taskNominal, rowPtr->stateNamePtr,
               rowPtr->bytes, rowPtr->callWaiters,
               rowPtr->acceptWaiters, rowPtr->sendWaiters,
               rowPtr->active);
    }
    else
    {
        printf("%3u %-8s %-6s %-9s %-8s %4s %5s %-8s %3u %4u %-8s %5lu %5lu %4lu %5lu %3u\r\n",
               rowPtr->pid, rowPtr->taskName, kTraceStatusName_(rowPtr->status),
               rowPtr->ipcNamePtr, rowPtr->serverName, "-", "-",
               rowPtr->peerName, rowPtr->taskPriority,
               rowPtr->taskNominal, rowPtr->stateNamePtr,
               rowPtr->bytes, rowPtr->callWaiters,
               rowPtr->acceptWaiters, rowPtr->sendWaiters,
               rowPtr->active);
    }
}

static VOID kTracePrintKipc_(VOID)
{
    RK_BOOL any = RK_FALSE;

    kTracePrintKipcHeader_();

    for (UINT i = 0U; i < RK_NTHREADS; i++)
    {
#if (RK_CONF_CHANNEL == ON)
        RK_TRACE_IPC_ROW channelCallerRow;
        RK_TRACE_IPC_ROW channelServerRow;
        RK_BOOL channelCallerValid = RK_FALSE;
        RK_BOOL channelServerValid = RK_FALSE;
#endif
#if (RK_CONF_RENDEZVOUS == ON)
        RK_TRACE_IPC_ROW rdvzSenderRow;
        RK_TRACE_IPC_ROW rdvzReceiverRow;
        RK_BOOL rdvzSenderValid = RK_FALSE;
        RK_BOOL rdvzReceiverValid = RK_FALSE;
#endif

        RK_CR_AREA
        RK_CR_ENTER
        RK_TCB const *taskPtr = &RK_gTcbs[i];
#if (RK_CONF_CHANNEL == ON)
        channelCallerValid =
            kTraceFillChannelCallerRow_(taskPtr, &channelCallerRow);
        channelServerValid =
            kTraceFillChannelServerRow_(taskPtr, &channelServerRow);
#endif
#if (RK_CONF_RENDEZVOUS == ON)
        rdvzSenderValid =
            kTraceFillRendezvousSenderRow_(taskPtr, &rdvzSenderRow);
        rdvzReceiverValid =
            kTraceFillRendezvousReceiverRow_(taskPtr, &rdvzReceiverRow);
#endif
        RK_CR_EXIT

#if (RK_CONF_CHANNEL == ON)
        if (channelCallerValid == RK_TRUE)
        {
            kTracePrintKipcRow_(&channelCallerRow);
            any = RK_TRUE;
        }
        if (channelServerValid == RK_TRUE)
        {
            kTracePrintKipcRow_(&channelServerRow);
            any = RK_TRUE;
        }
#endif
#if (RK_CONF_RENDEZVOUS == ON)
        if (rdvzSenderValid == RK_TRUE)
        {
            kTracePrintKipcRow_(&rdvzSenderRow);
            any = RK_TRUE;
        }
        if (rdvzReceiverValid == RK_TRUE)
        {
            kTracePrintKipcRow_(&rdvzReceiverRow);
            any = RK_TRUE;
        }
#endif
    }

    if (any == RK_FALSE)
    {
        printf("(no task-backed IPC state)\r\n");
    }
}
#endif

static VOID kTracePrintHelp_(VOID)
{
    printf("\r\nktrace commands:\r\n");
    printf("  top\r\n");
    printf("  list kobjects\r\n");
#if (RK_CONF_MESG_QUEUE == ON)
    printf("  list kmesg\r\n");
#endif
#if (RK_TRACE_HAS_IPC == ON)
    printf("  list kipc\r\n");
#endif
#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
    printf("  list ksema\r\n");
#endif
    printf("  list kmem\r\n");
#if (RK_CONF_SLEEP_QUEUE == ON)
    printf("  list ksleepq\r\n");
#endif
#if (RK_CONF_MRM == ON)
    printf("  list kmrm\r\n");
#endif
#if (RK_CONF_CALLOUT_TIMER == ON)
    printf("  list ktimers\r\n");
    printf("  list ktimerq\r\n");
#endif
    printf("  hist [object-name|task/name|task/pid]\r\n");
    printf("  history [object-name|task/name|task/pid]\r\n");
#if (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U)
    printf("  dump [frames]\r\n");
#endif
    printf("  help\r\n");
}

static VOID kTracePrintTop_(VOID)
{
    printf("\r\nPID NAME     ST     PRIO NOM RUNS  PCHG OVR   CPU%% TICKS OWNMTX STACK     FIRST    LAST     LOWSP    EVCUR    EVREQ    EVOP\r\n");
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
            info.overrunCount = taskPtr->overrunCount;
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
            RK_STACK const firstUsedIndex = kTraceStackFirstUsedIndex_(taskPtr);
            info.stackFreeWords = kTraceStackFreeFromIndex_(firstUsedIndex);
            info.stackSizeWords = taskPtr->stackSize;
            info.stackFirstPtr = kTraceStackWordPtr_(taskPtr, 0U);
            info.stackLastPtr = kTraceStackLastPtr_(taskPtr);
            info.stackLowWaterPtr = kTraceStackWordPtr_(taskPtr, firstUsedIndex);
            valid = RK_TRUE;
        }
        RK_CR_EXIT

        if (valid == RK_FALSE)
        {
            continue;
        }

        printf("%3u %-8s %-6s %4u %3u %5lu %4lu %5lu %3u %5lu %6lu %4u/%-4u %08lx %08lx %08lx %08lx %08lx %-4s\r\n",
               info.pid, info.name, kTraceStatusName_(info.status),
               info.priority, info.prioNominal, info.runCnt,
               info.prioChanges, info.overrunCount, info.cpuPct,
               info.cpuTicks, info.ownedMutexes, info.stackFreeWords,
               info.stackSizeWords,
               (unsigned long)info.stackFirstPtr,
               (unsigned long)info.stackLastPtr,
               (unsigned long)info.stackLowWaterPtr,
               (unsigned long)info.eventCurr, (unsigned long)info.eventReq,
               kTraceEventOptName_(info.eventOpt));
    }
}

#if (RK_CONF_MESG_QUEUE == ON)
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
#endif

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

#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
static VOID kTracePrintKsema_(VOID)
{
    printf("\r\nTYPE  NAME     OWNER    LOCK VAL MAX PR WAIT\r\n");
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
               info.maxValue, info.protocol, info.waitingTasks);
    }
}
#endif

#if (RK_CONF_SLEEP_QUEUE == ON)
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
#endif

#if (RK_CONF_MRM == ON)
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
#endif

#if (RK_CONF_CALLOUT_TIMER == ON)
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
#endif

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
    printf("TICK     CYCLE    TASK     OP       RET VAL\r\n");

    for (UINT i = 0U; i < count; i++)
    {
        RK_TRACE_RECORD_INFO record;
        RK_CR_ENTER
        UINT const idx = (start + i) % RK_CONF_TRACE_RECORD_DEPTH;
        record = slotPtr->records[idx];
        RK_CR_EXIT

        printf("%8lu %8lu %-8s %-8s %4d %lu\r\n",
               record.tick, record.actorCycle,
               kTraceActorName_(record.actorPid),
               kTraceOpName_((RK_TRACE_OP)record.op),
               record.result, record.value);
    }
}

#if (RK_CONF_TRACE_TASK_PRIO_HISTORY == ON)
static VOID kTracePrintTaskPrioHist_(CHAR const *taskNamePtr)
{
    RK_TRACE_PRIO_RECORD_INFO records[RK_CONF_TRACE_RECORD_DEPTH];
    RK_TCB const *taskPtr = NULL;
    UINT count = 0U;
    CHAR name[RK_OBJ_MAX_NAME_LEN];

    name[0] = '\0';
    RK_CR_AREA
    RK_CR_ENTER
    taskPtr = kTraceFindTaskByNameOrPid_(taskNamePtr);
    if (taskPtr != NULL)
    {
        RK_PID const pid = taskPtr->pid;
        kTraceNameCopy_(name, taskPtr->taskName);
        UINT const toCopy = tracePrioRecordCount[pid];
        UINT start = 0U;
        if (tracePrioRecordCount[pid] == RK_CONF_TRACE_RECORD_DEPTH)
        {
            start = tracePrioRecordHead[pid];
        }
        for (UINT i = 0U; i < toCopy; i++)
        {
            UINT const idx = (start + i) % RK_CONF_TRACE_RECORD_DEPTH;
            records[i] = tracePrioRecords[pid][idx];
        }
        count = toCopy;
    }
    RK_CR_EXIT

    if (taskPtr == NULL)
    {
        printf("\r\nktrace: task '%s' not found\r\n", taskNamePtr);
        return;
    }

    printf("\r\nhistory task/%s\r\n", name);
    printf("TICK     CYCLE    ACTOR    REASON   OLD NEW NOM\r\n");

    for (UINT i = 0U; i < count; i++)
    {
        RK_TRACE_PRIO_RECORD_INFO const *const recordPtr = &records[i];
        printf("%8lu %8lu %-8s %-8s %3u %3u %3u\r\n",
               recordPtr->tick, recordPtr->actorCycle,
               kTraceActorName_(recordPtr->actorPid), "prio",
               recordPtr->oldPriority, recordPtr->newPriority,
               recordPtr->nominalPriority);
    }
}
#else
static VOID kTracePrintTaskPrioHist_(CHAR const *taskNamePtr)
{
    K_UNUSE(taskNamePtr);
    printf("\r\nktrace: task priority history disabled\r\n");
}
#endif

static VOID kTracePrintHist_(CHAR const *namePtr)
{
    namePtr = kTraceSkipSpaces_(namePtr);
    if ((namePtr != NULL) && (namePtr[0] != '\0'))
    {
        RK_TRACE_OBJECT_SLOT const *slotPtr = NULL;
        if (kTraceStrStarts_(namePtr, "task/") == RK_TRUE)
        {
            kTracePrintTaskPrioHist_(namePtr + 5);
            return;
        }
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
    linePtr = kTraceSkipSpaces_(linePtr);

    if ((kTraceStrEq_(linePtr, "source") == RK_TRUE) ||
        (kTraceStrStarts_(linePtr, "source ") == RK_TRUE))
    {
        printf("\r\nktrace> ");
        return;
    }

    if (kTraceStrEq_(linePtr, "top") == RK_TRUE)
    {
        kTracePrintTop_();
    }
    else if (kTraceStrEq_(linePtr, "list kobjects") == RK_TRUE)
    {
        kTracePrintKobjects_();
    }
#if (RK_CONF_MESG_QUEUE == ON)
    else if (kTraceStrEq_(linePtr, "list kmesg") == RK_TRUE)
    {
        kTracePrintKmesg_();
    }
#endif
#if (RK_TRACE_HAS_IPC == ON)
    else if (kTraceStrEq_(linePtr, "list kipc") == RK_TRUE)
    {
        kTracePrintKipc_();
    }
#endif
#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
    else if (kTraceStrEq_(linePtr, "list ksema") == RK_TRUE)
    {
        kTracePrintKsema_();
    }
#endif
    else if (kTraceStrEq_(linePtr, "list kmem") == RK_TRUE)
    {
        kTracePrintKmem_();
    }
#if (RK_CONF_SLEEP_QUEUE == ON)
    else if (kTraceStrEq_(linePtr, "list ksleepq") == RK_TRUE)
    {
        kTracePrintKsleepq_();
    }
#endif
#if (RK_CONF_MRM == ON)
    else if (kTraceStrEq_(linePtr, "list kmrm") == RK_TRUE)
    {
        kTracePrintKmrm_();
    }
#endif
#if (RK_CONF_CALLOUT_TIMER == ON)
    else if (kTraceStrEq_(linePtr, "list ktimers") == RK_TRUE)
    {
        kTracePrintKtimers_();
    }
    else if (kTraceStrEq_(linePtr, "list ktimerq") == RK_TRUE)
    {
        kTracePrintKtimerq_();
    }
#endif
    else if (kTraceStrStarts_(linePtr, "history") == RK_TRUE)
    {
        kTracePrintHist_(linePtr + 7);
    }
    else if (kTraceStrStarts_(linePtr, "hist") == RK_TRUE)
    {
        kTracePrintHist_(linePtr + 4);
    }
    else if ((kTraceStrEq_(linePtr, "dump") == RK_TRUE) ||
             (kTraceStrEq_(linePtr, "dump frames") == RK_TRUE))
    {
#if (RK_CONF_TRACE_FRAME_BUFFER_DEPTH > 0U)
        kTraceFrameBufferPrintAndClear_();
#else
        printf("\r\nktrace: frame buffer disabled\r\n");
#endif
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
        kTraceOverflowDrain_();
        kTracePoll();
        kEventGet((RK_TRACE_RX_EVENT | RK_TRACE_OVERFLOW_EVENT), RK_EVENT_ANY,
                  NULL, RK_WAIT_FOREVER);
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
