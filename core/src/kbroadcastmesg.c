/* SPDX-License-Identifier: Apache-2.0 */
/* Independent copied broadcast cohorts; no queue or ring-buffer dependency. */
#define RK_SOURCE_CODE
#include <kbroadcastmesg.h>
#include <kapi.h>
#include <kstring.h>
#include <ktrace.h>
#include <ksystasks.h>

#if (RK_CONF_BROADCAST_MESG == ON)
#define BROADCAST_WAIT_RECV ((UINT)0xB001)
#define BROADCAST_DELIVER ((UINT)0xB002)
#define BROADCAST_WAIT_SEND ((UINT)0xB101)

static RK_ERR kBroadcastMesgResult_(RK_ERR const err)
{
#if (RK_CONF_ERR_CHECK == ON)
    switch (err)
    {
        case RK_ERR_OBJ_NULL: K_ERR_HANDLER(RK_FAULT_OBJ_NULL); break;
        case RK_ERR_OBJ_NOT_INIT: K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT); break;
        case RK_ERR_INVALID_OBJ: K_ERR_HANDLER(RK_FAULT_INVALID_OBJ); break;
        case RK_ERR_OBJ_DOUBLE_INIT: K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT); break;
        case RK_ERR_INVALID_PARAM:
        case RK_ERR_INVALID_MSG_SIZE: K_ERR_HANDLER(RK_FAULT_INVALID_PARAM); break;
        case RK_ERR_INVALID_PRIO: K_ERR_HANDLER(RK_FAULT_TASK_INVALID_PRIO); break;
        case RK_ERR_TASK_INVALID_ST: K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE); break;
        case RK_ERR_INVALID_TIMEOUT: K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT); break;
        case RK_ERR_INVALID_ISR_PRIMITIVE:
            K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE); break;
        default: break;
    }
#endif
    return (err);
}

static RK_ERR kBroadcastMesgCheck_(RK_BROADCAST_MESG const *const kobj)
{
    if (kobj == NULL)
        return (RK_ERR_OBJ_NULL);
    if (kobj->init != RK_TRUE)
        return (RK_ERR_OBJ_NOT_INIT);
    if (kobj->objID != RK_BROADCAST_MESG_KOBJ_ID)
        return (RK_ERR_INVALID_OBJ);
    return (RK_ERR_SUCCESS);
}

