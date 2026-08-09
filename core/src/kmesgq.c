/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.52.0 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MESSAGE QUEUE                                                   */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmesgq.h>
#include <klist.h>
#include <kringbuf.h>
#include <kstring.h>
#include <kapi.h>
#include <ksystasks.h>
#include <ktrace.h>

#if (RK_CONF_MESG_QUEUE == ON)

#define RK_MESGQ_RECV_WAIT_NORMAL ((UINT)0x0)
#define RK_MESGQ_RECV_WAIT_BROADCAST ((UINT)0xB001)
#define RK_MESGQ_RECV_BROADCAST_DELIVER ((UINT)0xB002)
#define RK_MESGQ_RECV_DIRECT_DELIVER ((UINT)0xD001)

RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                      const ULONG mesgWords, ULONG const nMesg)
{
    RK_CR_AREA

    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if ((kobj == NULL) || (bufPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    /* message size needs to be 1, 2, 4, or 8 words */
    if ((mesgWords == 0) || (mesgWords > 8UL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    if ((mesgWords != 1UL) && (mesgWords != 2UL))
    {
        if (mesgWords % 4UL != 0UL)
        {
            K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
            RK_CR_EXIT
            return (RK_ERR_INVALID_MSG_SIZE);
        }
    }

    if (nMesg == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_DEPTH);
    }

    if (kobj->init == 1)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#endif

    RK_ERR err = kRingBufInit(&kobj->ringBuf, bufPtr, mesgWords, nMesg);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kListInit(&kobj->waitingReceivers);
    if (err != 0)
    {
        RK_CR_EXIT
        return (err);
    }
    err = kListInit(&kobj->waitingSenders);
    if (err != 0)
    {
        RK_CR_EXIT
        return (err);
    }
    kobj->init = 1;
    kobj->objID = RK_MESGQQUEUE_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->broadcastReceivers = 0UL;

    kobj->ownerTask = NULL;
    kTraceRegisterObject(kobj, RK_MESGQQUEUE_KOBJ_ID);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    kobj->sendNotifyCbk = NULL;

#endif

    RK_CR_EXIT

    return (err);
}

static RK_ERR kPortBindOwner_(RK_MESG_QUEUE *const portPtr,
                              RK_TASK_HANDLE const ownerTask)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((portPtr == NULL) || (ownerTask == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (portPtr->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (portPtr->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if ((ownerTask->queuePortPtr != NULL) &&
        (ownerTask->queuePortPtr != portPtr))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_HAS_OWNER);
#endif
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }
    if ((portPtr->ownerTask != NULL) && (portPtr->ownerTask != ownerTask))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_HAS_OWNER);
#endif
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }

    portPtr->ownerTask = ownerTask;
    ownerTask->queuePortPtr = portPtr;

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kPortInit_(RK_MESG_QUEUE *const portPtr, VOID *const bufPtr,
                  ULONG const mesgWords, ULONG const depth,
                  RK_TASK_HANDLE const ownerTask)
{
    RK_ERR err = kMesgQueueInit(portPtr, bufPtr, mesgWords, depth);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
    return (kPortBindOwner_(portPtr, ownerTask));
}

static inline RK_BOOL kPortTaskOwnsMutex_(RK_TCB const *const taskPtr)
{
#if (RK_CONF_MUTEX == ON)
    return (((taskPtr != NULL) && (taskPtr->ownedMutexList.size > 0UL))
                ? RK_TRUE
                : RK_FALSE);
#else
    (VOID)taskPtr;
    return (RK_FALSE);
#endif
}

static inline RK_BOOL kPortOperationOwnsMutex_(RK_MESG_QUEUE const *const kobj)
{
    return (((kobj->ownerTask != NULL) &&
             (kPortTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE))
                ? RK_TRUE
                : RK_FALSE);
}

static VOID kMesgQueueSetOwnerPrio_(RK_TCB *const ownerPtr,
                                    RK_PRIO const targetPrio)
{
    if (ownerPtr == NULL)
    {
        return;
    }

    if (targetPrio == ownerPtr->priority)
    {
        return;
    }

    if (ownerPtr->status == RK_READY)
    {
        RK_PRIO const oldPrio = ownerPtr->priority;
        RK_TCB *remPtr = ownerPtr;
        RK_ERR err = kTCBQRem(&RK_gReadyQueue[ownerPtr->priority], &remPtr);
        K_ASSERT(!err);
        ownerPtr->priority = targetPrio;
        kTraceRecordTaskPrio(ownerPtr, oldPrio, targetPrio);
        err = kTCBQEnq(&RK_gReadyQueue[ownerPtr->priority], ownerPtr);
        K_ASSERT(!err);
    }
    else
    {
        RK_PRIO const oldPrio = ownerPtr->priority;
        ownerPtr->priority = targetPrio;
        kTraceRecordTaskPrio(ownerPtr, oldPrio, targetPrio);
        if (ownerPtr->status == RK_RUNNING)
        {
            kReschedRunning();
        }
    }
}

static inline VOID kMesgQueueUpdateOwnerPrio_(RK_MESG_QUEUE *const kobj)
{
    RK_TCB *ownerPtr = kobj->ownerTask;
    if (ownerPtr == NULL)
    {
        return;
    }

    RK_PRIO targetPrio = ownerPtr->prioNominal;
    if (kobj->waitingSenders.size > 0U)
    {
        RK_TCB *highestWaiterPtr = kTCBQPeek(&kobj->waitingSenders);
        K_ASSERT(highestWaiterPtr != NULL);
        if (targetPrio > highestWaiterPtr->priority)
        {
            targetPrio = highestWaiterPtr->priority;
        }
    }

    kMesgQueueSetOwnerPrio_(ownerPtr, targetPrio);
}

static inline VOID kMesgQueueRestoreOwnerPrio_(RK_MESG_QUEUE *const kobj)
{
    RK_TCB *ownerPtr = kobj->ownerTask;
    if (ownerPtr == NULL)
    {
        return;
    }

    kMesgQueueSetOwnerPrio_(ownerPtr, ownerPtr->prioNominal);
}

static VOID kMesgQueueReadyTopTask_(RK_TCB **const chosenTCBPtr,
                                    RK_TCB *const taskPtr)
{
    if ((chosenTCBPtr == NULL) || (taskPtr == NULL))
    {
        return;
    }

    kReadyNoSwtch(taskPtr);
    if ((*chosenTCBPtr == NULL) ||
        (taskPtr->priority < (*chosenTCBPtr)->priority))
    {
        *chosenTCBPtr = taskPtr;
    }
}

static VOID kMesgQueueClearBlockingTimeout_(RK_TCB *const taskPtr)
{
    if (taskPtr == NULL)
    {
        return;
    }

    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        kRemoveTimeoutNode(&taskPtr->timeoutNode);
        taskPtr->timeoutNode.timeoutType = 0;
        taskPtr->timeoutNode.waitingQueuePtr = NULL;
    }
}

static RK_ERR kMesgQueueDeqNormalReceiver_(RK_MESG_QUEUE *const kobj,
                                           RK_TCB **const recvTaskPPtr)
{
    RK_NODE *nodePtr = NULL;

    if ((kobj == NULL) || (recvTaskPPtr == NULL))
    {
        return (RK_ERR_OBJ_NULL);
    }

    nodePtr = kobj->waitingReceivers.listDummy.nextPtr;
    while (nodePtr != &kobj->waitingReceivers.listDummy)
    {
        RK_TCB *taskPtr = K_GET_TCB_ADDR(nodePtr);
        if (taskPtr->timeoutNode.waitInfo != RK_MESGQ_RECV_WAIT_BROADCAST)
        {
            RK_ERR err = kListRemove(&kobj->waitingReceivers,
                                     &taskPtr->tcbNode);
            if (err == RK_ERR_SUCCESS)
            {
                taskPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
                *recvTaskPPtr = taskPtr;
            }
            return (err);
        }
        nodePtr = nodePtr->nextPtr;
    }

    *recvTaskPPtr = NULL;
    return (RK_ERR_EMPTY_WAITING_QUEUE);
}

static ULONG kMesgQueueCountBroadcastWaiters_(RK_MESG_QUEUE const *const kobj)
{
    ULONG nWaiters = 0UL;
    RK_NODE const *nodePtr = NULL;

    if (kobj == NULL)
    {
        return (0UL);
    }

    nodePtr = kobj->waitingReceivers.listDummy.nextPtr;
    while (nodePtr != &kobj->waitingReceivers.listDummy)
    {
        RK_TCB const *taskPtr = K_GET_TCB_ADDR(nodePtr);
        if (taskPtr->timeoutNode.waitInfo == RK_MESGQ_RECV_WAIT_BROADCAST)
        {
            nWaiters++;
        }
        nodePtr = nodePtr->nextPtr;
    }

    return (nWaiters);
}

static UINT kMesgQueuePrepareBroadcastReceivers_(RK_MESG_QUEUE *const kobj,
                                                 UINT const nTasks)
{
    UINT marked = 0U;
    RK_NODE *nodePtr = kobj->waitingReceivers.listDummy.nextPtr;

    while ((marked < nTasks) && (nodePtr != &kobj->waitingReceivers.listDummy))
    {
        RK_NODE *const nextPtr = nodePtr->nextPtr;
        RK_TCB *const recvTaskPtr = K_GET_TCB_ADDR(nodePtr);

        if (recvTaskPtr->timeoutNode.waitInfo == RK_MESGQ_RECV_WAIT_BROADCAST)
        {
            kMesgQueueClearBlockingTimeout_(recvTaskPtr);
            recvTaskPtr->timeoutNode.waitInfo =
                RK_MESGQ_RECV_BROADCAST_DELIVER;
            marked++;
        }
        nodePtr = nextPtr;
    }

    return (marked);
}

RK_ERR kMesgQueueBroadcastWake(RK_MESG_QUEUE *const kobj, UINT const nTasks)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    UINT woken = 0U;
    RK_TCB *chosenTCBPtr = NULL;
    RK_NODE *nodePtr = kobj->waitingReceivers.listDummy.nextPtr;

    while ((woken < nTasks) && (nodePtr != &kobj->waitingReceivers.listDummy))
    {
        RK_NODE *const nextPtr = nodePtr->nextPtr;
        RK_TCB *const recvTaskPtr = K_GET_TCB_ADDR(nodePtr);

        if (recvTaskPtr->timeoutNode.waitInfo ==
            RK_MESGQ_RECV_BROADCAST_DELIVER)
        {
            RK_ERR err = kListRemove(&kobj->waitingReceivers,
                                     &recvTaskPtr->tcbNode);
            K_ASSERT(err == RK_ERR_SUCCESS);
            kMesgQueueReadyTopTask_(&chosenTCBPtr, recvTaskPtr);
            woken++;
        }
        nodePtr = nextPtr;
    }

    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size);
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

static VOID kMesgQueueWakeNormalReceiverIfAny_(RK_MESG_QUEUE *const kobj)
{
    RK_TCB *freeTaskPtr = NULL;

    if (kMesgQueueDeqNormalReceiver_(kobj, &freeTaskPtr) != RK_ERR_SUCCESS)
    {
        return;
    }

    kMesgQueueClearBlockingTimeout_(freeTaskPtr);
    freeTaskPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size);
    kReadySwtch(freeTaskPtr);
}

