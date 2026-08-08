/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MUTEX LOCK                                                      */
/******************************************************************************/

#define RK_SOURCE_CODE

#include "kmutex_private.h"

#if (RK_CONF_MUTEX == ON)

RK_ERR kMutexLockCore(RK_MUTEX *const kobj, RK_TICK const timeout)
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

#endif
