/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.71.0                                                          */
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
    ULONG schLock;       /* Scheduler lock depth owned */
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
    RK_TASK_EVENT flagsCurr; /* events signalled to this task */
    RK_OPTION flagsOpt;  /* a task expects ANY or ALL of */
    RK_TASK_EVENT flagsReq;  /* the events set here */


#if (RK_CONF_MESG_QUEUE == ON)
    VOID *mesgQueueRecvBufPtr;
#endif

#if ((RK_CONF_ASYNCH_MESG == ON) && (RK_CONF_MESG_QUEUE == ON))
    RK_BOOL asynchMesgInit;
    struct RK_STRUCT_LIST asynchMesgQueue;
    struct RK_STRUCT_LIST asynchMesgWaiters;
    struct RK_OBJ_TCB *asynchMesgWaitSenderPtr;
    RK_MESG **asynchMesgWaitDestPtr;
    RK_ERR asynchMesgWaitStatus;
#endif /* RK_CONF_ASYNCH_MESG && RK_CONF_MESG_QUEUE */

#if (RK_CONF_SYNCH_MESG == ON)
    ULONG synchMesgMaxBytes;
    VOID const *synchMesgPendingPtr;
    struct RK_OBJ_TCB *synchMesgPendingSenderPtr;
    VOID *synchMesgRecvBufPtr;
    ULONG *synchMesgRecvBytesPtr;
    RK_ERR synchMesgRecvStatus;
    struct RK_STRUCT_LIST synchMesgSenders;
    VOID const *synchMesgPtr;
    ULONG synchMesgBytes;
    RK_ERR synchMesgStatus;
    struct RK_OBJ_TCB *synchMesgReceiverPtr;
    struct RK_STRUCT_LIST synchMesgCallers;
    struct RK_STRUCT_LIST synchMesgAcceptWaiters;
    struct RK_OBJ_TCB *synchMesgActiveCallerPtr;
    RK_PRIO synchMesgActiveCallerPrio;
    VOID *synchMesgCallReplyBufPtr;
    ULONG *synchMesgCallReplyBytesPtr;
    ULONG synchMesgCallReplyMaxBytes;
    RK_SYNCH_CALL_STATE synchMesgCallState;
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
    CHAR objName[RK_NAME_SIZE];
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
    CHAR objName[RK_NAME_SIZE];
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
    CHAR objName[RK_NAME_SIZE];
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
    CHAR objName[RK_NAME_SIZE];
    UINT lock;
    UINT init;
    UINT protocol;
    struct RK_STRUCT_LIST waitingQueue;
    struct RK_OBJ_TCB *ownerPtr;
    struct RK_STRUCT_LIST_NODE mutexNode;
} K_ALIGN(4);
#endif

#if (RK_CONF_SLEEP_QUEUE == ON)

struct RK_OBJ_SLEEP_QUEUE
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
    struct RK_STRUCT_LIST waitingQueue;
    UINT init;
} K_ALIGN(4);

#endif /* RK_CONF_SLEEP_QUEUE */

#if (RK_CONF_MESG_QUEUE == ON)
struct RK_OBJ_MESG_QUEUE
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
    UINT init;
    struct RK_STRUCT_LIST waitingReceivers;
    struct RK_STRUCT_LIST waitingSenders;
    struct RK_STRUCT_RING_BUFFER ringBuf;
    ULONG broadcastReceivers;
#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)
    VOID (*sendNotifyCbk)(struct RK_OBJ_MESG_QUEUE *const);
#endif
} K_ALIGN(4);
#endif /* RK_CONF_MESG_QUEUE */

#if ((RK_CONF_ASYNCH_MESG == ON) && (RK_CONF_MESG_QUEUE == ON))
struct RK_OBJ_MESG
{
    struct RK_STRUCT_LIST_NODE mesgNode;
    RK_MEM_PARTITION *poolPtr;
    RK_TASK_HANDLE sender;
    RK_TASK_HANDLE receiver;
    ULONG payloadBytes;
    RK_PID senderPid;
    RK_PID receiverPid;
    RK_MESG_STATE state;
    RK_ID objID;
} K_ALIGN(4);
#endif /* RK_CONF_ASYNCH_MESG && RK_CONF_MESG_QUEUE */

#if (RK_CONF_SYNCH_MESG == ON)
struct RK_STRUCT_SYNCH_ATTR
{
    VOID const *reqPtr;
    ULONG reqBytes;
    VOID *replyPtr;
    ULONG replyMaxBytes;
    ULONG *replyBytesPtr;
} K_ALIGN(4);

struct RK_STRUCT_SYNCH_CALL_DATA
{
    RK_TASK_HANDLE caller;
    VOID *reqPtr;
    VOID *replyPtr;
    ULONG reqBytes;
    ULONG replyMaxBytes;
} K_ALIGN(4);
#endif /* RK_CONF_SYNCH_MESG */

#if (RK_CONF_MRM == ON)

struct RK_OBJ_MRM_BUF
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
    VOID *mrmData;
    ULONG nUsers; /* number of tasks using */
} K_ALIGN(4);

struct RK_OBJ_MRM
{
    RK_ID objID;
    CHAR objName[RK_NAME_SIZE];
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
