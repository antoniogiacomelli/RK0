/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.8                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
/** COMPONENT        : SLEEP QUEUE                                            */
/** DEPENDS ON       : LOW-LEVEL SCHEDULER, TIMER                             */
/** PROVIDES TO      : APPLICATION                                            */
/** PUBLIC API       : YES                                                    */
/******************************************************************************/
/******************************************************************************/
 
#define RK_SOURCE_CODE

#include <ksleepq.h>

/* 
Sleep Queues are priority queues where tasks can wait for a condition 
They are stateless.
*/


#if (RK_CONF_SLEEP_QUEUE == ON)
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const kobj)
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

#endif

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = RK_TRUE;
    kobj->objID = RK_SLEEPQ_KOBJ_ID;

    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}



RK_ERR kSleepQueueWait(RK_SLEEP_QUEUE *const kobj, RK_TICK const timeout)
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

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
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

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

    RK_gRunPtr->status = RK_SLEEPING;

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

        kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
    }
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT
    /* resuming here, if time is out, return error */
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
 
RK_ERR kSleepQueueSignal(RK_SLEEP_QUEUE *const kobj)
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

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
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

    if (kobj->waitingQueue.size == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    RK_TCB *nextTCBPtr = NULL;

    kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);

    kReadySwtch(nextTCBPtr);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* cherry pick a task to wake*/
RK_ERR kSleepQueueReady(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE taskHandle)
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

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
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


    if (kobj->waitingQueue.size == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    RK_ERR err = kTCBQRem(&kobj->waitingQueue, &taskHandle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    kReadySwtch(taskHandle);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const *const kobj, ULONG *const nTasksPtr)
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

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
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

    if (nTasksPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    *nTasksPtr = kobj->waitingQueue.size;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}


RK_ERR kSleepQueueWake(RK_SLEEP_QUEUE *const kobj, UINT nTasks, UINT *uTasksPtr)
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

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
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

    UINT nWaiting = kobj->waitingQueue.size;

    if (nWaiting == 0)
    {
        if (uTasksPtr)
            *uTasksPtr = 0;
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    /* Wake up to nTasks, but no more than nWaiting */
    UINT toWake = 0;
    if (nTasks == 0)
    {
        /* if 0, wake'em all */
        toWake = nWaiting;
    }
    else
    {
        toWake = (nTasks < nWaiting) ? (nTasks) : (nWaiting);
    }
    RK_TCB* chosenTCBPtr = NULL;
    for (UINT i = 0; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyNoSwtch(nextTCBPtr);
        if (chosenTCBPtr == NULL && (nextTCBPtr->priority < RK_gRunPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
        else
        {
            if (nextTCBPtr->priority < chosenTCBPtr->priority)
                chosenTCBPtr = nextTCBPtr;
        }
    }
    if (uTasksPtr)
    {
        *uTasksPtr = (UINT)kobj->waitingQueue.size;
    }
    kSchedTask(chosenTCBPtr);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSleepQueueSuspend(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE handle)
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

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
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

    if (handle == NULL || handle == RK_gRunPtr || handle->status != RK_READY)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

   
    kTCBQRem(&RK_gReadyQueue[handle->priority], &handle);    
    kTCBQEnqByPrio(&kobj->waitingQueue, handle);
    handle->status = RK_SLEEPING_SUSPENDED;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}


#endif /* sleep-wake event */

