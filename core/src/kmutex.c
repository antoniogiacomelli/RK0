/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.60.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MUTEX LOCK                                                      */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktimer.h>
#include <ksch.h>
#include <ktrace.h>
#include <kmutex.h>

#if (RK_CONF_MUTEX == ON)

/******************************************************************************/
/* MUTEX LIST                                                                 */
/******************************************************************************/
static inline RK_ERR kMutexListAdd(struct RK_STRUCT_LIST *ownedMutexList,
                                   struct RK_STRUCT_LIST_NODE *mutexNode)
{
    RK_DSB
    return kListAddTail(ownedMutexList, mutexNode);
}

static inline RK_ERR kMutexListRem(struct RK_STRUCT_LIST *ownedMutexList,
                                   struct RK_STRUCT_LIST_NODE *mutexNode)
{
    RK_DSB
    return kListRemove(ownedMutexList, mutexNode);
}

/******************************************************************************/
/* PRIORITY INHERITANCE                                                       */
/******************************************************************************/
static RK_PRIO kMutexOwnedPipPrio_(RK_TCB const *const ownerTcb,
                                   RK_PRIO const currentPrio)
{
    RK_PRIO newPrio = currentPrio;
    RK_NODE *node = ownerTcb->ownedMutexList.listDummy.nextPtr;

    while (node != &ownerTcb->ownedMutexList.listDummy)
    {
        RK_MUTEX *mtxPtr = K_GET_CONTAINER_ADDR(node, RK_MUTEX, mutexNode);

        if ((mtxPtr->protocol == RK_PRIO_INHERITANCE) &&
            (mtxPtr->waitingQueue.size > 0UL))
        {
            RK_TCB const *waiterPtr = kTCBQPeek(&mtxPtr->waitingQueue);
            if ((waiterPtr != NULL) && (waiterPtr->priority < newPrio))
            {
                newPrio = waiterPtr->priority;
            }
        }

        node = node->nextPtr;
        RK_BARRIER
    }

    return (newPrio);
}

static VOID kMutexSetTaskPrio_(RK_TCB *const tcbPtr,
                               RK_PRIO const newPrio)
{
    if (tcbPtr->priority == newPrio)
    {
        return;
    }

    RK_PRIO const oldPrio = tcbPtr->priority;

    if (tcbPtr->status == RK_READY)
    {
        RK_TCB *remPtr = tcbPtr;
        RK_ERR err = kTCBQRem(&RK_gReadyQueue[oldPrio], &remPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);

        tcbPtr->priority = newPrio;
        kTraceRecordTaskPrio(tcbPtr, oldPrio, newPrio);

        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);

        kReschedTask(tcbPtr);
    }
    else
    {
        tcbPtr->priority = newPrio;
        kTraceRecordTaskPrio(tcbPtr, oldPrio, newPrio);
        if (tcbPtr->status == RK_RUNNING)
        {
            kReschedRunning();
        }
    }
}

static VOID kMutexUpdateOwnerPrio_(RK_TCB *ownerTcb)
{
    RK_DSB

    RK_TCB *currTcbPtr = ownerTcb;

    while (currTcbPtr != NULL)
    {
        RK_PRIO const newPrio =
            kMutexOwnedPipPrio_(currTcbPtr, currTcbPtr->prioNominal);

        if (currTcbPtr->priority == newPrio)
        {
            break;
        }

        kMutexSetTaskPrio_(currTcbPtr, newPrio);
        RK_BARRIER

        if ((currTcbPtr->status != RK_BLOCKED) ||
            (currTcbPtr->waitingForMutexPtr == NULL))
        {
            break;
        }

        RK_LIST *waitQueuePtr = currTcbPtr->timeoutNode.waitingQueuePtr;
        if ((waitQueuePtr != NULL) && (waitQueuePtr->size > 1UL))
        {
            RK_TCB *requeuePtr = currTcbPtr;
            RK_ERR err = kTCBQRem(waitQueuePtr, &requeuePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);

            err = kTCBQEnqByPrio(waitQueuePtr, requeuePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);
        }

        RK_MUTEX *const waitMtxPtr = currTcbPtr->waitingForMutexPtr;
        if ((waitMtxPtr->protocol != RK_PRIO_INHERITANCE) ||
            (waitMtxPtr->ownerPtr == NULL))
        {
            break;
        }

        currTcbPtr = waitMtxPtr->ownerPtr;
    }

    RK_ISB
}

VOID kMutexTimeoutWaiter(RK_TCB *const waiterPtr)
{
    if ((waiterPtr == NULL) || (waiterPtr->waitingForMutexPtr == NULL))
    {
        return;
    }

    RK_MUTEX *const mtxPtr = waiterPtr->waitingForMutexPtr;
    if ((mtxPtr->protocol == RK_PRIO_INHERITANCE) &&
        (mtxPtr->ownerPtr != NULL))
    {
        kMutexUpdateOwnerPrio_(mtxPtr->ownerPtr);
    }
}

/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/
/* There is no recursive lock. Unlocking a mutex you do not own hard-faults. */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT const protocol)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_ERROR);
    }

    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
