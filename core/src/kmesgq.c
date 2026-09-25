/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.83.0                                                          */
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
#include <ksch.h>
#include <ksystasks.h>
#include <ktrace.h>

#if (RK_CONF_MESG_QUEUE == ON)

#define RK_MESGQ_RECV_WAIT_NORMAL ((UINT)0x0)
#if (RK_CONF_MBOX_BROADCAST == ON)
#define RK_MESGQ_RECV_WAIT_BROADCAST ((UINT)0xB001)
#define RK_MESGQ_RECV_BROADCAST_DELIVER ((UINT)0xB002)
#endif
#define RK_MESGQ_RECV_DIRECT_DELIVER ((UINT)0xD001)
#if (RK_CONF_MBOX_BROADCAST == ON)
#define RK_MESGQ_SEND_WAIT_BROADCAST ((UINT)0xB101)
#endif

#if (RK_CONF_MBOX_BROADCAST == ON)
static RK_ERR kMesgQueueCheckObject_(RK_MESG_QUEUE *const kobj)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
#else
    (void)kobj;
#endif

    return (RK_ERR_SUCCESS);
}

static RK_ERR kMboxJoinCurrent_(RK_MBOX *const kobj)
{
    if (kobj->mboxPrioCeilingEnabled != RK_TRUE)
    {
        (void)kobj;
        return (RK_ERR_SUCCESS);
    }

    if (RK_gRunPtr->mboxCeilingPtr == kobj)
    {
        return (RK_ERR_SUCCESS);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (RK_gRunPtr->mboxCeilingPtr != NULL)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (RK_gRunPtr->priority < kobj->mboxPrioCeiling)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_PRIO);
        return (RK_ERR_INVALID_PRIO);
    }
#endif

    RK_gRunPtr->mboxCeilingPtr = kobj;
    kTaskUpdateEffectivePrioChain(RK_gRunPtr);

    return (RK_ERR_SUCCESS);
}

static RK_ERR kMboxLeaveCurrent_(RK_MBOX *const kobj)
{
    if (RK_gRunPtr->mboxCeilingPtr == NULL)
    {
        (void)kobj;
        return (RK_ERR_SUCCESS);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (RK_gRunPtr->mboxCeilingPtr != kobj)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        return (RK_ERR_TASK_INVALID_ST);
    }
#endif

    RK_gRunPtr->mboxCeilingPtr = NULL;
    kTaskUpdateEffectivePrioChain(RK_gRunPtr);

    return (RK_ERR_SUCCESS);
}
#endif /* RK_CONF_MBOX_BROADCAST */

static RK_ERR kMesgQueueInit_(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                              const ULONG mesgWords, ULONG const nMesg
#if (RK_CONF_MBOX_BROADCAST == ON)
                              , RK_PRIO const mboxCeilingPrio
#endif
                              )
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

#if (RK_CONF_MBOX_BROADCAST == ON)
    if ((mboxCeilingPrio != RK_MBOX_PRIO_CEILING_NONE) &&
        (mboxCeilingPrio > RK_CONF_MIN_PRIO))
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_PRIO);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PRIO);
    }
#endif

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
#if (RK_CONF_MBOX_BROADCAST == ON)
    kobj->broadcastReceivers = 0UL;
    if (mboxCeilingPrio == RK_MBOX_PRIO_CEILING_NONE)
    {
        kobj->mboxPrioCeiling = RK_MBOX_PRIO_CEILING_NONE;
        kobj->mboxPrioCeilingEnabled = RK_FALSE;
    }
    else
    {
        kobj->mboxPrioCeiling = mboxCeilingPrio;
        kobj->mboxPrioCeilingEnabled = RK_TRUE;
    }
#endif

    kTraceRegisterObject(kobj, RK_MESGQQUEUE_KOBJ_ID);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    kobj->sendNotifyCbk = NULL;

#endif

    RK_CR_EXIT

    return (err);
}

RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                      const ULONG mesgWords, ULONG const nMesg)
{
#if (RK_CONF_MBOX_BROADCAST == ON)
    return (kMesgQueueInit_(kobj, bufPtr, mesgWords, nMesg,
                            RK_MBOX_PRIO_CEILING_NONE));
#else
    return (kMesgQueueInit_(kobj, bufPtr, mesgWords, nMesg));
#endif
}

