/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.90.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: CHANNEL                                                         */
/******************************************************************************/
/**
 * @brief 
 * This component implements the synchronous message passing, either zero-buffer
 * send or send/receive invocation. The channel is bound to a server task, which
 * may have several channels, but a task may have only one type of channel
 * SYNCH_SEND or SYNCH_INVOCATION. 
 * Invocation -> server runs at caller priority until reply.
 * Synch Send -> receiver's priority may boost if lower than sender's.
 */
#define RK_SOURCE_CODE
#include <kchannel.h>
#include <ksch.h>
#include <klist.h>
#include <ktimer.h>
#include <ktrace.h>
#include <kstring.h>

#if (RK_CONF_SYNCH_MESG == ON)
/* A new identity on every call also rejects tokens from a previous binding. */
static ULONG nextCallId;

static RK_CHANNEL *kChannelFromNode_(RK_NODE const *const nodePtr)
{
    return (K_GET_CONTAINER_ADDR(nodePtr, RK_CHANNEL_BINDING, node)
                ->channelPtr);
}

static RK_BOOL kChannelBytesValid_(ULONG const bytes)
{
    return ((RK_BOOL)((bytes != 0UL) && ((bytes % RK_WORD_SIZE) == 0UL)));
}

static VOID kChannelBytesSet_(ULONG *const bytesPtr, ULONG const bytes)
{
    if (bytesPtr != NULL)
        *bytesPtr = bytes;
}

static RK_ERR kChannelCheckObject_(RK_CHANNEL const *const kobj)
{
    if (kobj == NULL)
        return (RK_ERR_OBJ_NULL);
    if (kobj->init != RK_TRUE)
        return (RK_ERR_OBJ_NOT_INIT);
    if (kobj->objID != RK_CHANNEL_KOBJ_ID)
        return (RK_ERR_INVALID_OBJ);
    return (RK_ERR_SUCCESS);
}

static RK_ERR kChannelCheckTask_(RK_CHANNEL const *const kobj,
                                 RK_OPTION const mode,
                                 RK_BOOL const sending,
                                 RK_TICK const timeout)
{
    RK_ERR const err = kChannelCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
        return (err);
    if (kIsISR())
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    if (kobj->mode != mode)
        return (RK_ERR_INVALID_PARAM);
    RK_TCB const *const taskPtr = RK_gRunPtr;
    if ((taskPtr == NULL) || (!sending && (taskPtr != kobj->receiver)))
        return (RK_ERR_NOT_OWNER);
    if (sending && (taskPtr == kobj->receiver))
        return (RK_ERR_INVALID_PARAM);
    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
        return (RK_ERR_INVALID_TIMEOUT);
    if (taskPtr->waitingChannelPtr != NULL)
        return (RK_ERR_TASK_INVALID_ST);
#if (RK_CONF_MUTEX == ON)
    if (taskPtr->ownedMutexList.size != 0UL)
        return (RK_ERR_TASK_INVALID_ST);
#endif
    if (sending)
    {
        if ((kobj->state != RK_CHANNEL_IDLE) || (kobj->sender != NULL))
            return (RK_ERR_CHANNEL_BUSY);
        RK_NODE const *nodePtr = taskPtr->channelList.listDummy.nextPtr;
        while (nodePtr != &taskPtr->channelList.listDummy)
        {
            RK_CHANNEL const *const channelPtr = kChannelFromNode_(nodePtr);
            if (channelPtr->sender == taskPtr)
                return (RK_ERR_CHANNEL_BUSY);
            nodePtr = nodePtr->nextPtr;
        }
    }
    return (RK_ERR_SUCCESS);
}

static RK_BOOL kChannelTaskHasActiveCall_(RK_TCB const *const taskPtr)
{
    RK_NODE const *nodePtr = taskPtr->channelList.listDummy.nextPtr;
    while (nodePtr != &taskPtr->channelList.listDummy)
    {
        RK_CHANNEL const *const channelPtr = kChannelFromNode_(nodePtr);
        if ((channelPtr->receiver == taskPtr) &&
            ((channelPtr->state == RK_CHANNEL_ACTIVE) ||
             (channelPtr->state == RK_CHANNEL_ABANDONED)))
            return (RK_TRUE);
        nodePtr = nodePtr->nextPtr;
    }
    return (RK_FALSE);
}