static inline VOID kMesgQueueCopy_(RK_MESG_QUEUE const *const kobj,
                                   VOID *const recvPtr,
                                   VOID const *const sendPtr)
{
    ULONG const nBytes = kobj->ringBuf.dataSize * RK_WORD_SIZE;
    RK_MEMCPY(recvPtr, sendPtr, nBytes);
}

static RK_BOOL kMesgQueueDirectSendIfAny_(RK_MESG_QUEUE *const kobj,
                                          VOID const *const sendPtr,
                                          RK_TRACE_OP const traceOp,
                                          RK_BOOL const notifySend)
{
    RK_TCB *recvTaskPtr = NULL;

    if ((kobj->ringBuf.nFull != 0UL) || (kobj->broadcastReceivers > 0UL))
    {
        return (RK_FALSE);
    }

    if (kMesgQueueDeqNormalReceiver_(kobj, &recvTaskPtr) != RK_ERR_SUCCESS)
    {
        return (RK_FALSE);
    }

    K_ASSERT(recvTaskPtr != NULL);
    K_ASSERT(recvTaskPtr->status == RK_RECEIVING);
    K_ASSERT(recvTaskPtr->mesgQueueRecvBufPtr != NULL);

    kMesgQueueCopy_(kobj, recvTaskPtr->mesgQueueRecvBufPtr, sendPtr);
    recvTaskPtr->mesgQueueRecvBufPtr = NULL;
    recvTaskPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_DIRECT_DELIVER;
    kMesgQueueClearBlockingTimeout_(recvTaskPtr);
    kTraceRecordObject(kobj, traceOp, RK_ERR_SUCCESS, kobj->ringBuf.nFull);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)
    if ((notifySend == RK_TRUE) && (kobj->sendNotifyCbk != NULL))
    {
        kobj->sendNotifyCbk(kobj);
    }
