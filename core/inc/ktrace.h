/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.50.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_TRACE_H
#define RK_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kcommondefs.h>

typedef enum
{
    RK_TRACE_OP_INIT = 1U,
    RK_TRACE_OP_NAME,
    RK_TRACE_OP_QUERY,
    RK_TRACE_OP_ALLOC,
    RK_TRACE_OP_FREE,
    RK_TRACE_OP_SEND,
    RK_TRACE_OP_RECV,
    RK_TRACE_OP_JAM,
    RK_TRACE_OP_POST,
    RK_TRACE_OP_PEND,
    RK_TRACE_OP_BLOCK,
    RK_TRACE_OP_WAKE,
    RK_TRACE_OP_TIMEOUT,
    RK_TRACE_OP_RESET,
    RK_TRACE_OP_LOCK,
    RK_TRACE_OP_UNLOCK,
    RK_TRACE_OP_CALL,
    RK_TRACE_OP_ACCEPT,
    RK_TRACE_OP_DONE,
    RK_TRACE_OP_RESERVE,
    RK_TRACE_OP_PUBLISH,
    RK_TRACE_OP_GET,
    RK_TRACE_OP_UNGET,
    RK_TRACE_OP_CANCEL,
    RK_TRACE_OP_RELOAD,
    RK_TRACE_OP_EXPIRE,
    RK_TRACE_OP_SEND_BLOCK,
    RK_TRACE_OP_RECV_BLOCK,
    RK_TRACE_OP_JAM_BLOCK,
    RK_TRACE_OP_PEND_BLOCK,
    RK_TRACE_OP_LOCK_BLOCK,
    RK_TRACE_OP_WAIT,
    RK_TRACE_OP_WAIT_BLOCK,
    RK_TRACE_OP_OVERRUN
} RK_TRACE_OP;

typedef struct
{
    RK_TASK_HANDLE taskHandle;
    RK_PID pid;
    CHAR name[RK_OBJ_MAX_NAME_LEN];
    RK_TASK_STATUS status;
    RK_PRIO priority;
    RK_PRIO prioNominal;
    ULONG runCnt;
    ULONG prioChanges;
    ULONG ownedMutexes;
    RK_EVENT_FLAG eventCurr;
    RK_EVENT_FLAG eventReq;
    RK_OPTION eventOpt;
    RK_TICK cpuTicks;
    UINT cpuPct;
    ULONG overrunCount;
    RK_STACK stackFreeWords;
    RK_STACK stackSizeWords;
    VOID const *stackFirstPtr;
    VOID const *stackLastPtr;
    VOID const *stackLowWaterPtr;
} RK_TRACE_TASK_INFO;

typedef struct
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
    VOID const *objPtr;
    CHAR ownerName[RK_NAME_SIZE];
    VOID const *ownerPtr;
    ULONG buffered;
    ULONG capacity;
    ULONG waitingSenders;
    ULONG waitingReceivers;
    ULONG waitingRequesters;
    UINT active;
} RK_TRACE_OBJECT_INFO;

typedef struct
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
    VOID const *objPtr;
    CHAR ownerName[RK_NAME_SIZE];
    VOID const *ownerPtr;
    UINT locked;
    UINT value;
    UINT maxValue;
    UINT protocol;
    ULONG waitingTasks;
} RK_TRACE_SYNC_INFO;

typedef struct
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
    VOID const *objPtr;
    UINT active;
    UINT reload;
    RK_TICK phase;
    RK_TICK period;
    RK_TICK remainingTicks;
    VOID const *argsPtr;
} RK_TRACE_TIMER_INFO;

typedef struct
{
    RK_TICK tick;
    ULONG actorCycle;
    ULONG value;
    SHORT result;
    BYTE op;
    RK_PID actorPid;
} RK_TRACE_RECORD_INFO;