static RK_BOOL kChannelTaskModeValid_(RK_TCB const *const taskPtr,
                                      RK_OPTION const mode)
{
    RK_NODE const *nodePtr = taskPtr->channelList.listDummy.nextPtr;
    while (nodePtr != &taskPtr->channelList.listDummy)
    {
        RK_CHANNEL const *const channelPtr = kChannelFromNode_(nodePtr);
        if ((channelPtr->receiver == taskPtr) && (channelPtr->mode != mode))
            return (RK_FALSE);
        nodePtr = nodePtr->nextPtr;
    }
#if (RK_CONF_ASYNCH_MESG == ON)
    if ((taskPtr->mesgEndpointPtr != NULL) &&
        (taskPtr->mesgEndpointPtr->mode == RK_MESG_SEND_RECV))
        return (RK_FALSE);
#endif
    return (RK_TRUE);
}

static RK_ERR kChannelArmWait_(RK_CHANNEL *const kobj,
                               RK_TCB *const taskPtr,
                               UINT const timeoutType,
                               RK_TICK const timeout)
{
    taskPtr->waitingChannelPtr = kobj;
    if (timeout != RK_WAIT_FOREVER)
    {
        taskPtr->timeoutNode.timeoutType = timeoutType;
        RK_ERR const err = kTimeoutNodeAdd(&taskPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            taskPtr->waitingChannelPtr = NULL;
            kTimeoutNodeReset(&taskPtr->timeoutNode);
            return (err);
        }
    }
    return (RK_ERR_SUCCESS);
}

