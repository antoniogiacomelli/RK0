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

#ifndef RK_TRACE_H
#define RK_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kcommondefs.h>

#if (RK_CONF_TRACE == ON)

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
    RK_TRACE_OP_EXPIRE
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
    RK_STACK stackFreeWords;
    RK_STACK stackSizeWords;
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
    UINT prioInh;
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
    ULONG value;
    SHORT result;
    BYTE op;
    RK_PID actorPid;
} RK_TRACE_RECORD_INFO;

RK_ERR kTraceInit(VOID);
VOID kTracePoll(VOID);
VOID kTraceInputSignalFromISR(VOID);
RK_ERR kTraceObjectNameSet(VOID *const objPtr, CHAR const *const namePtr);
VOID kTraceRecordObject(VOID *const objPtr, RK_TRACE_OP const op,
                        RK_ERR const result, ULONG const value);
VOID kTraceRecordTaskPrio(RK_TASK_HANDLE const taskHandle,
                          RK_PRIO const oldPriority,
                          RK_PRIO const newPriority);

UINT kTraceTaskSnapshot(RK_TRACE_TASK_INFO *const infoPtr, UINT const maxInfo);
UINT kTraceMesgSnapshot(RK_TRACE_OBJECT_INFO *const infoPtr,
                        UINT const maxInfo);
UINT kTraceSemaSnapshot(RK_TRACE_SYNC_INFO *const infoPtr, UINT const maxInfo);
UINT kTraceTimerSnapshot(RK_TRACE_TIMER_INFO *const infoPtr,
                         UINT const maxInfo);
UINT kTraceRecordSnapshot(VOID *const objPtr,
                          RK_TRACE_RECORD_INFO *const infoPtr,
                          UINT const maxInfo);
VOID kTraceTick(VOID);
VOID kTraceRegisterObject(VOID *const objPtr, RK_ID const objID);

INT kTraceUartGetc(CHAR *const chPtr);
VOID kTraceUartRxEnable(VOID);

#define kTraceNameObject(OBJ_PTR, NAME_PTR)                                    \
    kTraceObjectNameSet((VOID *)(OBJ_PTR), (NAME_PTR))

#else

#define kTraceInit() (RK_ERR_SUCCESS)
#define kTracePoll() do { } while (0)
#define kTraceInputSignalFromISR() do { } while (0)
#define kTraceObjectNameSet(OBJ_PTR, NAME_PTR) (RK_ERR_SUCCESS)
#define kTraceRecordObject(OBJ_PTR, OP, RESULT, VALUE) do { } while (0)
#define kTraceRecordTaskPrio(TASK_HANDLE, OLD_PRIORITY, NEW_PRIORITY) do { } while (0)
#define kTraceTick() do { } while (0)
#define kTraceRegisterObject(OBJ_PTR, OBJ_ID) do { } while (0)
#define kTraceNameObject(OBJ_PTR, NAME_PTR) (RK_ERR_SUCCESS)

#endif /* RK_CONF_TRACE */

#ifdef __cplusplus
}
#endif

#endif /* RK_TRACE_H */