typedef struct
{
    RK_TICK tick;
    ULONG actorCycle;
    RK_PID actorPid;
    RK_PRIO oldPriority;
    RK_PRIO newPriority;
    RK_PRIO nominalPriority;
} RK_TRACE_PRIO_RECORD_INFO;

typedef enum
{
    RK_TRACE_OVERFLOW_OBJECT = 1U,
    RK_TRACE_OVERFLOW_TASK_PRIO,
    RK_TRACE_OVERFLOW_TASK_OVERRUN
} RK_TRACE_OVERFLOW_KIND;

typedef enum
{
    RK_TRACE_OVERRUN_RELEASE = 1U,
    RK_TRACE_OVERRUN_UNTIL
} RK_TRACE_OVERRUN_KIND;

typedef struct
{
    RK_TICK tick;
    ULONG actorCycle;
    RK_PID actorPid;
    RK_TRACE_OVERRUN_KIND overrunKind;
    RK_TICK period;
    RK_TICK lateBy;
    ULONG skipped;
    ULONG total;
} RK_TRACE_OVERRUN_RECORD_INFO;

typedef struct
{
    RK_TRACE_OVERFLOW_KIND kind;
    ULONG sequence;
    RK_ID objID;
    RK_PID pid;
    CHAR name[RK_NAME_SIZE];
    VOID const *subjectPtr;
    ULONG dropped;
    RK_TRACE_RECORD_INFO objectRecord;
    RK_TRACE_PRIO_RECORD_INFO prioRecord;
    RK_TRACE_OVERRUN_RECORD_INFO overrunRecord;
} RK_TRACE_OVERFLOW_INFO;

#if (RK_CONF_TRACE == ON)

RK_ERR kTraceInit(VOID);
VOID kTracePoll(VOID);
VOID kTraceInputSignalFromISR(VOID);
RK_ERR kTraceObjectNameSet(VOID *const, CHAR const *const);
VOID kTraceRecordObject(VOID *const, RK_TRACE_OP const, RK_ERR const,
                        ULONG const);
VOID kTraceRecordTaskPrio(RK_TASK_HANDLE const, RK_PRIO const, RK_PRIO const);
VOID kTraceRecordTaskOverrun(RK_TRACE_OVERRUN_KIND const, RK_TICK const,
                             RK_TICK const, ULONG const);
VOID kTraceOverflowPersist(RK_TRACE_OVERFLOW_INFO const *const);

UINT kTraceTaskSnapshot(RK_TRACE_TASK_INFO *const, UINT const);
#if (RK_CONF_MESG_QUEUE == ON)
UINT kTraceMesgSnapshot(RK_TRACE_OBJECT_INFO *const, UINT const);
#endif
#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
UINT kTraceSemaSnapshot(RK_TRACE_SYNC_INFO *const, UINT const);
#endif
#if (RK_CONF_CALLOUT_TIMER == ON)
UINT kTraceTimerSnapshot(RK_TRACE_TIMER_INFO *const, UINT const);
#endif
UINT kTraceRecordSnapshot(VOID *const, RK_TRACE_RECORD_INFO *const, UINT const);
UINT kTraceTaskPrioSnapshot(RK_TASK_HANDLE const,
                            RK_TRACE_PRIO_RECORD_INFO *const, UINT const);
VOID kTraceTick(VOID);
VOID kTraceRegisterObject(VOID *const, RK_ID const);

INT kTraceUartGetc(CHAR *const);
VOID kTraceUartRxEnable(VOID);

#define kTraceNameObject(OBJ_PTR, NAME_PTR)                                    \
    kTraceObjectNameSet((VOID *)(OBJ_PTR), (NAME_PTR))

/* Trace OFF */
#else

#define RK_TRACE_INLINE_ RK_FORCE_INLINE static inline

RK_TRACE_INLINE_ RK_ERR kTraceInit(VOID)
{
    return (RK_ERR_SUCCESS);
}

RK_TRACE_INLINE_ VOID kTracePoll(VOID)
{
}

