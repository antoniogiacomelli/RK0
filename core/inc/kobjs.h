/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
     RK_WALL_TICK checkInTime;
     RK_WALL_TICK checkOutTime;
     RK_LIST *waitingQueuePtr;
 } __K_ALIGN(4);
 
 #if (RK_CONF_CALLOUT_TIMER==ON)
 struct kTimer
 {
     BOOL reload;
     RK_TICK phase;
     RK_TIMER_CALLOUT funPtr;
     VOID *argsPtr;
     struct kTimeoutNode timeoutNode;
 } __K_ALIGN(4);
 #endif
 
 struct kListNode
 {
     struct kListNode *nextPtr;
     struct kListNode *prevPtr;
 } __K_ALIGN(4);
 
 struct kList
 {
     struct kListNode listDummy;
     CHAR *listName;
     ULONG size;
     BOOL init;
 } __K_ALIGN(4);
 
 struct kTcb
 {
 /* Don't change */
     UINT *sp;
     RK_TASK_STATUS status;
     ULONG runCnt;
     UINT   savedLR;
     UINT *stackAddrPtr;
     CHAR *taskName;
     UINT stackSize;
     RK_PID pid;/* System-defined task ID */
     RK_PRIO priority;/* Task priority (0-31) 32 is invalid */
     RK_PRIO prioReal;/* Real priority  */
     ULONG flagsReq;
     ULONG flagsCurr;
     ULONG flagsOpt;
     RK_TICK lastWakeTime;
      BOOL runToCompl;
     BOOL timeOut;
 /* Monitoring */
     UINT nPreempted;
     RK_PID preemptedBy;
     struct kTimeoutNode timeoutNode;
     struct kListNode tcbNode;
 } __K_ALIGN(4);
 
 struct kRunTime
 {
     volatile RK_TICK globalTick;
     volatile UINT nWraps;
 } __K_ALIGN(4);
 
 
 #if (RK_CONF_SEMA==ON)
 
 struct kSema
 {
     RK_KOBJ_ID objID;
     BOOL init;
     INT value;
     UINT semaType;
     struct kList waitingQueue;
 } __K_ALIGN(4);
 
 #endif
 
 #if (RK_CONF_MUTEX == ON)
 
 struct kMutex
 {
     RK_KOBJ_ID objID;
     struct kList waitingQueue;
     BOOL lock;
     struct kTcb *ownerPtr;
     BOOL init;
 } __K_ALIGN(4);
 #endif
 
 #if (RK_CONF_EVENT==ON)
 
 struct kEvent
 {
     RK_KOBJ_ID objID;
     struct kList waitingQueue;
     BOOL init;
 } __K_ALIGN(4);
 
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
 } __K_ALIGN(4);
 
 #if (RK_CONF_MBOX==ON)
 /* Mailbox (single capacity)*/
 struct kMailbox
 {
     RK_KOBJ_ID objID;
     BOOL init;
     VOID *mailPtr;
     struct kList waitingQueue;
     struct kTcb *ownerTask;
 } __K_ALIGN(4);
 #endif
 
 #if (RK_CONF_QUEUE==ON)
 struct kQ
 {
     RK_KOBJ_ID objID;
     BOOL init;
     VOID **mailQPtr;/* Base pointer to the queue buffer */
     VOID **bufEndPtr;/* Pointer to one past the end of the buffer */
     VOID **headPtr;/* Pointer to head element (to dequeue next) */
     VOID **tailPtr;/* Pointer to where next element will be placed */
     ULONG maxItems;/* Maximum queue capacity */
     ULONG countItems;/* Current number of items in queue */
     struct kTcb *ownerTask;
     struct kList waitingQueue;
 } __K_ALIGN(4);
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
 } __K_ALIGN(4);
 
 #endif /*RK_CONF_MSG_QUEUE*/
 
 #if (RK_CONF_MRM== ON)
 
 struct kMRMBuf
 {
 
     RK_KOBJ_ID objID;
     VOID *mrmData;
     ULONG nUsers;/* number of tasks using */
 } __K_ALIGN(4);
 
 struct kMRMMem
 {
     RK_KOBJ_ID objID;
     struct kMemBlock mrmMem;/* associated allocator */
     struct kMemBlock mrmDataMem;
     struct kMRMBuf *currBufPtr;/* current buffer   */
     ULONG size;
     UINT failReserve;
     BOOL init;
 } __K_ALIGN(4);
 
 #endif
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* KOBJS_H */
 