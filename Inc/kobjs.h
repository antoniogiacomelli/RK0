/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 *
 ******************************************************************************/

/*******************************************************************************
 *  In this header:
 *
 *                  o Kernel objects
 *
 *  Kernel Objects are the fundamental data structures for kernel services.
 *
 *
 ******************************************************************************/

#ifndef RK_OBJS_H
#define RK_OBJS_H

#ifdef __cplusplus
extern "C" {
#endif

struct kTimeoutNode
{
    struct kTimeoutNode *nextPtr;
    struct kTimeoutNode *prevPtr;
    UINT timeoutType;
    RK_TICK timeout;
    RK_TICK dtick;
    RK_LIST *waitingQueuePtr;
} __attribute__((aligned(4)));

#if (RK_CONF_CALLOUT_TIMER==ON)
struct kTimer
{
    BOOL reload;
    RK_TICK phase;
    RK_TIMER_CALLOUT funPtr;
    ADDR argsPtr;
    struct kTimeoutNode timeoutNode;
} __attribute__((aligned(4)));
#endif

struct kListNode
{
    struct kListNode *nextPtr;
    struct kListNode *prevPtr;
} __attribute__((aligned(4)));

struct kList
{
    struct kListNode listDummy;
    CHAR *listName;
    ULONG size;
    BOOL init;
} __attribute__((aligned(4)));

struct kTcb
{
/* Don't change */
    INT *sp;
    RK_TASK_STATUS status;
    ULONG runCnt;
/**/
    CHAR *taskName;
    INT *stackAddrPtr;
    UINT stackSize;
    RK_PID pid;/* System-defined task ID */
    RK_PRIO priority;/* Task priority (0-31) 32 is invalid */
#if ( (RK_CONF_FUNC_DYNAMIC_PRIO==ON) || (RK_CONF_MUTEX_PRIO_INH==ON) )
    RK_PRIO realPrio;/* Real priority  */
#endif
#if(RK_CONF_BIN_SEMA==ON)
    BOOL signalled;/* private binary semaphore */
#endif
#if (RK_CONF_EVENT_FLAGS==ON)
    ULONG currFlags;/* event flags */
    ULONG flagsOptions;
    ULONG gotFlags;
#endif
#if (RK_CONF_TASK_SIGNALS==ON)
    ULONG signals;
#endif
#if (RK_CONF_SCH_TSLICE == ON)
    RK_TICK timeSlice;
    RK_TICK timeSliceCnt;
#endif
    RK_TICK busyWaitTime;
#if (RK_CONF_SCH_TSLICE==OFF)
    RK_TICK lastWakeTime;
#endif
    BOOL runToCompl;
    BOOL yield;
    BOOL timeOut;
/* Monitoring */
    UINT nPreempted;
    RK_PID preemptedBy;
    struct kTimeoutNode timeoutNode;
    struct kListNode tcbNode;
} __attribute__((aligned(4)));

struct kRunTime
{
    RK_TICK globalTick;
    UINT nWraps;
} __attribute__((aligned(4)));

extern struct kRunTime runTime;

#if (RK_CONF_SEMA==ON)

struct kSema
{
    RK_KOBJ_ID objID;
    BOOL init;
    LONG value;
    struct kTcb *owner;
    struct kList waitingQueue;
} __attribute__((aligned(4)));

#endif

#if (RK_CONF_MUTEX == ON)

struct kMutex
{
    RK_KOBJ_ID objID;
    struct kList waitingQueue;
    BOOL lock;
    struct kTcb *ownerPtr;
    BOOL init;
} __attribute__((aligned(4)));
#endif

#if (RK_CONF_EVENT==ON)

struct kEvent
{
    RK_KOBJ_ID objID;
    struct kList waitingQueue;
    BOOL init;
#if (RK_CONF_EVENT_FLAGS)
    ULONG eventFlags;
#endif

} __attribute__((aligned(4)));

#endif /* RK_CONF_EVENT */

/* Fixed-size pool memory control block (BLOCK POOL) */
struct kMemBlock
{
    RK_KOBJ_ID objID;
    BYTE *freeListPtr;
    BYTE *poolPtr;
    ULONG blkSize;
    ULONG nMaxBlocks;
    ULONG nFreeBlocks;
    BOOL init;
} __attribute__((aligned(4)));

#if (RK_CONF_MBOX==ON)
/* Mailbox (single capacity)*/
struct kMailbox
{
    RK_KOBJ_ID objID;
    BOOL init;
    ADDR mailPtr;
    struct kList waitingQueue;
    struct kTcb *ownerTask;
} __attribute__((aligned(4)));
#endif

#if (RK_CONF_QUEUE==ON)
struct kQ
{
    RK_KOBJ_ID objID;
    BOOL init;
    ADDR *mailQPtr;/* Base pointer to the queue buffer */
    ADDR *bufEndPtr;/* Pointer to one past the end of the buffer */
    ADDR *headPtr;/* Pointer to head element (to dequeue next) */
    ADDR *tailPtr;/* Pointer to where next element will be placed */
    ULONG maxItems;/* Maximum queue capacity */
    ULONG countItems;/* Current number of items in queue */
    struct kTcb *ownerTask;
    struct kList waitingQueue;
} __attribute__((aligned(4)));
#endif

#if (RK_CONF_STREAM==ON)
struct kStream
{
    RK_KOBJ_ID objID;
    BOOL init;
    ULONG mesgSize;/* Number of ULONG words per message */
    ULONG maxMesg;/* Maximum number of messages */
    ULONG mesgCnt;/* Current number of messages */
    ULONG *bufPtr;/* Base pointer to the buffer (array of ULONG) */
    ULONG *writePtr;/* Write pointer (circular) */
    ULONG *readPtr;/* Read pointer (circular) */
    ULONG *bufEndPtr;/* Pointer to one past the end of the buffer */
    struct kTcb *ownerTask;
    struct kList waitingQueue;
} __attribute__((aligned(4)));

#endif /*RK_CONF_MSG_QUEUE*/

#if (RK_CONF_MRM== ON)

struct kMRMBuf
{

    RK_KOBJ_ID objID;
    ADDR mrmData;
    ULONG nUsers;/* number of tasks using */
} __attribute__((aligned(4)));

struct kMRMMem
{
    RK_KOBJ_ID objID;
    struct kMemBlock mrmMem;/* associated allocator */
    struct kMemBlock mrmDataMem;
    struct kMRMBuf *currBufPtr;/* current buffer   */
    ULONG size;
    UINT failReserve;
    BOOL init;
} __attribute__((aligned(4)));

#endif

#ifdef __cplusplus
}
#endif

#endif /* KOBJS_H */
