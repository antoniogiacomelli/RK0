/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MUTEX SEMAPHORE                                                 */
/******************************************************************************/

#define RK_SOURCE_CODE


#include <ktimer.h>
#include <ksch.h>
#include <ktrace.h>


#if (RK_CONF_MUTEX == ON)

/******************************************************************************/
/* MUTEX LIST                                                                 */
/******************************************************************************/
RK_FORCE_INLINE
static inline RK_ERR kMutexListAdd(struct RK_STRUCT_LIST *ownedMutexList,
                                   struct RK_STRUCT_LIST_NODE *mutexNode)
{
    RK_DSB
    return kListAddTail(ownedMutexList, mutexNode);
}

RK_FORCE_INLINE
static inline RK_ERR kMutexListRem(struct RK_STRUCT_LIST *ownedMutexList,
                                   struct RK_STRUCT_LIST_NODE *mutexNode)
{
    RK_DSB
    return kListRemove(ownedMutexList, mutexNode);
}

/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/
/* this implements the priority inheritance invariant: */
/* task prio = max_prio(prio of all tasks it is blocking)   */
/* s.t. max_prio(n,m) = min(n,m) */

RK_FORCE_INLINE
static inline void kMutexUpdateOwnerPrio_(struct RK_OBJ_TCB *ownerTcb)
{
    RK_DSB

    /*
     * A task may own several mutexes, but it may wait for
     * at most one mutex.
     */
    struct RK_OBJ_TCB *currTcbPtr = ownerTcb;

    while (currTcbPtr != NULL)
    {
        /*
         * Begin with the nominal priority so the same calculation
         * also covers the case in which there is no PI contribution.
         */
        RK_PRIO newPrio = currTcbPtr->prioNominal;

        RK_NODE *node =
            currTcbPtr->ownedMutexList.listDummy.nextPtr;

        /*
         * Determine the local PI contribution from all PI-enabled
         * mutexes owned by this task.
         */
        while (node != &currTcbPtr->ownedMutexList.listDummy)
        {
            RK_MUTEX *mtxPtr =
                K_GET_CONTAINER_ADDR(node, RK_MUTEX, mutexNode);

            if ((mtxPtr->prioInh != 0U) &&
                (mtxPtr->waitingQueue.size > 0UL))
            {
                RK_TCB const *wTcbPtr =
                    kTCBQPeek(&mtxPtr->waitingQueue);

                if ((wTcbPtr != NULL) &&
                    (wTcbPtr->priority < newPrio))
                {
                    newPrio = wTcbPtr->priority;
                }
            }

            node = node->nextPtr;
            RK_BARRIER
        }

        /*
          if this task's effective priority did not change, its
          contribution to the next owner in the chain also did not
          change -> propagation has reached a fixed point.
         */
        if (currTcbPtr->priority == newPrio)
        {
            break; /* break top while loop */
        }

        if (currTcbPtr->status == RK_READY)
        {
            RK_PRIO oldPrio = currTcbPtr->priority;

            RK_ERR err =
                kTCBQRem(&RK_gReadyQueue[oldPrio], &currTcbPtr);
            K_ASSERT(err == RK_ERR_SUCCESS);

            currTcbPtr->priority = newPrio;
            kTraceRecordTaskPrio(currTcbPtr, oldPrio, newPrio);

            err =
                kTCBQEnq(
                    &RK_gReadyQueue[currTcbPtr->priority],
                    currTcbPtr);
            K_ASSERT(err == RK_ERR_SUCCESS);

            kReschedTask(currTcbPtr);
        }
        else
        {
            RK_PRIO oldPrio = currTcbPtr->priority;
            currTcbPtr->priority = newPrio;
            kTraceRecordTaskPrio(currTcbPtr, oldPrio, newPrio);
        }

        RK_BARRIER

        if ((currTcbPtr->status != RK_BLOCKED) ||
            (currTcbPtr->waitingForMutexPtr == NULL))
        {
            break;
        }

        RK_MUTEX *waitMtxPtr =
            currTcbPtr->waitingForMutexPtr;

        RK_TCB *requeuePtr = currTcbPtr;

        /*
         reordering is necessary whenever the effective priority of
          a blocked task changes
         */
        if (waitMtxPtr->waitingQueue.size > 1UL)
        {
            RK_ERR err =
                kTCBQRem(&waitMtxPtr->waitingQueue, &requeuePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);

            err =
                kTCBQEnqByPrio(
                    &waitMtxPtr->waitingQueue,
                    requeuePtr);
            K_ASSERT(err == RK_ERR_SUCCESS);
        }

        /*
         if a mutex happens to not have prio inh enabled...
         too sad, too bad..
         */

        if (waitMtxPtr->prioInh == 0U)
        {
            break; /* end of story */
        }


        if (waitMtxPtr->ownerPtr == NULL)
        {
            break;
        }

        currTcbPtr = waitMtxPtr->ownerPtr;
    }

    RK_ISB
}

