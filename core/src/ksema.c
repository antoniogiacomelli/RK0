/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.14                                              */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
#define RK_SOURCE_CODE

#include <ksema.h>

#if (RK_CONF_SEMAPHORE == ON)
/******************************************************************************/
/* COUNTING/BIN SEMAPHORES                                                    */
/******************************************************************************/
/******************************************************************************/
/* A semaphore has a maxValue. To create a binary semaphore set its max value */
/* to 1U. Note, when signalling a semaphore whose value reached its limit,    */
/* return code is not a FAULT (negative) but a positive value                 */
/* (RK_ERR_SEMA_FULL).                                                        */
/* Depending on the case it might or not mean an error in the synch logic.    */
/******************************************************************************/

#ifndef K_SEMA_IS_BINARY
#define K_SEMA_IS_BINARY(kobj) ((kobj)->maxValue == 1U)
#endif

RK_ERR kSemaphoreInit(RK_SEMAPHORE *const kobj, const UINT initValue, const UINT maxValue)
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
    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    if ((maxValue == 0U) || (initValue > maxValue))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if (kTCBQInit(&(kobj->waitingQueue)) != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (RK_ERR_ERROR);
    }
#endif

    kobj->init = RK_TRUE;
    kobj->objID = RK_SEMAPHORE_KOBJ_ID;
    kobj->maxValue = maxValue;
    kobj->value = initValue;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSemaphorePend(RK_SEMAPHORE *const kobj, const RK_TICK timeout)
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

    if (kobj->objID != RK_SEMAPHORE_KOBJ_ID)
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

    if (K_BLOCKING_ON_ISR(timeout))
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    if (kobj->value > 0U)
    {
        if (K_SEMA_IS_BINARY(kobj))
        {
            kobj->value = 0U;
        }
        else
        {
            kobj->value = kobj->value - 1U;
        }
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }
    else
    {

        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_SEMA_BLOCKED);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
        {
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
            RK_CR_EXIT
            return (RK_ERR_INVALID_TIMEOUT);
        }
        RK_gRunPtr->status = RK_BLOCKED;
        kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_TCB *selfPtr = RK_gRunPtr;
                kTCBQRem(&kobj->waitingQueue, &selfPtr);
                RK_gRunPtr->status = RK_RUNNING;
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
#if (RK_CONF_ERR_CHECK == ON)
                if (err == RK_ERR_INVALID_PARAM)
                {
                    K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
                }
#endif
                if (err == RK_ERR_INVALID_PARAM)
                {
                    err = RK_ERR_INVALID_TIMEOUT;
                }
                RK_CR_EXIT
                return (err);
            }
        }
        RK_PEND_CTXTSWTCH
        RK_CR_EXIT
        RK_CR_ENTER
        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
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
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSemaphorePost(RK_SEMAPHORE *const kobj)
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

    if (kobj->objID != RK_SEMAPHORE_KOBJ_ID)
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

    RK_TCB *nextTCBPtr = NULL;
    RK_ERR ret = -1;
    if (kobj->waitingQueue.size > 0)
    {
       kTCBQDeq(&(kobj->waitingQueue), &nextTCBPtr);
       if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
       {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
       }
       ret = kReadySwtch(nextTCBPtr);
    }
    else
    {
        /* there are no waiting tasks */
        if (K_SEMA_IS_BINARY(kobj))
        {
            if (kobj->value != 0U)
            {
                ret = RK_ERR_SEMA_FULL;
            }
            else
            {
                kobj->value = 1U;
                ret = RK_ERR_SUCCESS;
            }
        }
        else
        {
            if (kobj->value == kobj->maxValue)
            {
                ret = RK_ERR_SEMA_FULL;
            }
            else
            {
                kobj->value += 1U;
                ret = RK_ERR_SUCCESS;
            }
        }
    }
    RK_CR_EXIT   
    return (ret);
}

RK_ERR kSemaphoreFlush(RK_SEMAPHORE *const kobj)
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

    if (kobj->objID != RK_SEMAPHORE_KOBJ_ID)
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

    UINT toWake = kobj->waitingQueue.size;
    if (toWake == 0U)
    {
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    RK_CR_EXIT

    kSchLock();

    RK_TCB *chosenTCBPtr = NULL;
    RK_ERR ret = RK_ERR_SUCCESS;

    for (UINT i = 0U; i < toWake; i++)
    {
        RK_CR_ENTER
        if (kobj->waitingQueue.size == 0U)
        {
            RK_CR_EXIT
            break;
        }
        RK_TCB *nextTCBPtr = NULL;
        ret = kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (ret != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            break;
        }
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        ret = kReadyNoSwtch(nextTCBPtr);
        if (ret != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            break;
        }
        if ((chosenTCBPtr == NULL) || (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
        RK_CR_EXIT
    }

    RK_CR_ENTER
    kobj->value = 0U;
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }
    RK_CR_EXIT

    kSchUnlock();

    return (ret);
}

RK_ERR kSemaphoreQuery(RK_SEMAPHORE const *const kobj, INT *const countPtr)
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

    if (kobj->objID != RK_SEMAPHORE_KOBJ_ID)
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

    if (countPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->waitingQueue.size > 0)
    {
        INT retVal = (-((INT)kobj->waitingQueue.size));
        *countPtr = retVal;
    }
    else
    {
        *countPtr = (INT)kobj->value;
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif
