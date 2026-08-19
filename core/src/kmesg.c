/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.72.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: ASYNCHRONOUS DIRECT MESSAGE                                     */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmesg.h>
#include <kmem.h>
#include <ksch.h>
#include <ktimer.h>
#include <ktrace.h>
#include <kerr.h>

#if ((RK_CONF_ASYNCH_MESG == ON) && (RK_CONF_MESG_QUEUE == ON))

#ifndef K_GET_MESG_ADDR
#define K_GET_MESG_ADDR(nodePtr) K_GET_CONTAINER_ADDR(nodePtr, RK_MESG, mesgNode)
#endif

static inline ULONG kMesgBlockBytes_(ULONG const payloadBytes)
{
    if ((payloadBytes == 0UL) ||
        (payloadBytes > (RK_ULONG_MAX - sizeof(RK_MESG) -
                         (RK_WORD_SIZE - 1UL))))
    {
        return (0UL);
    }

    return ((sizeof(RK_MESG) + payloadBytes + RK_WORD_SIZE - 1UL) &
            ~(RK_WORD_SIZE - 1UL));
}

static inline RK_BOOL kMesgIsValid_(RK_MESG const *const mesgPtr)
{
    return (((mesgPtr != NULL) && (mesgPtr->objID == RK_MESG_KOBJ_ID))
                ? RK_TRUE
                : RK_FALSE);
}

static inline RK_BOOL kMesgStateOwned_(RK_MESG const *const mesgPtr)
{
    return (((mesgPtr->state == RK_MESG_STATE_ALLOCATED) ||
             (mesgPtr->state == RK_MESG_STATE_RECEIVED))
                ? RK_TRUE
                : RK_FALSE);
}

static inline RK_BOOL kMesgStateHasSender_(RK_MESG const *const mesgPtr)
{
    return (((mesgPtr->state == RK_MESG_STATE_QUEUED) ||
             (mesgPtr->state == RK_MESG_STATE_RECEIVED))
                ? RK_TRUE
                : RK_FALSE);
}

static inline RK_ERR kMesgPublicReadyErr_(RK_ERR const err)
{
    if ((err == RK_ERR_RESCHED_PENDING) ||
        (err == RK_ERR_RESCHED_NOT_NEEDED))
    {
        return (RK_ERR_SUCCESS);
    }

    return (err);
}

static inline VOID kMesgClearWait_(RK_TCB *const taskPtr)
{
    taskPtr->asynchMesgWaitSenderPtr = NULL;
    taskPtr->asynchMesgWaitDestPtr = NULL;
    taskPtr->asynchMesgWaitStatus = RK_ERR_SUCCESS;
}

static inline VOID kMesgClearAllocWait_(RK_TCB *const taskPtr)
{
    taskPtr->asynchMesgAllocDestPtr = NULL;
    taskPtr->timeoutNode.waitingQueuePtr = NULL;
}

static VOID kMesgSetOwner_(RK_MESG *const mesgPtr,
                           RK_TASK_HANDLE const ownerPtr)
{
    RK_TASK_HANDLE const oldOwnerPtr = mesgPtr->owner;

    if (oldOwnerPtr == ownerPtr)
    {
        return;
    }

    if (oldOwnerPtr != NULL)
    {
        RK_ERR const err =
            kListRemove(&oldOwnerPtr->asynchMesgOwnedList,
                        &mesgPtr->ownerNode);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }

    mesgPtr->owner = ownerPtr;
    mesgPtr->ownerNode.nextPtr = NULL;
    mesgPtr->ownerNode.prevPtr = NULL;

    if (ownerPtr != NULL)
    {
        RK_ERR const err =
            kListAddTail(&ownerPtr->asynchMesgOwnedList,
                         &mesgPtr->ownerNode);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }

    if (oldOwnerPtr != NULL)
    {
        kTaskUpdateEffectivePrioChain(oldOwnerPtr);
    }

    if (ownerPtr != NULL)
    {
        kTaskUpdateEffectivePrioChain(ownerPtr);
    }
}