/* there is no recursive lock */
/* unlocking a mutex you do not own leads to hard fault */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT prioInh)
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
    if (prioInh > 1U)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = RK_TRUE;
    kobj->prioInh = prioInh;
    kobj->objID = RK_MUTEX_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->lock = RK_FALSE;
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
        /* lock mutex and set the owner */
        kobj->lock = RK_TRUE;
        kobj->ownerPtr = RK_gRunPtr;
        kMutexListAdd(&RK_gRunPtr->ownedMutexList, &kobj->mutexNode);
        kTraceRecordObject(kobj, RK_TRACE_OP_LOCK, RK_ERR_SUCCESS,
                           kobj->waitingQueue.size);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    /* mutex is locked, verify if owner is not the locker
    as no recursive lock is supported */

    if ((kobj->ownerPtr != RK_gRunPtr) && (kobj->ownerPtr != NULL))
    {
        if (timeout == 0)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, RK_ERR_MUTEX_LOCKED,
                               kobj->waitingQueue.size);
            RK_CR_EXIT
            return (RK_ERR_MUTEX_LOCKED);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {

            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, err,
                                   kobj->waitingQueue.size);
                RK_CR_EXIT
                return (err);
            }
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingQueue.size + 1UL);
        kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

        RK_gRunPtr->status = RK_BLOCKED;
        RK_gRunPtr->waitingForMutexPtr = kobj;
        /* apply priority inheritance */

        if (kobj->prioInh)
        {
            kMutexUpdateOwnerPrio_(kobj->ownerPtr);
        }

        kPendCtxSwtch();

            RK_CR_EXIT

        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            if (kobj->prioInh)
            {
                kMutexUpdateOwnerPrio_(kobj->ownerPtr);
            }

            RK_gRunPtr->timeOut = RK_FALSE;

            kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                               kobj->waitingQueue.size);
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

    else
    {
        if (kobj->ownerPtr == RK_gRunPtr)
        {
#if (RK_CONF_ERR_CHECK == ON)

            K_ERR_HANDLER(RK_FAULT_MUTEX_REC_LOCK);

#endif

            RK_CR_EXIT
            return (RK_ERR_MUTEX_REC_LOCK);
        }
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
    RK_TCB *tcbPtr;

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
        /* an ISR cannot own anything */
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

    /* RK_gRunPtr is the owner and mutex was locked */

    if (kobj->waitingQueue.size == 0)
    {

        kobj->lock = RK_FALSE;

        if (kobj->prioInh)
        { /* restore owner priority */

            kMutexUpdateOwnerPrio_(RK_gRunPtr);
            RK_BARRIER
        }
        kobj->ownerPtr = NULL;
        kTraceRecordObject(kobj, RK_TRACE_OP_UNLOCK, RK_ERR_SUCCESS, 0UL);
    }

    /* there are wTcbPtrs, unblock a wTcbPtr set new mutex owner */
    /* mutex is still locked */

    else
    {

        kTCBQDeq(&(kobj->waitingQueue), &tcbPtr);
        if (tcbPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&tcbPtr->timeoutNode);
            tcbPtr->timeoutNode.timeoutType = 0;
            tcbPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kobj->ownerPtr = tcbPtr;
        kMutexListAdd(&(tcbPtr->ownedMutexList), &(kobj->mutexNode));
        kobj->lock = RK_TRUE;
        tcbPtr->waitingForMutexPtr = NULL;
        if (kobj->prioInh)
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
