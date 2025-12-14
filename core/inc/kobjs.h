/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.1                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#ifndef RK_OBJS_H
#define RK_OBJS_H
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 struct RK_OBJ_TIMEOUT_NODE
 {
     struct RK_OBJ_TIMEOUT_NODE *nextPtr;
     struct RK_OBJ_TIMEOUT_NODE *prevPtr;
     UINT timeoutType;
     RK_TICK timeout;
     RK_TICK dtick;
     RK_LIST *waitingQueuePtr;
 }K_ALIGN(4);  
 
 struct RK_OBJ_LIST_NODE
 {
     struct RK_OBJ_LIST_NODE *nextPtr;
     struct RK_OBJ_LIST_NODE *prevPtr;
 } K_ALIGN(4);
 
 struct RK_OBJ_LIST
 {
     struct RK_OBJ_LIST_NODE listDummy;
     ULONG size;
 } K_ALIGN(4);
 

 struct RK_OBJ_TCB
{
/* Don't change */
    UINT *sp;
     RK_TASK_STATUS status;
     ULONG runCnt;
     UINT   savedLR;
     RK_STACK *stackBufPtr;
     CHAR taskName[RK_OBJ_MAX_NAME_LEN];
     UINT stackSize;
     RK_PID pid;/* System-defined task ID */
     RK_PRIO priority;/* Task priority (0-31) 32 is invalid */
     RK_PRIO prioReal;/* Real priority  */ 
     ULONG    preempt;

     ULONG flagsReq;
     ULONG flagsCurr;
     ULONG flagsOpt;

    RK_TICK wakeTime;     
    UINT timeOut;

#ifndef NDEBUG
     UINT nPreempted;
     RK_PID preemptedBy;
#endif

     ULONG    schLock;
#if (RK_CONF_MUTEX == ON)
     struct RK_OBJ_MUTEX *waitingForMutexPtr;
     struct RK_OBJ_LIST ownedMutexList;
#endif
     struct RK_OBJ_TIMEOUT_NODE timeoutNode;
     struct RK_OBJ_LIST_NODE tcbNode;
 } K_ALIGN(4);
 
 struct RK_OBJ_RUNTIME
 {
     volatile RK_TICK globalTick;
     volatile UINT nWraps;
 } K_ALIGN(4);
 
  struct RK_OBJ_MEM_PARTITION
 {
     RK_ID objID;
     BYTE *freeListPtr;
     BYTE *poolPtr;
     ULONG blkSize;
     ULONG nMaxBlocks;
     ULONG nFreeBlocks;
     UINT init;
 } K_ALIGN(4);
 
 
 #if (RK_CONF_CALLOUT_TIMER==ON)
 struct RK_OBJ_TIMER
 {
     UINT reload;
     UINT init;
     RK_TICK phase;
     RK_TICK period;
     RK_TICK nextTime;
     RK_TIMER_CALLOUT funPtr;
     VOID *argsPtr;
     struct RK_OBJ_TIMEOUT_NODE timeoutNode;
 }K_ALIGN(4);  
 #endif
 
 
 #if (RK_CONF_SEMAPHORE==ON)
 
 struct RK_OBJ_SEMAPHORE
 {
     RK_ID objID;
     UINT init;
     UINT value;
     UINT maxValue;
     #if (RK_CONF_SEMAPHORE_NOTIFY == ON)
     VOID (*postNotifyCbk)(struct RK_OBJ_SEMAPHORE *);
     #endif     
     struct RK_OBJ_LIST waitingQueue;
 } K_ALIGN(4);
 
 #endif
 
 #if (RK_CONF_MUTEX == ON)
 
 struct RK_OBJ_MUTEX
 {
     RK_ID objID;
     UINT lock;
     UINT init;
     UINT prioInh;
     struct RK_OBJ_LIST waitingQueue;
     struct RK_OBJ_TCB *ownerPtr;
     struct RK_OBJ_LIST_NODE mutexNode;
 } K_ALIGN(4);
 #endif
 
 #if (RK_CONF_SLEEP_QUEUE==ON)
 
 struct RK_OBJ_SLEEP_QUEUE
 {
     RK_ID objID;
     struct RK_OBJ_LIST waitingQueue;
     UINT init;
     #if (RK_CONF_SLEEP_QUEUE_NOTIFY==ON)
     VOID (*signalNotifyCbk)(struct RK_OBJ_SLEEP_QUEUE *);
     #endif
 } K_ALIGN(4);

 #endif /* RK_CONF_SLEEP_QUEUE */
 
 #if (RK_CONF_MESG_QUEUE==ON)
struct RK_OBJ_MESG_QUEUE
{
     RK_ID objID;
     UINT init;
     #if (RK_CONF_PORTS == ON)
     UINT isServer;/* enable client/server priority inheritance */
     #endif
     ULONG mesgSize;/* Number of ULONG words per message */
     ULONG maxMesg;/* Maximum number of messages */
     ULONG mesgCnt;/* Current number of messages */
     ULONG *bufPtr;/* Base pointer to the buffer (array of ULONG) */
     ULONG *writePtr;/* Write pointer (circular) */
     ULONG *readPtr;/* Read pointer (circular) */
     ULONG *bufEndPtr;/* Pointer to one past the end of the buffer */
     struct RK_OBJ_TCB *ownerTask;
     struct RK_OBJ_LIST waitingQueue;
#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)
    VOID (*sendNotifyCbk)(struct RK_OBJ_MESG_QUEUE *const);
#endif
} K_ALIGN(4);

struct RK_OBJ_MAILBOX
{
    struct RK_OBJ_MESG_QUEUE box;
    ULONG      slot[1];
} K_ALIGN(4);

#if (RK_CONF_PORTS == ON)
struct RK_OBJ_PORT_MSG_META
{
    struct RK_OBJ_TCB      *senderHandle;
    struct RK_OBJ_MESG_QUEUE *replyBox;
} K_ALIGN(4);

struct RK_OBJ_PORT_MSG2
{
    struct RK_OBJ_PORT_MSG_META meta;
} K_ALIGN(4);

struct RK_OBJ_PORT_MSG4
{
    struct RK_OBJ_PORT_MSG_META meta;
    ULONG                      payload[2];
} K_ALIGN(4);

struct RK_OBJ_PORT_MSG8
{
    struct RK_OBJ_PORT_MSG_META meta;
    ULONG                      payload[6];
} K_ALIGN(4);


struct RK_OBJ_PORT_MSG_OPAQUE
{
    struct RK_OBJ_PORT_MSG_META meta;
    VOID*                       cookie;
} K_ALIGN(4);

#define RK_PORT_META_WORDS RK_TYPE_WORD_COUNT(struct RK_OBJ_PORT_MSG_META)

#endif /* RK_CONF_PORTS */

#endif /*RK_CONF_MSG_QUEUE*/
 
 #if (RK_CONF_MRM== ON)
 
 struct RK_OBJ_MRM_BUF
 {
 
     RK_ID objID;
     VOID *mrmData;
     ULONG nUsers;/* number of tasks using */
 } K_ALIGN(4);
 
 struct RK_OBJ_MRM
 {
     RK_ID objID;
     struct RK_OBJ_MEM_PARTITION mrmMem;/* associated allocator */
     struct RK_OBJ_MEM_PARTITION mrmDataMem;
     struct RK_OBJ_MRM_BUF *currBufPtr;/* current buffer   */
     ULONG size;
     UINT init;
 } K_ALIGN(4);
 
 #endif /* MRM */
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* KOBJS_H */
 