RK_ERR kBroadcastMesgInitCeiling(RK_BROADCAST_MESG *const kobj,
                                 VOID *const bufferPtr, ULONG const mesgWords,
                                 RK_PRIO const ceilingPrio)
{
    if ((kobj == NULL) || (bufferPtr == NULL))
        return (kBroadcastMesgResult_(RK_ERR_OBJ_NULL));
    if ((mesgWords != 1UL) && (mesgWords != 2UL) &&
        (mesgWords != 4UL) && (mesgWords != 8UL))
        return (kBroadcastMesgResult_(RK_ERR_INVALID_MSG_SIZE));
    if ((ceilingPrio != RK_BROADCAST_MESG_PRIO_CEILING_NONE) &&
        (ceilingPrio > RK_CONF_MIN_PRIO))
        return (kBroadcastMesgResult_(RK_ERR_INVALID_PRIO));
    RK_CR_AREA
    RK_CR_ENTER
    if (kobj->init == RK_TRUE)
    {
        RK_CR_EXIT
        return (kBroadcastMesgResult_(RK_ERR_OBJ_DOUBLE_INIT));
    }
    kListInit(&kobj->waitingReceivers);
    kListInit(&kobj->waitingSenders);
    kobj->bufferPtr = bufferPtr;
    kobj->mesgWords = mesgWords;
    kobj->activeReceivers = 0UL;
    kobj->joinedTasks = 0U;
    kobj->waitCalls = 0U;
    kobj->prioCeiling = ceilingPrio;
    kobj->prioCeilingEnabled =
        (ceilingPrio != RK_BROADCAST_MESG_PRIO_CEILING_NONE) ? RK_TRUE : RK_FALSE;
    kobj->objID = RK_BROADCAST_MESG_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->init = RK_TRUE;
    kTraceRegisterObject(kobj, RK_BROADCAST_MESG_KOBJ_ID);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kBroadcastMesgInit(RK_BROADCAST_MESG *const kobj, VOID *const bufferPtr,
                          ULONG const mesgWords)
{
    return (kBroadcastMesgInitCeiling(kobj, bufferPtr, mesgWords,
                                      RK_BROADCAST_MESG_PRIO_CEILING_NONE));
}

static RK_ERR kBroadcastMesgJoinCurrent_(RK_BROADCAST_MESG *const kobj)
{
    if (kobj->prioCeilingEnabled != RK_TRUE)
        return (RK_ERR_SUCCESS);
    if (RK_gRunPtr->broadcastCeilingPtr == kobj)
        return (RK_ERR_SUCCESS);
    if (RK_gRunPtr->broadcastCeilingPtr != NULL)
        return (RK_ERR_TASK_INVALID_ST);
    if (RK_gRunPtr->priority < kobj->prioCeiling)
        return (RK_ERR_INVALID_PRIO);
    RK_gRunPtr->broadcastCeilingPtr = kobj;
    kobj->joinedTasks++;
    kTaskUpdateEffectivePrioChain(RK_gRunPtr);
    return (RK_ERR_SUCCESS);
}

static RK_ERR kBroadcastMesgLeaveCurrent_(RK_BROADCAST_MESG *const kobj)
{
    if (RK_gRunPtr->broadcastCeilingPtr == NULL)
        return (RK_ERR_SUCCESS);
    if (RK_gRunPtr->broadcastCeilingPtr != kobj)
        return (RK_ERR_TASK_INVALID_ST);
    RK_gRunPtr->broadcastCeilingPtr = NULL;
    kobj->joinedTasks--;
    kTaskUpdateEffectivePrioChain(RK_gRunPtr);
    return (RK_ERR_SUCCESS);
}

RK_ERR kBroadcastMesgJoin(RK_BROADCAST_MESG *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kBroadcastMesgCheck_(kobj);
    if ((err == RK_ERR_SUCCESS) && (kIsISR() || (RK_gRunPtr == NULL)))
        err = RK_ERR_INVALID_ISR_PRIMITIVE;
    if (err == RK_ERR_SUCCESS)
        err = kBroadcastMesgJoinCurrent_(kobj);
    RK_CR_EXIT
    return (kBroadcastMesgResult_(err));
}

RK_ERR kBroadcastMesgLeave(RK_BROADCAST_MESG *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kBroadcastMesgCheck_(kobj);
    if ((err == RK_ERR_SUCCESS) && (kIsISR() || (RK_gRunPtr == NULL)))
        err = RK_ERR_INVALID_ISR_PRIMITIVE;
    if (err == RK_ERR_SUCCESS)
        err = kBroadcastMesgLeaveCurrent_(kobj);
    RK_CR_EXIT
    return (kBroadcastMesgResult_(err));
}

/* Retain the object through a wait and the READY interval before resumption.
 * Destroy/termination must not invalidate a suspended call's object or frame.
 */
static VOID kBroadcastMesgRetain_(RK_BROADCAST_MESG *const kobj)
{
    if (RK_gRunPtr->broadcastWaitingPtr == NULL)
    {
        RK_gRunPtr->broadcastWaitingPtr = kobj;
        kobj->waitCalls++;
    }
}

static VOID kBroadcastMesgRelease_(RK_BROADCAST_MESG *const kobj)
{
    if (RK_gRunPtr->broadcastWaitingPtr == kobj)
    {
        RK_gRunPtr->broadcastWaitingPtr = NULL;
        kobj->waitCalls--;
    }
    RK_gRunPtr->broadcastMinReceivers = 0U;
}

static VOID kBroadcastMesgReadyTop_(RK_TCB **const chosenTCBPtr,
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

static VOID kBroadcastMesgClearTimeout_(RK_TCB *const taskPtr)
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

static ULONG kBroadcastMesgCountWaiters_(RK_BROADCAST_MESG const *const kobj)
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
        if (taskPtr->timeoutNode.waitInfo == BROADCAST_WAIT_RECV)
        {
            nWaiters++;
        }
        nodePtr = nodePtr->nextPtr;
    }

    return (nWaiters);
}

