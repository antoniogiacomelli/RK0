/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.19.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_OBJS_H
#define RK_OBJS_H

#ifdef __cplusplus
extern "C" {
#endif
struct RK_STRUCT_RING_BUFFER
{
    ULONG dataSize;
    ULONG maxBuf;
    ULONG nFull;
    ULONG *bufPtr;
    ULONG *writePtr;
    ULONG *readPtr;
    ULONG *bufEndPtr;
} K_ALIGN(4);
struct  RK_STRUCT_TIMEOUT_NODE
{
    struct RK_STRUCT_TIMEOUT_NODE *nextPtr;
    struct RK_STRUCT_TIMEOUT_NODE *prevPtr;
    volatile struct RK_STRUCT_TIMEOUT_NODE **listRefPtr;
    UINT timeoutType;
    RK_TICK timeout;
    RK_TICK dtick;
    RK_LIST *waitingQueuePtr;
    UINT waitInfo;    /* object-specific wake context */
} K_ALIGN(4);

struct RK_STRUCT_LIST_NODE
{
    struct RK_STRUCT_LIST_NODE *nextPtr;
    struct RK_STRUCT_LIST_NODE *prevPtr;
} K_ALIGN(4);

struct RK_STRUCT_LIST
{
    struct RK_STRUCT_LIST_NODE listDummy;
    ULONG size;
} K_ALIGN(4);

struct RK_OBJ_TCB;
struct RK_OBJ_PORT;
struct RK_OBJ_CHANNEL;

#if (RK_CONF_DYNAMIC_TASK == ON)
struct RK_STRUCT_DYNAMIC_TASK_ATTR
{
    RK_TASKENTRY taskFunc;
    VOID *argsPtr;
    CHAR *taskName;
    RK_PRIO priority;
    RK_OPTION preempt;
    RK_MEM_PARTITION *stackMemPtr;
} K_ALIGN(4);
#endif

struct  RK_OBJ_TCB
{
    /* --- dont change begin --- */
    UINT *sp;
    RK_TASK_STATUS status;
    ULONG runCnt;
    UINT savedLR;
    RK_STACK *stackBufPtr;
    CHAR taskName[RK_OBJ_MAX_NAME_LEN];
    ULONG stackSize;
    RK_PID pid; /* System-defined task ID */

    /*priority range: 0...31, highest to lowest */
    RK_PRIO priority;    /* Effective priority (in-use) */
    RK_PRIO prioNominal; /* Nominal assigned  priority  */
    ULONG preempt;       /* 1 if task is preemptable, 0 if not (exceptional) */
    RK_BOOL init;
    /* --- dont change end --- */

    /* sleep-timers */
    /* on every sleep-release-until call this
    field is computed and replaced */
    RK_TICK wakeTime;
    /*
    overrun count for sleep-release/until
    */
    ULONG overrunCount;
    /*
    this flag is only true when a bounded waiting expires
    not for sleep timers
    */
    RK_BOOL timeOut;

    /* Event Flags */
    RK_EVENT_FLAG flagsCurr; /* events signalled to this task */
    RK_OPTION flagsOpt;  /* a task expects ANY or ALL of */
    RK_EVENT_FLAG flagsReq;  /* the events set here */


#if (RK_CONF_MESG_QUEUE == ON)
    struct RK_OBJ_MESG_QUEUE *queuePortPtr;
#endif

#if (RK_CONF_CHANNEL == ON)
    struct RK_OBJ_CHANNEL *serverChannelPtr;
#endif

#if (RK_CONF_MUTEX == ON)
    struct RK_OBJ_MUTEX *waitingForMutexPtr;
    struct RK_STRUCT_LIST ownedMutexList;
#endif

    struct RK_STRUCT_TIMEOUT_NODE timeoutNode;
    struct RK_STRUCT_LIST_NODE tcbNode;

} K_ALIGN(4);

struct RK_STRUCT_RUNTIME
{
    volatile RK_TICK globalTick;
    volatile UINT nWraps;
} K_ALIGN(4);

struct RK_OBJ_MEM_PARTITION
{
    RK_ID objID;
    UINT init;
    BYTE *freeListPtr;
    BYTE *poolPtr;
    ULONG blkSize;
    ULONG nMaxBlocks;
    ULONG nFreeBlocks;
} K_ALIGN(4);

#if (RK_CONF_CALLOUT_TIMER == ON)
struct RK_OBJ_TIMER
{
    RK_ID objID;
    UINT reload;
    UINT init;
    RK_TICK phase;
    RK_TICK period;
    RK_TICK nextTime;
    RK_TIMER_CALLOUT funPtr;
    VOID *argsPtr;
    struct RK_STRUCT_TIMEOUT_NODE timeoutNode;
} K_ALIGN(4);
#endif

#if (RK_CONF_SEMAPHORE == ON)

struct RK_OBJ_SEMAPHORE
{
    RK_ID objID;
    UINT init;
    UINT value;
    UINT maxValue;
    struct RK_STRUCT_LIST waitingQueue;
} K_ALIGN(4);

#endif

#if (RK_CONF_MUTEX == ON)

struct RK_OBJ_MUTEX
{
    RK_ID objID;
    UINT lock;
    UINT init;
    UINT prioInh;
    struct RK_STRUCT_LIST waitingQueue;
    struct RK_OBJ_TCB *ownerPtr;
    struct RK_STRUCT_LIST_NODE mutexNode;
} K_ALIGN(4);
#endif

#if (RK_CONF_SLEEP_QUEUE == ON)

struct RK_OBJ_SLEEP_QUEUE
{
    RK_ID objID;
    struct RK_STRUCT_LIST waitingQueue;
    UINT init;
} K_ALIGN(4);

#endif /* RK_CONF_SLEEP_QUEUE */

#if (RK_CONF_MESG_QUEUE == ON)
struct RK_OBJ_MESG_QUEUE
{
    RK_ID objID;
    UINT init;
    struct RK_STRUCT_LIST waitingReceivers;
    struct RK_STRUCT_LIST waitingSenders;
    struct RK_STRUCT_RING_BUFFER ringBuf;
    struct RK_OBJ_TCB *ownerTask;
#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)
    VOID (*sendNotifyCbk)(struct RK_OBJ_MESG_QUEUE *const);
#endif
} K_ALIGN(4);
#endif /* RK_CONF_MESG_QUEUE */

#if (RK_CONF_CHANNEL == ON)
struct RK_OBJ_CHANNEL
{
    RK_ID objID;
    UINT init;
    struct RK_STRUCT_RING_BUFFER ringBuf;
    struct RK_OBJ_TCB *serverTask;
    struct RK_STRUCT_LIST waitingReceivers;
    struct RK_STRUCT_LIST waitingRequesters;
    struct RK_OBJ_MEM_PARTITION *reqPartPtr; /* request-envelope pool */
} K_ALIGN(4);

struct RK_STRUCT_REQUEST_MESG_BUF
{
    RK_TASK_HANDLE sender;
    struct RK_OBJ_CHANNEL *channelPtr;
    ULONG size;
    /* below is application-dependent
       minimally it is a generic pointer
    */
    VOID *reqPtr;
    VOID *respPtr;
} K_ALIGN(4);
#endif /* RK_CONF_CHANNEL */

#if (RK_CONF_MRM == ON)

struct RK_OBJ_MRM_BUF
{
    RK_ID objID;
    VOID *mrmData;
    ULONG nUsers; /* number of tasks using */
} K_ALIGN(4);

struct RK_OBJ_MRM
{
    RK_ID objID;
    struct RK_OBJ_MEM_PARTITION mrmMem; /* associated allocator */
    struct RK_OBJ_MEM_PARTITION mrmDataMem;
    struct RK_OBJ_MRM_BUF *currBufPtr; /* current buffer   */
    ULONG size;
    UINT init;
} K_ALIGN(4);

#endif /* RK_CONF_MRM */

#ifdef __cplusplus
}
#endif

#endif /* RK_OBJS_H */