#if (RK_CONF_MBOX_BROADCAST == ON)
RK_ERR kMboxInitCeiling(RK_MBOX *const kobj, VOID *const bufPtr,
                        const ULONG mesgWords, RK_PRIO const ceilingPrio)
{
    return (kMesgQueueInit_(kobj, bufPtr, mesgWords, 1UL, ceilingPrio));
}

RK_ERR kMboxJoin(RK_MBOX *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR err = kMesgQueueCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR() || (RK_gRunPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    err = kMboxJoinCurrent_(kobj);

    RK_CR_EXIT
    return (err);
}

RK_ERR kMboxLeave(RK_MBOX *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR err = kMesgQueueCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR() || (RK_gRunPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    err = kMboxLeaveCurrent_(kobj);

    RK_CR_EXIT
    return (err);
}
#endif /* RK_CONF_MBOX_BROADCAST */

#if (RK_CONF_MBOX_BROADCAST == ON)
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
#endif /* RK_CONF_MBOX_BROADCAST */

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
    }
}

static VOID kMesgQueueClearSenderWait_(RK_TCB *const taskPtr)
{
    if (taskPtr == NULL)
    {
        return;
    }

    taskPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
#if (RK_CONF_MBOX_BROADCAST == ON)
    taskPtr->mboxBcastMinRecv = 0U;
#endif
    kMesgQueueClearBlockingTimeout_(taskPtr);
}

static RK_BOOL kMesgQueueSenderEligible_(RK_MESG_QUEUE const *const kobj,
                                         RK_TCB const *const taskPtr
#if (RK_CONF_MBOX_BROADCAST == ON)
                                         , ULONG const broadcastWaiters
#endif
                                         )
{
    if ((kobj == NULL) || (taskPtr == NULL))
    {
        return (RK_FALSE);
    }

#if (RK_CONF_MBOX_BROADCAST == ON)
    if (taskPtr->timeoutNode.waitInfo == RK_MESGQ_SEND_WAIT_BROADCAST)
    {
        return (((kobj->ringBuf.maxBuf == 1UL) &&
                 (kobj->ringBuf.nFull == 0UL) &&
                 (kobj->broadcastReceivers == 0UL) &&
                 (broadcastWaiters >=
                  (ULONG)taskPtr->mboxBcastMinRecv))
                    ? RK_TRUE
                    : RK_FALSE);
    }
#endif

    return ((kobj->ringBuf.nFull < kobj->ringBuf.maxBuf) ? RK_TRUE
                                                          : RK_FALSE);
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
#if (RK_CONF_MBOX_BROADCAST == ON)
        if (taskPtr->timeoutNode.waitInfo != RK_MESGQ_RECV_WAIT_BROADCAST)
#endif
        {
            RK_ERR err = kWaitQRemove(&kobj->waitingReceivers, taskPtr);
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

#if (RK_CONF_MBOX_BROADCAST == ON)
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
            RK_ERR err = kWaitQRemove(&kobj->waitingReceivers, recvTaskPtr);
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
#endif /* RK_CONF_MBOX_BROADCAST */

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

    if (kobj->ringBuf.nFull != 0UL)
    {
        return (RK_FALSE);
    }
#if (RK_CONF_MBOX_BROADCAST == ON)
    if (kobj->broadcastReceivers > 0UL)
    {
        return (RK_FALSE);
    }
#endif

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

#if (RK_CONF_MBOX_BROADCAST == ON)
    ULONG const broadcastWaiters = kMesgQueueCountBroadcastWaiters_(kobj);
#endif
    RK_TCB *freeTaskPtr = NULL;
    RK_NODE *nodePtr = kobj->waitingSenders.listDummy.nextPtr;

    while (nodePtr != &kobj->waitingSenders.listDummy)
    {
        RK_TCB *const taskPtr = K_GET_TCB_ADDR(nodePtr);
        if (kMesgQueueSenderEligible_(kobj, taskPtr
#if (RK_CONF_MBOX_BROADCAST == ON)
                                      , broadcastWaiters
#endif
                                      ) == RK_TRUE)
        {
            freeTaskPtr = taskPtr;
            break;
        }
        nodePtr = nodePtr->nextPtr;
    }

    if (freeTaskPtr == NULL)
    {
        return;
    }

    RK_ERR const err = kWaitQRemove(&kobj->waitingSenders, freeTaskPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);
    kMesgQueueClearSenderWait_(freeTaskPtr);
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
                RK_COMPILER_BARRIER
                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, err,
                                       kobj->waitingSenders.size);
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_SENDING;
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
#if (RK_CONF_MBOX_BROADCAST == ON)
            RK_gRunPtr->mboxBcastMinRecv = 0U;
#endif
            kTraceRecordObject(kobj, RK_TRACE_OP_SEND_BLOCK, RK_ERR_SUCCESS,
                               kobj->waitingSenders.size + 1UL);
            kWaitQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);

            kPendCtxSwtch();
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
#if (RK_CONF_MBOX_BROADCAST == ON)
                RK_gRunPtr->mboxBcastMinRecv = 0U;