#endif

    if ((protocol != RK_PRIO_NONE) && (protocol != RK_PRIO_INHERITANCE))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = RK_TRUE;
    kobj->protocol = protocol;
    kobj->objID = RK_MUTEX_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->lock = RK_FALSE;
    kobj->ownerPtr = NULL;
    kTraceRegisterObject(kobj, RK_MUTEX_KOBJ_ID);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMutexLock(RK_MUTEX *const kobj, RK_TICK const timeout)
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

    if (kobj->objID != RK_MUTEX_KOBJ_ID)
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

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    if (kobj->lock == RK_FALSE)
    {
        kobj->lock = RK_TRUE;
        kobj->ownerPtr = RK_gRunPtr;
        kMutexListAdd(&RK_gRunPtr->ownedMutexList, &kobj->mutexNode);
        kTraceRecordObject(kobj, RK_TRACE_OP_LOCK, RK_ERR_SUCCESS,
                           kobj->waitingQueue.size);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if ((kobj->ownerPtr != RK_gRunPtr) && (kobj->ownerPtr != NULL))
    {
        if (timeout == 0UL)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_LOCK, RK_ERR_MUTEX_LOCKED,
                               kobj->waitingQueue.size);
            RK_CR_EXIT
            return (RK_ERR_MUTEX_LOCKED);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                kTraceRecordObject(kobj, RK_TRACE_OP_LOCK, err,
                                   kobj->waitingQueue.size);
                RK_CR_EXIT
                return (err);
            }
        }
        if (timeout == RK_WAIT_FOREVER)
        {
            RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
        }

        kTraceRecordObject(kobj, RK_TRACE_OP_LOCK_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingQueue.size + 1UL);
        kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

        RK_gRunPtr->status = RK_BLOCKED;
        RK_gRunPtr->waitingForMutexPtr = kobj;
        if (kobj->protocol == RK_PRIO_INHERITANCE)
        {
            kMutexUpdateOwnerPrio_(kobj->ownerPtr);
        }

        kPendCtxSwtch();

        RK_CR_EXIT

        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            if ((kobj->protocol == RK_PRIO_INHERITANCE) &&
                (kobj->ownerPtr != NULL))
            {
                kMutexUpdateOwnerPrio_(kobj->ownerPtr);
            }

            RK_gRunPtr->timeOut = RK_FALSE;
            RK_gRunPtr->waitingForMutexPtr = NULL;

            kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                               kobj->waitingQueue.size);
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        }
    }
    else if (kobj->ownerPtr == RK_gRunPtr)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_MUTEX_REC_LOCK);
#endif
        RK_CR_EXIT
        return (RK_ERR_MUTEX_REC_LOCK);
    }

    kTraceRecordObject(kobj, RK_TRACE_OP_LOCK, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMutexUnlock(RK_MUTEX *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TCB *tcbPtr = NULL;

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MUTEX_KOBJ_ID)
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

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (kobj->lock == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_MUTEX_NOT_LOCKED);
        RK_CR_EXIT
        return (RK_ERR_MUTEX_NOT_LOCKED);
    }

    if (kobj->ownerPtr != RK_gRunPtr)
    {
        K_ERR_HANDLER(RK_FAULT_UNLOCK_OWNED_MUTEX);
        RK_CR_EXIT
        return (RK_ERR_MUTEX_NOT_OWNER);
    }

#endif

    kMutexListRem(&(RK_gRunPtr->ownedMutexList), &(kobj->mutexNode));

    if (kobj->waitingQueue.size == 0UL)
    {
        kobj->lock = RK_FALSE;
        kobj->ownerPtr = NULL;

        if (kobj->protocol == RK_PRIO_INHERITANCE)
        {
            kMutexUpdateOwnerPrio_(RK_gRunPtr);
            RK_BARRIER
        }

        kTraceRecordObject(kobj, RK_TRACE_OP_UNLOCK, RK_ERR_SUCCESS, 0UL);
    }
    else
    {
        kTCBQDeq(&(kobj->waitingQueue), &tcbPtr);
        if (tcbPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&tcbPtr->timeoutNode);
            tcbPtr->timeoutNode.timeoutType = 0;
        }
        tcbPtr->timeoutNode.waitingQueuePtr = NULL;
        kobj->ownerPtr = tcbPtr;
        kMutexListAdd(&(tcbPtr->ownedMutexList), &(kobj->mutexNode));
        kobj->lock = RK_TRUE;
        tcbPtr->waitingForMutexPtr = NULL;

        if (kobj->protocol == RK_PRIO_INHERITANCE)
        {
            kMutexUpdateOwnerPrio_(RK_gRunPtr);
            kMutexUpdateOwnerPrio_(tcbPtr);
        }

        kTraceRecordObject(kobj, RK_TRACE_OP_UNLOCK, RK_ERR_SUCCESS,
                           kobj->waitingQueue.size);
        kReadySwtch(tcbPtr);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMutexQuery(RK_MUTEX const *const kobj, UINT *const statePtr)
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

    if (kobj->objID != RK_MUTEX_KOBJ_ID)
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

    if (statePtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    *statePtr = ((UINT)kobj->lock);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* mutex */