#else
    (VOID)notifySend;
#endif

    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size);
    kReadySwtch(recvTaskPtr);
    return (RK_TRUE);
}

static VOID kMesgQueueWakeSenderIfAny_(RK_MESG_QUEUE *const kobj)
{
    if ((kobj == NULL) || (kobj->waitingSenders.size == 0UL))
    {
        return;
    }

    RK_TCB *freeTaskPtr = NULL;
    freeTaskPtr = kTCBQPeek(&kobj->waitingSenders);
    kTCBQDeq(&kobj->waitingSenders, &freeTaskPtr);
    kMesgQueueClearBlockingTimeout_(freeTaskPtr);
    kMesgQueueRestoreOwnerPrio_(kobj);
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingSenders.size);
    kReadySwtch(freeTaskPtr);
}

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

RK_ERR kMesgQueueInstallSendCbk(RK_MESG_QUEUE *const kobj,
                                VOID (*cbk)(RK_MESG_QUEUE *))

{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    kobj->sendNotifyCbk = cbk;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
#endif

RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                      const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    { /* Queue full */
        if (timeout == 0)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_BUFFER_FULL,
                               kobj->ringBuf.nFull);
            RK_CR_EXIT
            return (RK_ERR_BUFFER_FULL);
        }

        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingSenders;
                RK_BARRIER
                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, err,
                                       kobj->waitingSenders.size);
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_SENDING;
            kTraceRecordObject(kobj, RK_TRACE_OP_SEND_BLOCK, RK_ERR_SUCCESS,
                               kobj->waitingSenders.size + 1UL);
            kTCBQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);
            kMesgQueueUpdateOwnerPrio_(kobj);

            kPendCtxSwtch();
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                                   kobj->waitingSenders.size);
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
        } while (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf);
    }

    if (kMesgQueueDirectSendIfAny_(kobj, sendPtr, RK_TRACE_OP_SEND,
                                   RK_TRUE) == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)
    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);