#endif
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

    if ((kobj->ringBuf.nFull == 0)
#if (RK_CONF_MBOX_BROADCAST == ON)
        || (kobj->broadcastReceivers > 0UL)
#endif
        )
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
                RK_COMPILER_BARRIER

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
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
            kWaitQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);

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
            }
            if (RK_gRunPtr->timeoutNode.waitInfo ==
                RK_MESGQ_RECV_DIRECT_DELIVER)
            {
                RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
                RK_gRunPtr->mesgQueueRecvBufPtr = NULL;
                /*
                 * A sender woken by a previous buffered receive may deliver
                 * directly to us. Other senders can still be queued even
                 * though that delivery left buffer capacity available.
                 */
                if (kobj->ringBuf.nFull < kobj->ringBuf.maxBuf)
                {
                    kMesgQueueWakeSenderIfAny_(kobj);
                }
                kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_SUCCESS,
                                   kobj->ringBuf.nFull);
                RK_CR_EXIT
                return (RK_ERR_SUCCESS);
            }
        } while ((kobj->ringBuf.nFull == 0)
#if (RK_CONF_MBOX_BROADCAST == ON)
                 || (kobj->broadcastReceivers > 0UL)
#endif
                 );
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

    if ((kobj->ringBuf.nFull == 0)
#if (RK_CONF_MBOX_BROADCAST == ON)
        || (kobj->broadcastReceivers > 0UL)
#endif
        )
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
                RK_COMPILER_BARRIER

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    kTraceRecordObject(kobj, RK_TRACE_OP_JAM, err,
                                       kobj->waitingSenders.size);
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_SENDING;
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
#if (RK_CONF_MBOX_BROADCAST == ON)
            RK_gRunPtr->mboxBcastMinRecv = 0U;
#endif
            kTraceRecordObject(kobj, RK_TRACE_OP_JAM_BLOCK, RK_ERR_SUCCESS,
                               kobj->waitingSenders.size + 1UL);

            kWaitQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);

            kPendCtxSwtch();
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
#if (RK_CONF_MBOX_BROADCAST == ON)
                RK_gRunPtr->mboxBcastMinRecv = 0U;
#endif
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
#if (RK_CONF_MBOX_BROADCAST == ON)
    kobj->broadcastReceivers = 0UL;
#endif
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
        kWaitQDeq(&kobj->waitingReceivers, &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
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
        kWaitQDeq(&kobj->waitingSenders, &nextTCBPtr);
        kMesgQueueClearSenderWait_(nextTCBPtr);
        kReadyNoSwtch(nextTCBPtr);
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
    }
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

#if (RK_CONF_MBOX_BROADCAST == ON)
    if (kobj->broadcastReceivers > 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_FULL);
    }
#endif

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

#if (RK_CONF_MBOX_BROADCAST == ON)
static RK_ERR kMesgQueueBroadcastCommit_(RK_MESG_QUEUE *const kobj,
                                         VOID *const sendPtr,
                                         UINT const toWake,
                                         UINT *const nRecvPtr)
{
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
        return (RK_ERR_SUCCESS);
    }

    return (kMesgQueueBroadcastWake(kobj, prepared));
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

    RK_ERR const wakeErr =
        kMesgQueueBroadcastCommit_(kobj, sendPtr, toWake, nRecvPtr);
    RK_CR_EXIT
    return (wakeErr);
}

