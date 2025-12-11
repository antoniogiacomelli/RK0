/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.0                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

/******************************************************************************/
/**                                                                           */
/**  COMPONENT        : MUTEX SEMAPHORE                                       */
/**  DEPENDS ON       : TIMER, LOW-LEVEL SCHEDULER                            */
/**  PROVIDES TO      : HIGH-LEVEL SCHEDULER                                  */
/**  PUBLIC API       : YES                                                   */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

/******************************************************************************/
/* Mutexes in RK0 remarkably implement fully transitive priority inheritance. */
/* They are not recursive.                                                    */
/******************************************************************************/

#define RK_SOURCE_CODE


#include <kmutex.h>
#include <klist.h>

#if (RK_CONF_MUTEX == ON)

/******************************************************************************/
/* MUTEX LIST                                                                 */
/******************************************************************************/
RK_FORCE_INLINE
static inline RK_ERR kMutexListAdd(struct RK_OBJ_LIST *ownedMutexList,
                            struct RK_OBJ_LIST_NODE *mutexNode)
{
    return kListAddTail(ownedMutexList, mutexNode);
}

RK_FORCE_INLINE
static inline RK_ERR kMutexListRem(struct RK_OBJ_LIST *ownedMutexList,
                            struct RK_OBJ_LIST_NODE *mutexNode)
{
    return kListRemove(ownedMutexList, mutexNode);
}

/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/

/* this implements the priority inheritance invariant: */
/* task prio = max(prio of all tasks it is blocking)   */    
RK_FORCE_INLINE
static inline 
void kMutexUpdateOwnerPrio_(struct RK_OBJ_TCB *ownerTcb)
{

    /* a task can own several mutexes, but it can only block at one     */
    struct RK_OBJ_TCB *currTcbPtr = ownerTcb;
    while (currTcbPtr != NULL)
    {
        /* to generalise we start with the nominal priority */
        /* (so it can be used for the trivial case ) */
        RK_PRIO newPrio = currTcbPtr->prioReal;

        /* point to the first mutex this task owns */
        RK_NODE *node = currTcbPtr->ownedMutexList.listDummy.nextPtr;
        
        /* find the highest priority task blocked by this man, if any */
        while (node != &currTcbPtr->ownedMutexList.listDummy)
        {
            RK_MUTEX *mtxPtr = K_GET_CONTAINER_ADDR(node, RK_MUTEX, mutexNode);
            if (mtxPtr->waitingQueue.size > 0)
            {
                RK_TCB const *wTcbPtr = kTCBQPeek(&mtxPtr->waitingQueue);
                if (wTcbPtr && wTcbPtr->priority < newPrio)
                    newPrio = wTcbPtr->priority;
            }
            node = node->nextPtr;
        }
        /* here, highest priority effective value has been found */
        if (currTcbPtr->priority == newPrio)
        {
            break; /* it is equal to the current effective, break */
        }
        /* otherwise, inherit it */

        currTcbPtr->priority = newPrio;

        /****  propagate the inherited priority ****/

        /* if TH is blocked by TM and TM is blocked by TL */
        /* TL inherits H's priority via TM                */
        if (currTcbPtr->status == RK_BLOCKED &&
            currTcbPtr->waitingForMutexPtr != NULL &&
            currTcbPtr->waitingForMutexPtr->ownerPtr != NULL)
        {
            /* now the owner looks up its blocking chain */
            currTcbPtr = currTcbPtr->waitingForMutexPtr->ownerPtr;
        }
        else
        {
            break; /* chain is over */
        }
    }
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
    kobj->lock = RK_FALSE;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}


/* Timeout Node Setup */
#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP                 \
    RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
    RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif


RK_ERR kMutexLock(RK_MUTEX *const kobj,
                  RK_TICK const timeout)
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
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->lock == RK_FALSE)
    {
        /* lock mutex and set the owner */
        kobj->lock = RK_TRUE;
        kobj->ownerPtr = RK_gRunPtr;
        kMutexListAdd(&RK_gRunPtr->ownedMutexList, &kobj->mutexNode);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    /* mutex is locked, verify if owner is not the locker
    as no recursive lock is supported */

    if ((kobj->ownerPtr != RK_gRunPtr) && (kobj->ownerPtr != NULL))
    {
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_MUTEX_LOCKED);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

        RK_gRunPtr->status = RK_BLOCKED;
        RK_gRunPtr->waitingForMutexPtr = kobj;
        /* apply priority inheritance */

        if (kobj->prioInh)
        {
            kMutexUpdateOwnerPrio_(kobj->ownerPtr);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {

            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            kTimeOut(&RK_gRunPtr->timeoutNode, timeout);
        }

        RK_PEND_CTXTSWTCH

        RK_CR_EXIT

        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            if (kobj->prioInh)
                kMutexUpdateOwnerPrio_(kobj->ownerPtr);

            RK_gRunPtr->timeOut = RK_FALSE;

            RK_CR_EXIT

            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
    }

#if (RK_CONF_ERR_CHECK == ON)
   
    else
    {
        if (kobj->ownerPtr == RK_gRunPtr)
        {
            K_ERR_HANDLER(RK_FAULT_MUTEX_REC_LOCK);
            RK_CR_EXIT
            return (RK_ERR_MUTEX_REC_LOCK);
        }
    }
#endif
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
        }
        kobj->ownerPtr = NULL;
    }

    /* there are wTcbPtrs, unblock a wTcbPtr set new mutex owner */
    /* mutex is still locked */

    else
    {

        kTCBQDeq(&(kobj->waitingQueue), &tcbPtr);
        kobj->ownerPtr = tcbPtr;
        kMutexListAdd(&(tcbPtr->ownedMutexList), &(kobj->mutexNode));
        kobj->lock = RK_TRUE;
        tcbPtr->waitingForMutexPtr = NULL;
        if (kobj->prioInh)
        {
            kMutexUpdateOwnerPrio_(RK_gRunPtr);
            kMutexUpdateOwnerPrio_(tcbPtr);
        }
        kReadySwtch(tcbPtr);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* return mutex state - it checks for abnormal values */
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