#endif
    K_ASSERT(kobj->ringBuf.nFull <= kobj->ringBuf.maxBuf);
    /* unblock a normal reader, if any */
    kMesgQueueWakeNormalReceiverIfAny_(kobj);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const kobj, VOID *const recvPtr,
                      const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (recvPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if ((kobj->ownerTask != NULL) && (kobj->ownerTask != RK_gRunPtr))
    {
        RK_CR_EXIT
        return (RK_ERR_NOT_OWNER);
    }
    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }
    if ((kobj->ringBuf.nFull == 0) || (kobj->broadcastReceivers > 0UL))
    {
        if (timeout == RK_NO_WAIT)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_BUFFER_EMPTY,
                               kobj->ringBuf.nFull);
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }
        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr =
                    &kobj->waitingReceivers;
                RK_BARRIER

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                    kTraceRecordObject(kobj, RK_TRACE_OP_RECV, err,
                                       kobj->waitingReceivers.size);
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_RECEIVING;
            RK_gRunPtr->mesgQueueRecvBufPtr = recvPtr;
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
            kTraceRecordObject(kobj, RK_TRACE_OP_RECV_BLOCK, RK_ERR_SUCCESS,
                               kobj->waitingReceivers.size + 1UL);
            kTCBQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);

            kPendCtxSwtch();

            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                RK_gRunPtr->mesgQueueRecvBufPtr = NULL;
                RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
                kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                                   kobj->waitingReceivers.size);
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
            if (RK_gRunPtr->timeoutNode.waitInfo ==
                RK_MESGQ_RECV_DIRECT_DELIVER)
            {
                RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
                RK_gRunPtr->mesgQueueRecvBufPtr = NULL;
                kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_SUCCESS,
                                   kobj->ringBuf.nFull);
                RK_CR_EXIT
                return (RK_ERR_SUCCESS);
            }
        } while ((kobj->ringBuf.nFull == 0) ||
                 (kobj->broadcastReceivers > 0UL));
    }

    kRingBufRead(&kobj->ringBuf, (ULONG *)recvPtr);
    RK_gRunPtr->mesgQueueRecvBufPtr = NULL;
    kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);
    /* unlock a writer, if any */
    kMesgQueueWakeSenderIfAny_(kobj);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const kobj, VOID *const recvPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (recvPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if ((kobj->ringBuf.nFull == 0) || (kobj->broadcastReceivers > 0UL))
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    kRingBufPeek(&kobj->ringBuf, (ULONG *)recvPtr);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                     const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif
    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    { /* Queue full */
        if (timeout == RK_NO_WAIT)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_JAM, RK_ERR_BUFFER_FULL,
                               kobj->ringBuf.nFull);
            RK_CR_EXIT
            return (RK_ERR_BUFFER_FULL);
        }

        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingSenders;
                RK_BARRIER

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                    kTraceRecordObject(kobj, RK_TRACE_OP_JAM, err,
                                       kobj->waitingSenders.size);
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_SENDING;
            kTraceRecordObject(kobj, RK_TRACE_OP_JAM_BLOCK, RK_ERR_SUCCESS,
                               kobj->waitingSenders.size + 1UL);

            kTCBQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);
            kMesgQueueUpdateOwnerPrio_(kobj);

            kPendCtxSwtch();
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                                   kobj->waitingSenders.size);
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
        } while (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf);
    }

    if (kMesgQueueDirectSendIfAny_(kobj, sendPtr, RK_TRACE_OP_JAM,
                                   RK_TRUE) == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    kRingBufJam(&kobj->ringBuf, (ULONG const *)sendPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_JAM, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);

#endif

    /* unblock a reader, if any */
    kMesgQueueWakeNormalReceiverIfAny_(kobj);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr,
                       UINT *const nWaitRPtr, UINT *const nWaitSPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

#endif

    if ((nMesgPtr == NULL) && (nWaitRPtr == NULL) && (nWaitSPtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (nMesgPtr != NULL)
    {
        *nMesgPtr = (UINT)kobj->ringBuf.nFull;
    }
    if (nWaitRPtr != NULL)
    {
        *nWaitRPtr = (UINT)kobj->waitingReceivers.size;
    }
    if (nWaitSPtr != NULL)
    {
        *nWaitSPtr = (UINT)kobj->waitingSenders.size;
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

#endif

    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    UINT toWakeR = kobj->waitingReceivers.size;
    UINT toWakeS = kobj->waitingSenders.size;
    UINT toWake = toWakeR + toWakeS;
    /* Defer only when running in ISR context; handle multi-wake inline
       otherwise to avoid re-enqueue loops when the PostProc worker invokes this
       helper. */
    if ((toWake > 0U) && kIsISR())
    {
        RK_CR_EXIT
        return (
            kPostProcJobEnq(RK_POSTPROC_JOB_MESGQ_RESET, (VOID *)kobj, toWake));
    }

    kRingBufReset(&kobj->ringBuf);
    kobj->broadcastReceivers = 0UL;
    kTraceRecordObject(kobj, RK_TRACE_OP_RESET, RK_ERR_SUCCESS, toWake);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    kobj->sendNotifyCbk = NULL;
#endif

    if (toWake == 0U)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    RK_TCB *chosenTCBPtr = NULL;
    for (UINT i = 0U; i < toWakeR; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingReceivers, &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        nextTCBPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
        nextTCBPtr->mesgQueueRecvBufPtr = NULL;
        kReadyNoSwtch(nextTCBPtr);
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
    }
    for (UINT i = 0U; i < toWakeS; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingSenders, &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadyNoSwtch(nextTCBPtr);
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
    }
    kMesgQueueUpdateOwnerPrio_(kobj);
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* this works only for N=1 */
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->ringBuf.maxBuf > 1)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }

    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (kobj->broadcastReceivers > 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_FULL);
    }

    if (kMesgQueueDirectSendIfAny_(kobj, sendPtr, RK_TRACE_OP_SEND,
                                   RK_FALSE) == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    RK_BOOL wasEmpty = RK_FALSE;

    if (kobj->ringBuf.nFull == 0)
    {
        wasEmpty = RK_TRUE;
    }

    kRingBufOverwrite(&kobj->ringBuf, (ULONG const *)sendPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);

    if (wasEmpty)
    {
        kMesgQueueWakeNormalReceiverIfAny_(kobj);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* only work for 1-slot mail queues */
RK_ERR kMesgQueueBroadcast(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                           UINT *const nRecvPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (nRecvPtr != NULL)
    {
        *nRecvPtr = 0U;
    }

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->ringBuf.maxBuf > 1)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }

    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    UINT const toWake = (UINT)kMesgQueueCountBroadcastWaiters_(kobj);

    if (toWake == 0U)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_BUFFER_EMPTY,
                           kobj->ringBuf.nFull);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_BUFFER_FULL,
                           kobj->ringBuf.nFull);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_FULL);
    }

    RK_BOOL const deferWake = (toWake > 1U) ? RK_TRUE : RK_FALSE;
    if (deferWake == RK_TRUE)
    {
        RK_ERR const deferErr =
            kPostProcJobEnq(RK_POSTPROC_JOB_MESGQ_BROADCAST_WAKE,
                            (VOID *)kobj, toWake);
        if (deferErr != RK_ERR_SUCCESS)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, deferErr,
                               (ULONG)toWake);
            RK_CR_EXIT
            return (deferErr);
        }
    }

    kobj->broadcastReceivers = (ULONG)toWake;
    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);

    UINT const prepared = kMesgQueuePrepareBroadcastReceivers_(kobj, toWake);
    K_ASSERT(prepared == toWake);
    kobj->broadcastReceivers = (ULONG)prepared;

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)
    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);