RK_ERR kMesgQueueBroadcastWaitN(RK_MESG_QUEUE *const kobj,
                                VOID *const sendPtr,
                                UINT const minReceivers,
                                const RK_TICK timeout,
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

    if ((minReceivers == 0U) || (minReceivers > RK_NTHREADS))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if ((minReceivers == 0U) || (minReceivers > RK_NTHREADS))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (kobj->ringBuf.maxBuf > 1UL)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }

    while (1)
    {
        UINT const toWake = (UINT)kMesgQueueCountBroadcastWaiters_(kobj);

        if (toWake >= minReceivers)
        {
            if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
            {
                if (timeout == RK_NO_WAIT)
                {
                    kTraceRecordObject(kobj, RK_TRACE_OP_SEND,
                                       RK_ERR_BUFFER_FULL,
                                       kobj->ringBuf.nFull);
                    RK_CR_EXIT
                    return (RK_ERR_BUFFER_FULL);
                }
            }
            else
            {
                RK_ERR const err =
                    kMesgQueueBroadcastCommit_(kobj, sendPtr, toWake,
                                               nRecvPtr);
                RK_CR_EXIT
                return (err);
            }
        }
        else if (timeout == RK_NO_WAIT)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_BUFFER_EMPTY,
                               kobj->ringBuf.nFull);
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_COMPILER_BARRIER

            RK_ERR const err =
                kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                kTraceRecordObject(kobj, RK_TRACE_OP_SEND, err,
                                   kobj->waitingSenders.size);
                RK_CR_EXIT
                return (err);
            }
        }

        RK_gRunPtr->status = RK_SENDING;
        RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_SEND_WAIT_BROADCAST;
        RK_gRunPtr->mboxBcastMinRecv = minReceivers;
        kTraceRecordObject(kobj, RK_TRACE_OP_SEND_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingSenders.size + 1UL);
        RK_ERR const enqErr = kWaitQEnqByPrio(&kobj->waitingSenders,
                                              RK_gRunPtr);
        if (enqErr != RK_ERR_SUCCESS)
        {
            kMesgQueueClearSenderWait_(RK_gRunPtr);
            RK_gRunPtr->status = RK_RUNNING;
            RK_CR_EXIT
            return (enqErr);
        }

        kPendCtxSwtch();

        RK_CR_EXIT
        RK_CR_ENTER
        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
            RK_gRunPtr->mboxBcastMinRecv = 0U;
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
        }

        RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
        RK_gRunPtr->mboxBcastMinRecv = 0U;
    }
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

    RK_BOOL const lazyCeiling =
        ((timeout != RK_NO_WAIT) &&
         (kobj->mboxPrioCeilingEnabled == RK_TRUE) &&
         (RK_gRunPtr->mboxCeilingPtr == NULL))
            ? RK_TRUE
            : RK_FALSE;
    RK_ERR err = RK_ERR_SUCCESS;
    if (timeout != RK_NO_WAIT)
    {
        err = kMboxJoinCurrent_(kobj);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (err);
        }
    }

    while (RK_gRunPtr->timeoutNode.waitInfo !=
           RK_MESGQ_RECV_BROADCAST_DELIVER)
    {
        if (timeout == RK_NO_WAIT)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_BUFFER_EMPTY,
                               kobj->ringBuf.nFull);
            if (lazyCeiling == RK_TRUE)
            {
                kMboxLeaveCurrent_(kobj);
            }
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_COMPILER_BARRIER

            err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                kTraceRecordObject(kobj, RK_TRACE_OP_RECV, err,
                                   kobj->waitingReceivers.size);
                if (lazyCeiling == RK_TRUE)
                {
                    kMboxLeaveCurrent_(kobj);
                }
                RK_CR_EXIT
                return (err);
            }
        }
        RK_gRunPtr->status = RK_RECEIVING;
        RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_BROADCAST;
        kTraceRecordObject(kobj, RK_TRACE_OP_RECV_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingReceivers.size + 1UL);
        err = kWaitQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);
        if (err != RK_ERR_SUCCESS)
        {
            kMesgQueueClearBlockingTimeout_(RK_gRunPtr);
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
            RK_gRunPtr->status = RK_RUNNING;
            if (lazyCeiling == RK_TRUE)
            {
                kMboxLeaveCurrent_(kobj);
            }
            RK_CR_EXIT
            return (err);
        }

        kMesgQueueWakeSenderIfAny_(kobj);

        kPendCtxSwtch();

        RK_CR_EXIT
        RK_CR_ENTER
        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            RK_gRunPtr->timeoutNode.waitInfo = RK_MESGQ_RECV_WAIT_NORMAL;
            kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                               kobj->waitingReceivers.size);
            if (lazyCeiling == RK_TRUE)
            {
                kMboxLeaveCurrent_(kobj);
            }
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0;
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

    if (lazyCeiling == RK_TRUE)
    {
        kMboxLeaveCurrent_(kobj);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
#endif /* RK_CONF_MBOX_BROADCAST */

#endif /* RK_CONF_MESG_QUEUE */