static VOID kBroadcastMesgWakeSender_(RK_BROADCAST_MESG *const kobj)
{
    if (kobj->activeReceivers != 0UL)
        return;
    ULONG const waiting = kBroadcastMesgCountWaiters_(kobj);
    RK_NODE *node = kobj->waitingSenders.listDummy.nextPtr;
    while (node != &kobj->waitingSenders.listDummy)
    {
        RK_TCB *const task = K_GET_TCB_ADDR(node);
        if (waiting >= (ULONG)task->broadcastMinReceivers)
        {
            if (kWaitQRemove(&kobj->waitingSenders, task) == RK_ERR_SUCCESS)
            {
                kBroadcastMesgClearTimeout_(task);
                task->timeoutNode.waitInfo = 0U;
                task->broadcastMinReceivers = 0U;
                kReadySwtch(task);
            }
            return;
        }
        node = node->nextPtr;
    }
}

static UINT kBroadcastMesgPrepare_(RK_BROADCAST_MESG *const kobj,
                                                 UINT const nTasks)
{
    UINT marked = 0U;
    RK_NODE *nodePtr = kobj->waitingReceivers.listDummy.nextPtr;

    while ((marked < nTasks) && (nodePtr != &kobj->waitingReceivers.listDummy))
    {
        RK_NODE *const nextPtr = nodePtr->nextPtr;
        RK_TCB *const recvTaskPtr = K_GET_TCB_ADDR(nodePtr);

        if (recvTaskPtr->timeoutNode.waitInfo == BROADCAST_WAIT_RECV)
        {
            kBroadcastMesgClearTimeout_(recvTaskPtr);
            recvTaskPtr->timeoutNode.waitInfo =
                BROADCAST_DELIVER;
            marked++;
        }
        nodePtr = nextPtr;
    }

    return (marked);
}

RK_ERR kBroadcastMesgWake(RK_BROADCAST_MESG *const kobj, UINT const nTasks)
{
    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR const checked = kBroadcastMesgCheck_(kobj);
    if (checked != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (kBroadcastMesgResult_(checked));
    }


    kSchLock();
    RK_CR_EXIT

    UINT woken = 0U;
    RK_TCB *chosenTCBPtr = NULL;

    while (woken < nTasks)
    {
        RK_CR_ENTER
        /* Restart after every interrupt-enabled gap: an ISR may have removed
         * or reordered other waiters. Never retain a list cursor across it. */
        RK_NODE *nodePtr = kobj->waitingReceivers.listDummy.nextPtr;
        while (nodePtr != &kobj->waitingReceivers.listDummy)
        {
            RK_TCB const *const taskPtr = K_GET_TCB_ADDR(nodePtr);
            if (taskPtr->timeoutNode.waitInfo == BROADCAST_DELIVER)
                break;
            nodePtr = nodePtr->nextPtr;
        }
        if (nodePtr == &kobj->waitingReceivers.listDummy)
        {
            RK_CR_EXIT
            break;
        }

        RK_TCB *const recvTaskPtr = K_GET_TCB_ADDR(nodePtr);
        RK_ERR const err = kWaitQRemove(&kobj->waitingReceivers, recvTaskPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            kSchUnlock();
            return (kBroadcastMesgResult_(err));
        }
        kBroadcastMesgReadyTop_(&chosenTCBPtr, recvTaskPtr);
        woken++;
        RK_CR_EXIT
#if defined(RK_QEMU_UNIT_TEST)
        if (kPostProcTestBroadcastStep != 0)
            kPostProcTestBroadcastStep(kobj);
#endif
    }

    RK_CR_ENTER
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size);
    if (chosenTCBPtr != NULL)
    {
        /* Select from current ready queues after interrupt-enabled fanout. */
        kReschedRunning();
    }

    RK_CR_EXIT
    kSchUnlock();
    return (RK_ERR_SUCCESS);
}

static RK_ERR kBroadcastMesgCommit_(RK_BROADCAST_MESG *const kobj,
                                      VOID const *const sendPtr, UINT const count,
                                      UINT *const nRecvPtr, RK_BOOL const defer)
{
    if (defer == RK_TRUE)
    {
        RK_ERR const err = kPostProcJobEnq(RK_POSTPROC_JOB_BROADCAST_MESG_WAKE,
                                           kobj, count);
        if (err != RK_ERR_SUCCESS)
            return (err);
    }
    RK_MEMCPY(kobj->bufferPtr, sendPtr, kobj->mesgWords * RK_WORD_SIZE);
    kobj->activeReceivers = (ULONG)count;
    UINT const prepared = kBroadcastMesgPrepare_(kobj, count);
    K_ASSERT(prepared == count);
    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_SUCCESS, (ULONG)prepared);
    if (nRecvPtr != NULL)
        *nRecvPtr = prepared;
    return (RK_ERR_SUCCESS);
}

