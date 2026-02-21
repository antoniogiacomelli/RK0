/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.19                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SLEEP QUEUE                                                 */
/******************************************************************************/
 
#define RK_SOURCE_CODE

#include <ksleepq.h>
#include <ksystasks.h>

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
    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

    RK_gRunPtr->status = RK_SLEEPING;

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
    /* resuming here, if time is out, return error */
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
    if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
        nextTCBPtr->timeoutNode.timeoutType = 0;
        nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
    }

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
    if (taskHandle->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        kRemoveTimeoutNode(&taskHandle->timeoutNode);
        taskHandle->timeoutNode.timeoutType = 0;
        taskHandle->timeoutNode.waitingQueuePtr = NULL;
    }
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

    if (kIsISR())
    {
        if (uTasksPtr != NULL)
        {
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
            RK_CR_EXIT
            return (RK_ERR_INVALID_PARAM);
        }
        RK_CR_EXIT
        return (kPostProcJobEnq(RK_POSTPROC_JOB_SLEEPQ_WAKE, (VOID *)kobj, toWake));
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
    if (uTasksPtr)
    {
        *uTasksPtr = (UINT)kobj->waitingQueue.size;
    }
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }
    RK_CR_EXIT

    kSchUnlock();
    return (ret);
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

    RK_TCB **const taskPPtr = (RK_TCB **const)&handle;
    kTCBQRem(&RK_gReadyQueue[handle->priority], taskPPtr);    
    RK_TCB* taskPtr = *taskPPtr;
    RK_ERR err = kTCBQEnqByPrio(&kobj->waitingQueue, taskPtr);
    if (!err)
        taskPtr->status = RK_SLEEPING_SUSPENDED;
    RK_CR_EXIT
    return (err);
}

#if (RK_CONF_MUTEX == ON)
RK_ERR kCondVarWait(RK_SLEEP_QUEUE *const cv, RK_MUTEX *const mutex,
                    RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    kSchLock();
    RK_ERR err = kMutexUnlock(mutex);
    if (err == RK_ERR_SUCCESS)
    {
        err = kSleepQueueWait(cv, timeout);
    }
    kSchUnlock();

    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    return (kMutexLock(mutex, timeout));
}

RK_ERR kCondVarSignal(RK_SLEEP_QUEUE *const cv)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif
    return (kSleepQueueSignal(cv));
}

RK_ERR kCondVarBroadcast(RK_SLEEP_QUEUE *const cv)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif
    return (kSleepQueueWake(cv, 0U, NULL));
}
#endif


#endif /* sleep-wake event */