#endif

    if (nRecvPtr != NULL)
    {
        *nRecvPtr = prepared;
    }
    if (deferWake == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    RK_ERR const wakeErr = kMesgQueueBroadcastWake(kobj, prepared);
    RK_CR_EXIT
    return (wakeErr);
}

RK_ERR kMesgQueueBroadcastRecv(RK_MESG_QUEUE *const kobj,
                               VOID *const recvPtr,
                               const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (recvPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->ringBuf.maxBuf > 1UL)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }

    if ((kobj->ownerTask != NULL) && (kobj->ownerTask != RK_gRunPtr))
    {
        RK_CR_EXIT
        return (RK_ERR_NOT_OWNER);
    }
    if (kPortOperationOwnsMutex_(kobj) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    while (RK_gRunPtr->timeoutNode.waitInfo !=
           RK_MESGQ_RECV_BROADCAST_DELIVER)
    {
        if (timeout == RK_NO_WAIT)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_BUFFER_EMPTY,
                               kobj->ringBuf.nFull);
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_gRunPtr->timeoutNode.waitingQueuePtr =
                &kobj->waitingReceivers;
            RK_BARRIER

            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                kTraceRecordObject(kobj, RK_TRACE_OP_RECV, err,
                                   kobj->waitingReceivers.size);
                RK_CR_EXIT
                return (err);
            }
        }
        RK_gRunPtr->status = RK_RECEIVING;
        RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_BROADCAST;
        kTraceRecordObject(kobj, RK_TRACE_OP_RECV_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingReceivers.size + 1UL);
        kTCBQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);

        kPendCtxSwtch();

        RK_CR_EXIT
        RK_CR_ENTER
        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
            kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                               kobj->waitingReceivers.size);
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        }
    }

    K_ASSERT(kobj->ringBuf.nFull == 1UL);
    K_ASSERT(kobj->broadcastReceivers > 0UL);

    RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;

    if (kobj->broadcastReceivers == 1UL)
    {
        kRingBufRead(&kobj->ringBuf, (ULONG *)recvPtr);
        kobj->broadcastReceivers = 0UL;
        kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_SUCCESS,
                           kobj->ringBuf.nFull);
        kMesgQueueWakeSenderIfAny_(kobj);
    }
    else
    {
        kRingBufPeek(&kobj->ringBuf, (ULONG *)recvPtr);
        kobj->broadcastReceivers--;
        kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_SUCCESS,
                           kobj->ringBuf.nFull);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_MESG_QUEUE */