RK_ERR kBroadcastMesgSend(RK_BROADCAST_MESG *const kobj,
                          VOID const *const sendPtr, UINT *const nRecvPtr)
{
    return (kBroadcastMesgSendWaitN(kobj, sendPtr, 1U, RK_NO_WAIT, nRecvPtr));
}

RK_ERR kBroadcastMesgSendWaitN(RK_BROADCAST_MESG *const kobj,
                               VOID const *const sendPtr,
                               UINT const minReceivers, RK_TICK const timeout,
                               UINT *const nRecvPtr)
{
    if (nRecvPtr != NULL)
        *nRecvPtr = 0U;
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kBroadcastMesgCheck_(kobj);
    if ((err == RK_ERR_SUCCESS) && (sendPtr == NULL))
        err = RK_ERR_OBJ_NULL;
    if ((err == RK_ERR_SUCCESS) &&
        ((minReceivers == 0U) || (minReceivers > RK_NTHREADS)))
        err = RK_ERR_INVALID_PARAM;
    if ((err == RK_ERR_SUCCESS) && (timeout != RK_WAIT_FOREVER) &&
        (timeout > RK_MAX_PERIOD))
        err = RK_ERR_INVALID_TIMEOUT;
    if ((err == RK_ERR_SUCCESS) && K_BLOCKING_ON_ISR(timeout))
        err = RK_ERR_INVALID_ISR_PRIMITIVE;
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (kBroadcastMesgResult_(err));
    }
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), timeout);
    while (1)
    {
        UINT const count = (UINT)kBroadcastMesgCountWaiters_(kobj);
        if ((count >= minReceivers) && (kobj->activeReceivers == 0UL))
        {
            RK_BOOL const defer = ((count > 1U) && kIsISR()) ? RK_TRUE : RK_FALSE;
            if (defer == RK_FALSE)
                kSchLock();
            err = kBroadcastMesgCommit_(kobj, sendPtr, count, nRecvPtr, defer);
            if (!kIsISR())
                kBroadcastMesgRelease_(kobj);
            RK_CR_EXIT
            if (defer == RK_FALSE)
            {
                if (err == RK_ERR_SUCCESS)
                    err = kBroadcastMesgWake(kobj, count);
                kSchUnlock();
            }
            return (kBroadcastMesgResult_(err));
        }
        if (timeout == RK_NO_WAIT)
        {
            err = (count < minReceivers) ? RK_ERR_BUFFER_EMPTY : RK_ERR_BUFFER_FULL;
            RK_CR_EXIT
            return (kBroadcastMesgResult_(err));
        }
        kBroadcastMesgRetain_(kobj);
        if (timeout != RK_WAIT_FOREVER)
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            err = kTimeoutNodeAddUntil(&RK_gRunPtr->timeoutNode, timeout, deadline);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
                kBroadcastMesgRelease_(kobj);
                RK_CR_EXIT
                return (kBroadcastMesgResult_(err));
            }
        }
        RK_gRunPtr->status = RK_SENDING;
        RK_gRunPtr->timeoutNode.waitInfo = BROADCAST_WAIT_SEND;
        RK_gRunPtr->broadcastMinReceivers = minReceivers;
        err = kWaitQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);
        if (err != RK_ERR_SUCCESS)
        {
            kBroadcastMesgClearTimeout_(RK_gRunPtr);
            RK_gRunPtr->timeoutNode.waitInfo = 0U;
            RK_gRunPtr->status = RK_RUNNING;
            kBroadcastMesgRelease_(kobj);
            RK_CR_EXIT
            return (kBroadcastMesgResult_(err));
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_SEND_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingSenders.size);
        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER
        RK_gRunPtr->timeoutNode.waitInfo = 0U;
        RK_gRunPtr->broadcastMinReceivers = 0U;
        if (RK_gRunPtr->timeOut == RK_TRUE)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            kBroadcastMesgRelease_(kobj);
            RK_CR_EXIT
            return (kBroadcastMesgResult_(RK_ERR_TIMEOUT));
        }
        kBroadcastMesgClearTimeout_(RK_gRunPtr);
    }
}

