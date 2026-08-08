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

RK_ERR kMutexUnlockCore(RK_MUTEX *const kobj)
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

#endif