RK_TRACE_INLINE_ VOID kTraceInputSignalFromISR(VOID)
{
}

RK_TRACE_INLINE_ RK_ERR kTraceObjectNameSet(VOID *const objPtr,
                                            CHAR const *const namePtr)
{
    K_UNUSE(objPtr);
    K_UNUSE(namePtr);
    return (RK_ERR_SUCCESS);
}

RK_TRACE_INLINE_ VOID kTraceRecordObject(VOID *const objPtr,
                                         RK_TRACE_OP const op,
                                         RK_ERR const result,
                                         ULONG const value)
{
    K_UNUSE(objPtr);
    K_UNUSE(op);
    K_UNUSE(result);
    K_UNUSE(value);
}

RK_TRACE_INLINE_ VOID kTraceRecordTaskPrio(RK_TASK_HANDLE const taskHandle,
                                           RK_PRIO const oldPriority,
                                           RK_PRIO const newPriority)
{
    K_UNUSE(taskHandle);
    K_UNUSE(oldPriority);
    K_UNUSE(newPriority);
}

RK_TRACE_INLINE_ VOID kTraceRecordTaskOverrun(RK_TRACE_OVERRUN_KIND const kind,
                                              RK_TICK const period,
                                              RK_TICK const lateBy,
                                              ULONG const skipped)
{
    K_UNUSE(kind);
    K_UNUSE(period);
    K_UNUSE(lateBy);
    K_UNUSE(skipped);
}

RK_TRACE_INLINE_ VOID kTraceOverflowPersist(
    RK_TRACE_OVERFLOW_INFO const *const infoPtr)
{
    K_UNUSE(infoPtr);
}

RK_TRACE_INLINE_ UINT kTraceTaskSnapshot(RK_TRACE_TASK_INFO *const infoPtr,
                                         UINT const maxInfo)
{
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
}

RK_TRACE_INLINE_ UINT kTraceMesgSnapshot(RK_TRACE_OBJECT_INFO *const infoPtr,
                                         UINT const maxInfo)
{
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
}

RK_TRACE_INLINE_ UINT kTraceSemaSnapshot(RK_TRACE_SYNC_INFO *const infoPtr,
                                         UINT const maxInfo)
{
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
}

RK_TRACE_INLINE_ UINT kTraceTimerSnapshot(RK_TRACE_TIMER_INFO *const infoPtr,
                                          UINT const maxInfo)
{
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
}

RK_TRACE_INLINE_ UINT kTraceRecordSnapshot(VOID *const objPtr,
                                           RK_TRACE_RECORD_INFO *const infoPtr,
                                           UINT const maxInfo)
{
    K_UNUSE(objPtr);
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
}

RK_TRACE_INLINE_ UINT kTraceTaskPrioSnapshot(
    RK_TASK_HANDLE const taskHandle,
    RK_TRACE_PRIO_RECORD_INFO *const infoPtr,
    UINT const maxInfo)
{
    K_UNUSE(taskHandle);
    K_UNUSE(infoPtr);
    K_UNUSE(maxInfo);
    return (0U);
}

RK_TRACE_INLINE_ VOID kTraceTick(VOID)
{
}

RK_TRACE_INLINE_ VOID kTraceRegisterObject(VOID *const objPtr,
                                           RK_ID const objID)
{
    K_UNUSE(objPtr);
    K_UNUSE(objID);
}

RK_TRACE_INLINE_ INT kTraceUartGetc(CHAR *const chPtr)
{
    K_UNUSE(chPtr);
    return (0);
}

RK_TRACE_INLINE_ VOID kTraceUartRxEnable(VOID)
{
}

#define kTraceNameObject(OBJ_PTR, NAME_PTR)                                   \
    kTraceObjectNameSet((VOID *)(OBJ_PTR), (NAME_PTR))

#undef RK_TRACE_INLINE_

#endif /* RK_CONF_TRACE */

#ifdef __cplusplus
}
#endif

#endif /* RK_TRACE_H */