static inline VOID kMesgPrepareAllocated_(RK_MESG *const mesgPtr,
                                          RK_MEM_PARTITION *const poolPtr,
                                          RK_TASK_HANDLE const ownerPtr)
{
    if (mesgPtr->objID != RK_MESG_KOBJ_ID)
    {
        mesgPtr->ownerNode.nextPtr = NULL;
        mesgPtr->ownerNode.prevPtr = NULL;
        mesgPtr->owner = NULL;
    }

    mesgPtr->mesgNode.nextPtr = NULL;
    mesgPtr->mesgNode.prevPtr = NULL;
    mesgPtr->poolPtr = poolPtr;
    mesgPtr->sender = NULL;
    mesgPtr->receiver = NULL;
    mesgPtr->payloadBytes = poolPtr->blkSize - sizeof(RK_MESG);
    mesgPtr->senderPid = 0U;
    mesgPtr->receiverPid = 0U;
    mesgPtr->state = RK_MESG_STATE_ALLOCATED;
    mesgPtr->objID = RK_MESG_KOBJ_ID;
    kMesgSetOwner_(mesgPtr, ownerPtr);
}

static RK_ERR kMesgAllocFromPool_(RK_MEM_PARTITION *const poolPtr,
                                  RK_MESG **const mesgPtrPtr)
{
    RK_MESG *const mesgPtr = (RK_MESG *)kMemPartitionAlloc(poolPtr);
    if (mesgPtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    kMesgPrepareAllocated_(mesgPtr, poolPtr, RK_gRunPtr);
    *mesgPtrPtr = mesgPtr;
    return (RK_ERR_SUCCESS);
}

static RK_ERR kMesgDeliverFreedToAllocator_(RK_MEM_PARTITION *const poolPtr,
                                            RK_MESG *const mesgPtr)
{
    RK_TCB *allocatorPtr = NULL;
    RK_ERR err = kTCBQDeq(&poolPtr->waitingQueue, &allocatorPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    if (allocatorPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        kRemoveTimeoutNode(&allocatorPtr->timeoutNode);
        allocatorPtr->timeoutNode.timeoutType = 0U;
    }
    allocatorPtr->timeoutNode.waitingQueuePtr = NULL;

    K_ASSERT(allocatorPtr->asynchMesgAllocDestPtr != NULL);
    if (allocatorPtr->asynchMesgAllocDestPtr == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }

    kMesgPrepareAllocated_(mesgPtr, poolPtr, allocatorPtr);
    *(allocatorPtr->asynchMesgAllocDestPtr) = mesgPtr;
    kMesgClearAllocWait_(allocatorPtr);

    err = kMesgPublicReadyErr_(kReadySwtch(allocatorPtr));
    kTraceRecordObject(poolPtr, RK_TRACE_OP_WAKE, err,
                       poolPtr->waitingQueue.size);
    return (err);
}

static inline RK_BOOL kMesgSenderMatches_(RK_MESG const *const mesgPtr,
                                          RK_TASK_HANDLE const fromTaskHandle)
{
    if (fromTaskHandle == RK_ANY_TASK)
    {
        return (RK_TRUE);
    }

    return (((mesgPtr->sender == fromTaskHandle) &&
             (mesgPtr->senderPid == fromTaskHandle->pid))
                ? RK_TRUE
                : RK_FALSE);
}

static RK_ERR kMesgValidateFilter_(RK_TASK_HANDLE const fromTaskHandle)
{
    if (fromTaskHandle == RK_ANY_TASK)
    {
        return (RK_ERR_SUCCESS);
    }

    if (fromTaskHandle == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    if (fromTaskHandle->init != RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
#endif
        return (RK_ERR_OBJ_NOT_INIT);
    }

    return (RK_ERR_SUCCESS);
}

static RK_MESG *kMesgDequeueMatching_(RK_TCB *const receiverPtr,
                                      RK_TASK_HANDLE const fromTaskHandle)
{
    RK_NODE *nodePtr = receiverPtr->asynchMesgQueue.listDummy.nextPtr;

    while (nodePtr != &receiverPtr->asynchMesgQueue.listDummy)
    {
        RK_NODE *const nextPtr = nodePtr->nextPtr;
        RK_MESG *const mesgPtr = K_GET_MESG_ADDR(nodePtr);

        if (kMesgSenderMatches_(mesgPtr, fromTaskHandle) == RK_TRUE)
        {
            RK_ERR const err =
                kListRemove(&receiverPtr->asynchMesgQueue, nodePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);
            mesgPtr->state = RK_MESG_STATE_RECEIVED;
            mesgPtr->receiver = receiverPtr;
            mesgPtr->receiverPid = receiverPtr->pid;
            return (mesgPtr);
        }

        nodePtr = nextPtr;
        RK_BARRIER
    }

    return (NULL);
}

static RK_BOOL kMesgReceiverWaitMatches_(RK_TCB const *const receiverPtr,
                                         RK_MESG const *const mesgPtr)
{
    if ((receiverPtr == NULL) || (receiverPtr->asynchMesgWaitDestPtr == NULL))
    {
        return (RK_FALSE);
    }

    return (kMesgSenderMatches_(mesgPtr,
                                receiverPtr->asynchMesgWaitSenderPtr));
}

static RK_ERR kMesgDisarmBlockingTimeout_(RK_TCB *const taskPtr)
{
    if (taskPtr->timeoutNode.timeoutType != RK_TIMEOUT_BLOCKING)
    {
        return (RK_ERR_SUCCESS);
    }

    return (kTimeoutNodeDisarm(&taskPtr->timeoutNode));
}

static RK_BOOL kMesgDeliverToWaiter_(RK_TCB *const receiverPtr,
                                     RK_MESG *const mesgPtr,
                                     RK_ERR *const errPtr)
{
    if ((receiverPtr->asynchMesgWaiters.size == 0UL) ||
        (kMesgReceiverWaitMatches_(receiverPtr, mesgPtr) == RK_FALSE))
    {
        return (RK_FALSE);
    }

    RK_TCB *waiterPtr = kTCBQPeek(&receiverPtr->asynchMesgWaiters);
    K_ASSERT(waiterPtr == receiverPtr);
    RK_ERR err = kTCBQDeq(&receiverPtr->asynchMesgWaiters, &waiterPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
    {
        *errPtr = err;
        return (RK_TRUE);
    }

    err = kMesgDisarmBlockingTimeout_(receiverPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
    {
        *errPtr = err;
        return (RK_TRUE);
    }

    mesgPtr->state = RK_MESG_STATE_RECEIVED;
    mesgPtr->receiver = receiverPtr;
    mesgPtr->receiverPid = receiverPtr->pid;
    *(receiverPtr->asynchMesgWaitDestPtr) = mesgPtr;
    receiverPtr->asynchMesgWaitStatus = RK_ERR_SUCCESS;
    kMesgClearWait_(receiverPtr);

    *errPtr = kMesgPublicReadyErr_(kReadySwtch(receiverPtr));
    return (RK_TRUE);
}

RK_ERR kMesgEndpointInit(RK_TASK_HANDLE const taskHandle)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (kIsISR())
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (taskHandle == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle->init != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (taskHandle->asynchMesgInit == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#if (RK_CONF_SYNCH_MESG == ON)
    if (taskHandle->synchMesgMaxBytes != 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }
#endif

    taskHandle->asynchMesgInit = RK_TRUE;
    kListInit(&taskHandle->asynchMesgQueue);
    kListInit(&taskHandle->asynchMesgWaiters);
    kMesgClearWait_(taskHandle);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgPoolInit(RK_MEM_PARTITION *const poolPtr,
                     VOID *const memPoolPtr,
                     ULONG const payloadBytes,
                     ULONG const nMesg,
                     RK_PRIO const ceilingPrio)
{
    ULONG const blockBytes = kMesgBlockBytes_(payloadBytes);

    if (blockBytes == 0UL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_INVALID_PARAM);
    }

    if ((ceilingPrio != RK_MESG_PRIO_CEILING_NONE) &&
        (ceilingPrio > RK_CONF_MIN_PRIO))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_PRIO);
#endif
        return (RK_ERR_INVALID_PRIO);
    }

    RK_ERR const err = kMemPartitionInit(poolPtr, memPoolPtr, blockBytes,
                                         nMesg);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    if (ceilingPrio == RK_MESG_PRIO_CEILING_NONE)
    {
        poolPtr->mesgPrioCeiling = RK_MESG_PRIO_CEILING_NONE;
        poolPtr->mesgPrioCeilingEnabled = RK_FALSE;
    }
    else
    {
        poolPtr->mesgPrioCeiling = ceilingPrio;
        poolPtr->mesgPrioCeilingEnabled = RK_TRUE;
    }

    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgAlloc(RK_MEM_PARTITION *const poolPtr,
                  RK_MESG **const mesgPtrPtr,
                  RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((poolPtr == NULL) || (mesgPtrPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (poolPtr->objID != RK_MEMALLOC_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (poolPtr->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }
#endif

    if (mesgPtrPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    *mesgPtrPtr = NULL;

    if (poolPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (poolPtr->objID != RK_MEMALLOC_KOBJ_ID)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (poolPtr->init != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (poolPtr->blkSize <= sizeof(RK_MESG))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if ((kIsISR()) && (timeout != RK_NO_WAIT))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((RK_gRunPtr == NULL) && (timeout != RK_NO_WAIT))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    RK_ERR err = kMesgAllocFromPool_(poolPtr, mesgPtrPtr);
    if (err == RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }
    if ((err != RK_ERR_BUFFER_EMPTY) || (timeout == RK_NO_WAIT))
    {
        RK_CR_EXIT
        return (err);
    }

    while (*mesgPtrPtr == NULL)
    {
        RK_gRunPtr->timeoutNode.waitingQueuePtr = &poolPtr->waitingQueue;
        if (timeout != RK_WAIT_FOREVER)
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_ERR const timeoutErr =
                kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (timeoutErr != RK_ERR_SUCCESS)
            {
                kTimeoutNodeReset(&RK_gRunPtr->timeoutNode);
                RK_CR_EXIT
                return (timeoutErr);
            }
        }

        RK_gRunPtr->status = RK_BLOCKED;
        RK_gRunPtr->asynchMesgAllocDestPtr = mesgPtrPtr;
        kTraceRecordObject(poolPtr, RK_TRACE_OP_WAIT_BLOCK, RK_ERR_SUCCESS,
                           poolPtr->waitingQueue.size + 1UL);
        err = kTCBQEnqByPrio(&poolPtr->waitingQueue, RK_gRunPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        if (err != RK_ERR_SUCCESS)
        {
            if (timeout != RK_WAIT_FOREVER)
            {
                RK_ERR const disarmErr =
                    kTimeoutNodeDisarm(&RK_gRunPtr->timeoutNode);
                K_ASSERT(disarmErr == RK_ERR_SUCCESS);
            }
            kMesgClearAllocWait_(RK_gRunPtr);
            RK_gRunPtr->status = RK_RUNNING;
            RK_CR_EXIT
            return (err);
        }

        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            kMesgClearAllocWait_(RK_gRunPtr);
            kTraceRecordObject(poolPtr, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                               poolPtr->waitingQueue.size);
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if (*mesgPtrPtr != NULL)
        {
            kTraceRecordObject(poolPtr, RK_TRACE_OP_ALLOC, RK_ERR_SUCCESS,
                               poolPtr->nFreeBlocks);
            RK_CR_EXIT
            return (RK_ERR_SUCCESS);
        }

        err = kMesgAllocFromPool_(poolPtr, mesgPtrPtr);
        if (err == RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (RK_ERR_SUCCESS);
        }
        if (err != RK_ERR_BUFFER_EMPTY)
        {
            RK_CR_EXIT
            return (err);
        }
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgFree(RK_MESG *const mesgPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (mesgPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (mesgPtr->objID != RK_MESG_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    if (mesgPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kMesgStateOwned_(mesgPtr) == RK_FALSE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_MESG_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_MESG_INVALID_STATE);
    }

    RK_MEM_PARTITION *const poolPtr = mesgPtr->poolPtr;
    if (poolPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (poolPtr->waitingQueue.size > 0UL)
    {
        RK_ERR const err = kMesgDeliverFreedToAllocator_(poolPtr, mesgPtr);
        RK_CR_EXIT
        return (err);
    }

    kMesgSetOwner_(mesgPtr, NULL);
    mesgPtr->sender = NULL;
    mesgPtr->receiver = NULL;
    mesgPtr->senderPid = 0U;
    mesgPtr->receiverPid = 0U;
    mesgPtr->state = RK_MESG_STATE_FREE;
    mesgPtr->objID = RK_INVALID_KOBJ;
    RK_ERR const err = kMemPartitionFree(poolPtr, mesgPtr);

    RK_CR_EXIT
    return (err);
}

VOID *kMesgPayload(RK_MESG *const mesgPtr)
{
    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        return (NULL);
    }

    return ((VOID *)((BYTE *)mesgPtr + sizeof(RK_MESG)));
}

VOID const *kMesgPayloadConst(RK_MESG const *const mesgPtr)
{
    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        return (NULL);
    }

    return ((VOID const *)((BYTE const *)mesgPtr + sizeof(RK_MESG)));
}

ULONG kMesgPayloadBytes(RK_MESG const *const mesgPtr)
{
    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        return (0UL);
    }

    return (mesgPtr->payloadBytes);
}

RK_TASK_HANDLE kMesgGetSenderHandle(RK_MESG const *const mesgPtr)
{
    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        return (NULL);
    }

    return (mesgPtr->sender);
}

RK_ERR kMesgGetSenderID(RK_MESG const *const mesgPtr,
                        RK_PID *const senderIDPtr)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((mesgPtr == NULL) || (senderIDPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    if (mesgPtr->objID != RK_MESG_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    if ((mesgPtr == NULL) || (senderIDPtr == NULL))
    {
        return (RK_ERR_OBJ_NULL);
    }

    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        return (RK_ERR_INVALID_OBJ);
    }

    if (kMesgStateHasSender_(mesgPtr) == RK_FALSE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_MESG_INVALID_STATE);
#endif
        return (RK_ERR_MESG_INVALID_STATE);
    }

    *senderIDPtr = mesgPtr->senderPid;
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgSend(RK_TASK_HANDLE const taskHandle,
                 RK_MESG *const mesgPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((taskHandle == NULL) || (mesgPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle == RK_ANY_TASK)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (taskHandle->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if ((taskHandle == NULL) || (mesgPtr == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle == RK_ANY_TASK)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if ((kIsISR()) || (RK_gRunPtr == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (taskHandle->init != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (taskHandle->asynchMesgInit != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kMesgIsValid_(mesgPtr) == RK_FALSE)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kMesgStateOwned_(mesgPtr) == RK_FALSE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_MESG_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_MESG_INVALID_STATE);
    }

    mesgPtr->sender = RK_gRunPtr;
    mesgPtr->senderPid = RK_gRunPtr->pid;
    mesgPtr->receiver = taskHandle;
    mesgPtr->receiverPid = taskHandle->pid;
    kMesgSetOwner_(mesgPtr, taskHandle);

    RK_ERR err = RK_ERR_SUCCESS;
    if (kMesgDeliverToWaiter_(taskHandle, mesgPtr, &err) == RK_TRUE)
    {
        kTraceRecordObject(mesgPtr->poolPtr, RK_TRACE_OP_SEND, err,
                           taskHandle->asynchMesgQueue.size);
        RK_CR_EXIT
        return (err);
    }

    mesgPtr->state = RK_MESG_STATE_QUEUED;
    err = kListAddTail(&taskHandle->asynchMesgQueue, &mesgPtr->mesgNode);
    kTraceRecordObject(mesgPtr->poolPtr, RK_TRACE_OP_SEND, err,
                       taskHandle->asynchMesgQueue.size);

    RK_CR_EXIT
    return (err);
}

RK_ERR kMesgWait(RK_TASK_HANDLE const fromTaskHandle,
                 RK_MESG **const mesgPtrPtr,
                 RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (mesgPtrPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if ((kIsISR()) || (RK_gRunPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (RK_gRunPtr->asynchMesgInit != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }
#endif

    if (mesgPtrPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if ((kIsISR()) || (RK_gRunPtr == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    *mesgPtrPtr = NULL;

    RK_ERR err = kMesgValidateFilter_(fromTaskHandle);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (RK_gRunPtr->asynchMesgInit != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    while (*mesgPtrPtr == NULL)
    {
        *mesgPtrPtr = kMesgDequeueMatching_(RK_gRunPtr, fromTaskHandle);
        if (*mesgPtrPtr != NULL)
        {
            kTraceRecordObject((*mesgPtrPtr)->poolPtr, RK_TRACE_OP_RECV,
                               RK_ERR_SUCCESS,
                               RK_gRunPtr->asynchMesgQueue.size);
            RK_CR_EXIT
            return (RK_ERR_SUCCESS);
        }

        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        if (timeout != RK_WAIT_FOREVER)
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_gRunPtr->timeoutNode.waitingQueuePtr =
                &RK_gRunPtr->asynchMesgWaiters;
            RK_ERR const timeoutErr =
                kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (timeoutErr != RK_ERR_SUCCESS)
            {
                kTimeoutNodeReset(&RK_gRunPtr->timeoutNode);
                RK_CR_EXIT
                return (timeoutErr);
            }
        }

        RK_gRunPtr->status = RK_RECEIVING;
        RK_gRunPtr->asynchMesgWaitSenderPtr = fromTaskHandle;
        RK_gRunPtr->asynchMesgWaitDestPtr = mesgPtrPtr;
        RK_gRunPtr->asynchMesgWaitStatus = RK_ERR_SUCCESS;
        err = kTCBQEnq(&RK_gRunPtr->asynchMesgWaiters, RK_gRunPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        if (err != RK_ERR_SUCCESS)
        {
            if (timeout != RK_WAIT_FOREVER)
            {
                RK_ERR const disarmErr =
                    kTimeoutNodeDisarm(&RK_gRunPtr->timeoutNode);
                K_ASSERT(disarmErr == RK_ERR_SUCCESS);
            }
            kMesgClearWait_(RK_gRunPtr);
            RK_gRunPtr->status = RK_RUNNING;
            RK_CR_EXIT
            return (err);
        }

        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            kMesgClearWait_(RK_gRunPtr);
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if (*mesgPtrPtr != NULL)
        {
            kTraceRecordObject((*mesgPtrPtr)->poolPtr, RK_TRACE_OP_RECV,
                               RK_ERR_SUCCESS,
                               RK_gRunPtr->asynchMesgQueue.size);
            kMesgClearWait_(RK_gRunPtr);
            RK_CR_EXIT
            return (RK_ERR_SUCCESS);
        }
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_ASYNCH_MESG && RK_CONF_MESG_QUEUE */