RK_ERR kBroadcastMesgRecv(RK_BROADCAST_MESG *const kobj,
                          VOID *const recvPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kBroadcastMesgCheck_(kobj);
    if ((err == RK_ERR_SUCCESS) && (recvPtr == NULL))
        err = RK_ERR_OBJ_NULL;
    if ((err == RK_ERR_SUCCESS) && (timeout != RK_WAIT_FOREVER) &&
        (timeout > RK_MAX_PERIOD))
        err = RK_ERR_INVALID_TIMEOUT;
    if ((err == RK_ERR_SUCCESS) && K_BLOCKING_ON_ISR(timeout))
        err = RK_ERR_INVALID_ISR_PRIMITIVE;
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (kBroadcastMesgResult_(err));
    }
    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (kBroadcastMesgResult_(RK_ERR_BUFFER_EMPTY));
    }
    RK_TICK const deadline = K_TICK_ADD(kTickGet(), timeout);
    RK_BOOL const lazy = ((kobj->prioCeilingEnabled == RK_TRUE) &&
                          (RK_gRunPtr->broadcastCeilingPtr == NULL))
                             ? RK_TRUE : RK_FALSE;
    err = kBroadcastMesgJoinCurrent_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (kBroadcastMesgResult_(err));
    }
    kBroadcastMesgRetain_(kobj);
    while (RK_gRunPtr->timeoutNode.waitInfo != BROADCAST_DELIVER)
    {
        if (timeout != RK_WAIT_FOREVER)
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            err = kTimeoutNodeAddUntil(&RK_gRunPtr->timeoutNode, timeout, deadline);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
                break;
            }
        }
        RK_gRunPtr->status = RK_RECEIVING;
        RK_gRunPtr->timeoutNode.waitInfo = BROADCAST_WAIT_RECV;
        err = kWaitQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);
        if (err != RK_ERR_SUCCESS)
        {
            kBroadcastMesgClearTimeout_(RK_gRunPtr);
            RK_gRunPtr->status = RK_RUNNING;
            break;
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_RECV_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingReceivers.size);
        kBroadcastMesgWakeSender_(kobj);
        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER
        if (RK_gRunPtr->timeOut == RK_TRUE)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            err = RK_ERR_TIMEOUT;
            break;
        }
        kBroadcastMesgClearTimeout_(RK_gRunPtr);
    }
    if (err == RK_ERR_SUCCESS)
    {
        K_ASSERT(kobj->activeReceivers > 0UL);
        RK_MEMCPY(recvPtr, kobj->bufferPtr, kobj->mesgWords * RK_WORD_SIZE);
        kobj->activeReceivers--;
        kBroadcastMesgWakeSender_(kobj);
    }
    RK_gRunPtr->timeoutNode.waitInfo = 0U;
    kBroadcastMesgRelease_(kobj);
    if (lazy == RK_TRUE)
        kBroadcastMesgLeaveCurrent_(kobj);
    kTraceRecordObject(kobj, RK_TRACE_OP_RECV, err, kobj->activeReceivers);
    RK_CR_EXIT
    return (kBroadcastMesgResult_(err));
}

RK_ERR kBroadcastMesgQuery(RK_BROADCAST_MESG const *const kobj,
                           UINT *const waitingReceivers,
                           UINT *const waitingSenders,
                           UINT *const activeReceivers)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_ERR err = kBroadcastMesgCheck_(kobj);
    if ((err == RK_ERR_SUCCESS) && (waitingReceivers == NULL) &&
        (waitingSenders == NULL) && (activeReceivers == NULL))
        err = RK_ERR_OBJ_NULL;
    if (err == RK_ERR_SUCCESS)
    {
        if (waitingReceivers != NULL)
            *waitingReceivers = (UINT)kBroadcastMesgCountWaiters_(kobj);
        if (waitingSenders != NULL)
            *waitingSenders = (UINT)kobj->waitingSenders.size;
        if (activeReceivers != NULL)
            *activeReceivers = (UINT)kobj->activeReceivers;
    }
    RK_CR_EXIT
    return (kBroadcastMesgResult_(err));
}
#endif /* RK_CONF_BROADCAST_MESG */
