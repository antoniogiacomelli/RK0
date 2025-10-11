/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
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
/* return code is not a FAULT (negative) but a positive value (unsuccessful). */
/* Depending on the case it might or not mean an error in the synch logic.    */
/******************************************************************************/
/******************************************************************************/

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
    if (kobj->init == TRUE)
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

    kobj->init = TRUE;
    kobj->objID = RK_SEMAPHORE_KOBJ_ID;
    kobj->maxValue = maxValue;
    kobj->value = initValue;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* Timeout Node Setup */
#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP                 \
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
    runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif

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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (K_IS_BLOCK_ON_ISR(timeout))
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    if (kobj->value > 0)
    {
        kobj->value--;
    }
    else
    {

        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_SEMA_BLOCKED);
        }
        runPtr->status = RK_BLOCKED;
        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            kTimeOut(&runPtr->timeoutNode, timeout);
        }
        RK_PEND_CTXTSWTCH
        RK_CR_EXIT
        RK_CR_ENTER
        if (runPtr->timeOut)
        {
            runPtr->timeOut = FALSE;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            kRemoveTimeoutNode(&runPtr->timeoutNode);
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

    if (kobj->init == FALSE)
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
       ret = kReadySwtch(nextTCBPtr);
    }
    else
    {
        /* there are no waiting tasks */
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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

#endif

    UINT toWake = kobj->waitingQueue.size;
    if (toWake == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }
    RK_TCB* chosenTCBPtr = NULL;
    for (UINT i = 0; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyNoSwtch(nextTCBPtr);
        if (chosenTCBPtr == NULL && (nextTCBPtr->priority < runPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
        else
        {
            if (nextTCBPtr->priority < chosenTCBPtr->priority)
                chosenTCBPtr = nextTCBPtr;
        }
    }
    kSchedTask(chosenTCBPtr); 
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
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

    if (kobj->init == FALSE)
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