static RK_ERR kChannelWake_(RK_TCB *const taskPtr)
{
    if (kTimeoutNodeIsArmed(&taskPtr->timeoutNode) == RK_TRUE)
    {
        RK_ERR const err = kTimeoutNodeDisarm(&taskPtr->timeoutNode);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    else
        kTimeoutNodeReset(&taskPtr->timeoutNode);

    RK_ERR const err = kReadySwtch(taskPtr);
    return (((err == RK_ERR_RESCHED_PENDING) ||
             (err == RK_ERR_RESCHED_NOT_NEEDED)) ? RK_ERR_SUCCESS : err);
}

static VOID kChannelRetainSender_(RK_CHANNEL *const kobj,
                                  RK_TCB *const taskPtr)
{
    kobj->sender = taskPtr;
    kListAddTail(&taskPtr->channelList, &kobj->senderRef.node);
}

static VOID kChannelReleaseSender_(RK_CHANNEL *const kobj)
{
    kListRemove(&kobj->sender->channelList, &kobj->senderRef.node);
    kobj->sender = NULL;
}

static RK_ERR kChannelWaitResult_(RK_CHANNEL *const kobj,
                                  RK_TCB *const taskPtr,
                                  RK_BOOL const sending)
{
    RK_ERR err = sending ? kobj->senderStatus : RK_ERR_SUCCESS;
    taskPtr->waitingChannelPtr = NULL;
    if (taskPtr->timeOut == RK_TRUE)
    {
        taskPtr->timeOut = RK_FALSE;
        err = RK_ERR_TIMEOUT;
    }
    if (sending && (kobj->state != RK_CHANNEL_ABANDONED))
        kChannelReleaseSender_(kobj);
    return (err);
}

static VOID kChannelClearRequest_(RK_CHANNEL *const kobj)
{
    kobj->requestPtr = NULL;
    kobj->requestBytes = 0UL;
}

static VOID kChannelClearReply_(RK_CHANNEL *const kobj)
{
    kobj->data.invocation.replyPtr = NULL;
    kobj->data.invocation.replyCapacity = 0UL;
    kobj->data.invocation.replyBytesPtr = NULL;
}

RK_ERR kChannelInit(RK_CHANNEL *const kobj,
                    RK_TASK_HANDLE const *const serverHandlePtr,
                    RK_OPTION const mode)
{
    RK_CR_AREA
    RK_CR_ENTER
    if ((kobj == NULL) || (serverHandlePtr == NULL) ||
        (*serverHandlePtr == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kIsISR())
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (kobj->init == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    RK_TCB *const receiverPtr = *serverHandlePtr;
    if (receiverPtr->init != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
    if ((mode != SYNCH_SEND) && (mode != SYNCH_INVOCATION))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if (kChannelTaskModeValid_(receiverPtr, mode) == RK_FALSE)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }
    kobj->mode = mode;
    kobj->sender = NULL;
    kobj->receiver = receiverPtr;
    kobj->senderRef.channelPtr = kobj;
    kobj->receiverBinding.channelPtr = kobj;
    kListAddTail(&receiverPtr->channelList, &kobj->receiverBinding.node);
    kobj->state = RK_CHANNEL_IDLE;
    kobj->receiverWaiting = RK_FALSE;
    kobj->senderStatus = RK_ERR_SUCCESS;
    kChannelClearRequest_(kobj);
    RK_MEMSET(&kobj->data, 0, sizeof(kobj->data));
    kobj->objID = RK_CHANNEL_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->init = RK_TRUE;
    kTraceRegisterObject(kobj, RK_CHANNEL_KOBJ_ID);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelDestroy(RK_CHANNEL *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR const err = kChannelCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    if (kIsISR())
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if ((kobj->state != RK_CHANNEL_IDLE) || kobj->receiverWaiting ||
        (kobj->sender != NULL) ||
        (kobj->receiver->waitingChannelPtr == kobj))
    {
        RK_CR_EXIT
        return (RK_ERR_CHANNEL_BUSY);
    }
    kListRemove(&kobj->receiver->channelList, &kobj->receiverBinding.node);
    kTraceUnregisterObject(kobj);
    kobj->init = RK_FALSE;
    kobj->objID = RK_INVALID_KOBJ;
    kobj->sender = NULL;
    kobj->receiver = NULL;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelSend(RK_CHANNEL *const kobj, VOID const *const mesgPtr,
                    ULONG const mesgBytes, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kChannelCheckTask_(kobj, SYNCH_SEND, RK_TRUE, timeout);
    if (err == RK_ERR_SUCCESS)
    {
        if (mesgPtr == NULL)
            err = RK_ERR_OBJ_NULL;
        else if (kChannelBytesValid_(mesgBytes) == RK_FALSE)
            err = RK_ERR_INVALID_MSG_SIZE;
    }
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    if (kobj->receiverWaiting)
    {
        if (mesgBytes > kobj->data.send.capacity)
        {
            RK_CR_EXIT
            return (RK_ERR_INVALID_MSG_SIZE);
        }
        RK_MEMCPY(kobj->data.send.recvPtr, mesgPtr, mesgBytes);
        kChannelBytesSet_(kobj->data.send.bytesPtr, mesgBytes);
        kobj->data.send.recvPtr = NULL;
        kobj->data.send.bytesPtr = NULL;
        kobj->receiverWaiting = RK_FALSE;
        err = kChannelWake_(kobj->receiver);
        kTraceRecordObject(kobj, RK_TRACE_OP_SEND, err, mesgBytes);
        RK_CR_EXIT
        return (err);
    }
    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }
    RK_TCB *const senderPtr = RK_gRunPtr;
    err = kChannelArmWait_(kobj, senderPtr, RK_TIMEOUT_SYNCH_SEND, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    kChannelRetainSender_(kobj, senderPtr);
    kobj->requestPtr = mesgPtr;
    kobj->requestBytes = mesgBytes;
    kobj->senderStatus = RK_ERR_SUCCESS;
    kobj->state = RK_CHANNEL_QUEUED;
    senderPtr->status = RK_SENDING;
    kTaskUpdateEffectivePrioChain(kobj->receiver);
    kTraceRecordObject(kobj, RK_TRACE_OP_SEND_BLOCK, RK_ERR_SUCCESS, mesgBytes);
    kPendCtxSwtch();
    RK_CR_EXIT
    RK_CR_ENTER
    err = kChannelWaitResult_(kobj, senderPtr, RK_TRUE);
    RK_CR_EXIT
    return (err);
}

RK_ERR kChannelRecv(RK_CHANNEL *const kobj, VOID *const recvPtr,
                    ULONG const capacity, ULONG *const bytesPtr,
                    RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kChannelCheckTask_(kobj, SYNCH_SEND, RK_FALSE, timeout);
    if (err == RK_ERR_SUCCESS)
    {
        if (recvPtr == NULL)
            err = RK_ERR_OBJ_NULL;
        else if (kChannelBytesValid_(capacity) == RK_FALSE)
            err = RK_ERR_INVALID_MSG_SIZE;
    }
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    if (kobj->state == RK_CHANNEL_QUEUED)
    {
        if (kobj->requestBytes > capacity)
        {
            RK_CR_EXIT
            return (RK_ERR_INVALID_MSG_SIZE);
        }
        RK_MEMCPY(recvPtr, kobj->requestPtr, kobj->requestBytes);
        kChannelBytesSet_(bytesPtr, kobj->requestBytes);
        kChannelClearRequest_(kobj);
        kobj->state = RK_CHANNEL_IDLE;
        kobj->senderStatus = RK_ERR_SUCCESS;
        kTaskUpdateEffectivePrioChain(kobj->receiver);
        err = kChannelWake_(kobj->sender);
        RK_CR_EXIT
        return (err);
    }
    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }
    RK_TCB *const receiverPtr = kobj->receiver;
    err = kChannelArmWait_(kobj, receiverPtr, RK_TIMEOUT_SYNCH_RECV, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    kobj->data.send.recvPtr = recvPtr;
    kobj->data.send.capacity = capacity;
    kobj->data.send.bytesPtr = bytesPtr;
    kobj->receiverWaiting = RK_TRUE;
    receiverPtr->status = RK_RECEIVING;
    kTraceRecordObject(kobj, RK_TRACE_OP_RECV_BLOCK, RK_ERR_SUCCESS, capacity);
    kPendCtxSwtch();
    RK_CR_EXIT
    RK_CR_ENTER
    err = kChannelWaitResult_(kobj, receiverPtr, RK_FALSE);
    RK_CR_EXIT
    return (err);
}

RK_ERR kChannelCall(RK_CHANNEL *const kobj,
                    RK_CHANNEL_ATTR const *const attrPtr,
                    RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kChannelCheckTask_(kobj, SYNCH_INVOCATION, RK_TRUE, timeout);
    if (err == RK_ERR_SUCCESS)
    {
        if ((attrPtr == NULL) || (attrPtr->reqPtr == NULL) ||
            (attrPtr->replyPtr == NULL))
            err = RK_ERR_OBJ_NULL;
        else if ((kChannelBytesValid_(attrPtr->reqBytes) == RK_FALSE) ||
                 (kChannelBytesValid_(attrPtr->replyMaxBytes) == RK_FALSE))
            err = RK_ERR_INVALID_MSG_SIZE;
        else if (timeout == RK_NO_WAIT)
            err = RK_ERR_INVALID_TIMEOUT;
    }
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    RK_TCB *const senderPtr = RK_gRunPtr;
    err = kChannelArmWait_(kobj, senderPtr, RK_TIMEOUT_SYNCH_CALL, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    kChannelRetainSender_(kobj, senderPtr);
    kobj->requestPtr = attrPtr->reqPtr;
    kobj->requestBytes = attrPtr->reqBytes;
    kobj->data.invocation.replyPtr = attrPtr->replyPtr;
    kobj->data.invocation.replyCapacity = attrPtr->replyMaxBytes;
    kobj->data.invocation.replyBytesPtr = attrPtr->replyBytesPtr;
    if (++nextCallId == 0UL)
        ++nextCallId;
    kobj->data.invocation.callId = nextCallId;
    kChannelBytesSet_(attrPtr->replyBytesPtr, 0UL);
    kobj->senderStatus = RK_ERR_SUCCESS;
    kobj->state = RK_CHANNEL_QUEUED;
    senderPtr->status = RK_RECEIVING;
    kTaskUpdateEffectivePrioChain(kobj->receiver);
    if (kobj->receiverWaiting)
    {
        kobj->receiverWaiting = RK_FALSE;
        err = kChannelWake_(kobj->receiver);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_CALL, RK_ERR_SUCCESS, attrPtr->reqBytes);
    kPendCtxSwtch();
    RK_CR_EXIT
    RK_CR_ENTER
    err = kChannelWaitResult_(kobj, senderPtr, RK_TRUE);
    RK_CR_EXIT
    return (err);
}

RK_ERR kChannelAccept(RK_CHANNEL *const kobj,
                      RK_CHANNEL_CALL_DATA *const callPtr,
                      VOID *const recvPtr, ULONG const capacity,
                      ULONG *const reqBytesPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kChannelCheckTask_(kobj, SYNCH_INVOCATION, RK_FALSE, timeout);
    if (err == RK_ERR_SUCCESS)
    {
        if ((callPtr == NULL) || (recvPtr == NULL))
            err = RK_ERR_OBJ_NULL;
        else if (kChannelBytesValid_(capacity) == RK_FALSE)
            err = RK_ERR_INVALID_MSG_SIZE;
        else if (kChannelTaskHasActiveCall_(kobj->receiver))
            err = RK_ERR_SYNCH_CALL_BUSY;
    }
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    RK_TCB *const receiverPtr = kobj->receiver;
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), timeout);
    while (kobj->state != RK_CHANNEL_QUEUED)
    {
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }
        RK_TICK remaining = RK_WAIT_FOREVER;
        if (timeout != RK_WAIT_FOREVER)
        {
            RK_STICK const delta = K_TICK_DIFF(deadline, kTickGet());
            if (delta <= 0)
            {
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            remaining = (RK_TICK)delta;
        }
        err = kChannelArmWait_(kobj, receiverPtr, RK_TIMEOUT_SYNCH_RECV,
                               remaining);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (err);
        }
        kobj->receiverWaiting = RK_TRUE;
        receiverPtr->status = RK_RECEIVING;
        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER
        err = kChannelWaitResult_(kobj, receiverPtr, RK_FALSE);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (err);
        }
    }
    if (kobj->requestBytes > capacity)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_MSG_SIZE);
    }
    RK_MEMCPY(recvPtr, kobj->requestPtr, kobj->requestBytes);
    kChannelBytesSet_(reqBytesPtr, kobj->requestBytes);
    callPtr->channelPtr = kobj;
    callPtr->callId = kobj->data.invocation.callId;
    callPtr->caller = kobj->sender;
    callPtr->reqPtr = recvPtr;
    callPtr->reqBytes = kobj->requestBytes;
    callPtr->replyPtr = kobj->data.invocation.replyPtr;
    callPtr->replyMaxBytes = kobj->data.invocation.replyCapacity;
    kChannelClearRequest_(kobj);
    kobj->state = RK_CHANNEL_ACTIVE;
    kobj->data.invocation.acceptedPriority = kobj->sender->priority;
    kTaskUpdateEffectivePrioChain(receiverPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_ACCEPT, RK_ERR_SUCCESS,
                       callPtr->reqBytes);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelReply(RK_CHANNEL_CALL_DATA const *const callPtr,
                     VOID const *const replyPtr, ULONG const replyBytes)
{
    RK_CR_AREA
    RK_CR_ENTER
    if (callPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    RK_CHANNEL *const kobj = callPtr->channelPtr;
    RK_ERR err = kChannelCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    if (kIsISR())
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if ((RK_gRunPtr == NULL) || (RK_gRunPtr != kobj->receiver))
    {
        RK_CR_EXIT
        return (RK_ERR_NOT_OWNER);
    }
    if ((kobj->mode != SYNCH_INVOCATION) ||
        (callPtr->caller != kobj->sender) ||
        (callPtr->callId != kobj->data.invocation.callId) ||
        ((kobj->state != RK_CHANNEL_ACTIVE) &&
         (kobj->state != RK_CHANNEL_ABANDONED)))
    {
        RK_CR_EXIT
        return (RK_ERR_SYNCH_CALL_NOT_ACTIVE);
    }
#if (RK_CONF_MUTEX == ON)
    if (RK_gRunPtr->ownedMutexList.size != 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }
#endif
    if (kobj->state == RK_CHANNEL_ABANDONED)
    {
        kobj->state = RK_CHANNEL_IDLE;
        kChannelClearReply_(kobj);
        if (kobj->sender->waitingChannelPtr != kobj)
            kChannelReleaseSender_(kobj);
        kTaskUpdateEffectivePrioChain(kobj->receiver);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }
    if (((replyBytes != 0UL) && (replyPtr == NULL)) ||
        (replyBytes > kobj->data.invocation.replyCapacity) ||
        ((replyBytes % RK_WORD_SIZE) != 0UL))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_MSG_SIZE);
    }
    if (replyBytes != 0UL)
        RK_MEMCPY(kobj->data.invocation.replyPtr, replyPtr, replyBytes);
    kChannelBytesSet_(kobj->data.invocation.replyBytesPtr, replyBytes);
    kobj->senderStatus = RK_ERR_SUCCESS;
    kobj->state = RK_CHANNEL_IDLE;
    kChannelClearReply_(kobj);
    /* Complete the timeout transaction before any priority-driven reschedule. */
    if (kTimeoutNodeIsArmed(&kobj->sender->timeoutNode) == RK_TRUE)
    {
        err = kTimeoutNodeDisarm(&kobj->sender->timeoutNode);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    kTaskUpdateEffectivePrioChain(kobj->receiver);
    err = kChannelWake_(kobj->sender);
    kTraceRecordObject(kobj, RK_TRACE_OP_DONE, err, replyBytes);
    RK_CR_EXIT
    return (err);
}

VOID kChannelTimeout(RK_TCB *const taskPtr)
{
    RK_CHANNEL *const kobj = taskPtr->waitingChannelPtr;
    K_ASSERT(kobj != NULL);
    if (taskPtr == kobj->sender)
    {
        kChannelClearRequest_(kobj);
        kobj->senderStatus = RK_ERR_TIMEOUT;
        if ((kobj->mode == SYNCH_INVOCATION) &&
            (kobj->state == RK_CHANNEL_ACTIVE))
        {
            kChannelClearReply_(kobj);
            kobj->state = RK_CHANNEL_ABANDONED;
        }
        else
        {
            kobj->state = RK_CHANNEL_IDLE;
            if (kobj->mode == SYNCH_INVOCATION)
                kChannelClearReply_(kobj);
        }
        kTaskUpdateEffectivePrioChain(kobj->receiver);
    }
    else
    {
        kobj->receiverWaiting = RK_FALSE;
        if (kobj->mode == SYNCH_SEND)
        {
            kobj->data.send.recvPtr = NULL;
            kobj->data.send.bytesPtr = NULL;
        }
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT, 0UL);
}

RK_PRIO kChannelTaskBasePrio(RK_TCB const *const taskPtr,
                            RK_PRIO const currentPrio)
{
    RK_NODE const *nodePtr = taskPtr->channelList.listDummy.nextPtr;
    while (nodePtr != &taskPtr->channelList.listDummy)
    {
        RK_CHANNEL const *const kobj = kChannelFromNode_(nodePtr);
        if ((kobj->receiver == taskPtr) && (kobj->mode == SYNCH_INVOCATION) &&
            (kobj->state == RK_CHANNEL_ACTIVE))
            return (kobj->data.invocation.acceptedPriority);
        nodePtr = nodePtr->nextPtr;
    }
    return (currentPrio);
}

RK_PRIO kChannelTaskWaiterPrio(RK_TCB const *const taskPtr,
                              RK_PRIO const currentPrio)
{
    RK_PRIO newPrio = currentPrio;
    RK_NODE const *nodePtr = taskPtr->channelList.listDummy.nextPtr;
    while (nodePtr != &taskPtr->channelList.listDummy)
    {
        RK_CHANNEL const *const kobj = kChannelFromNode_(nodePtr);
        if ((kobj->receiver == taskPtr) && (kobj->state == RK_CHANNEL_QUEUED) &&
            (kobj->sender->priority < newPrio))
            newPrio = kobj->sender->priority;
        nodePtr = nodePtr->nextPtr;
    }
    return (newPrio);
}
#endif /* RK_CONF_SYNCH_MESG */
