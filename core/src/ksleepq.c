/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.70.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SLEEP QUEUE                                                     */
/******************************************************************************/
/**
 * @note
 * A Sleep Queue is a stateless wait queue: it does not test a predicate and
 * does not remember previous signals. kSleepQueueSleep() suspends the running
 * task until another task wakes it, signals it, or its timeout expires.
 *
 * The condition variable helpers compose a Sleep Queue with a mutex. The
 * monitor code owns the predicate and calls kCondVarWait() in a Mesa-style
 * loop while holding the mutex.
 *
 * @code
 * while (!condition)
 * {
 *     kCondVarWait(&monitor->queue, &monitor->lock, RK_WAIT_FOREVER);
 * }
 *
 */


#define RK_SOURCE_CODE

#include <ksleepq.h>
#include <ksystasks.h>
#include <ktimer.h>
#include <ktrace.h>

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
    kobj->objName[0] = '\0';
    kTraceRegisterObject(kobj, RK_SLEEPQ_KOBJ_ID);

    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSleepQueueSleep(RK_SLEEP_QUEUE *const kobj, RK_TICK const timeout)
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
        kTraceRecordObject(kobj, RK_TRACE_OP_WAIT, RK_ERR_NOWAIT,
                           kobj->waitingQueue.size);
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            RK_gRunPtr->timeoutNode.timeoutType = 0;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_CR_EXIT
            return (err);
        }
    }
    RK_gRunPtr->status = RK_SLEEPING;
    kTraceRecordObject(kobj, RK_TRACE_OP_WAIT_BLOCK, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size + 1UL);
    kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

    kPendCtxSwtch();
    RK_CR_EXIT
    /* resuming here, if time is out, return error */
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
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

    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size);
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
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE,
                           RK_ERR_EMPTY_WAITING_QUEUE, 0UL);
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
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size);
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
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE,
                           RK_ERR_EMPTY_WAITING_QUEUE, 0UL);
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
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size);

    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const *const kobj,
                        ULONG *const nTasksPtr)
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
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE,
                           RK_ERR_EMPTY_WAITING_QUEUE, 0UL);
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
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                           (ULONG)toWake);
        RK_CR_EXIT
        return (
            kPostProcJobEnq(RK_POSTPROC_JOB_SLEEPQ_WAKE, (VOID *)kobj, toWake));
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
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
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
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, ret,
                       (ULONG)(nWaiting - kobj->waitingQueue.size));
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }
    RK_CR_EXIT

    kSchUnlock();
    return (ret);
}

RK_ERR kSleepQueueUnready(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE handle)
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

    RK_TCB **const taskPPtr = (RK_TCB * *const)&handle;
    kTCBQRem(&RK_gReadyQueue[handle->priority], taskPPtr);
    RK_TCB *taskPtr = *taskPPtr;
    RK_ERR err = kTCBQEnqByPrio(&kobj->waitingQueue, taskPtr);
    if (!err)
    {
        taskPtr->status = RK_SLEEPQ_BLOCKED;
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, err, kobj->waitingQueue.size);
    RK_CR_EXIT
    return (err);
}
#if (RK_CONF_CONDVAR == ON)

RK_ERR kCondVarInit(RK_SLEEP_QUEUE *const cond, RK_MUTEX *const lock)
{
    RK_ERR err = kSleepQueueInit(cond);
    if (err != RK_ERR_SUCCESS)
        return (err);
    err = kMutexInit(lock, RK_PRIO_INHERITANCE);
    return (err);
}

RK_ERR kCondVarWait(RK_SLEEP_QUEUE *const cond,
                    RK_MUTEX *const lock,
                    RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    /*
     * RK_NO_WAIT cannot express a condition variable wait.
     * Finite timeouts must remain within the wrap-safe half-range.
     */
    if ((timeout == RK_NO_WAIT) ||
        ((timeout != RK_WAIT_FOREVER) &&
         (timeout > RK_MAX_PERIOD)))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        return (RK_ERR_INVALID_TIMEOUT);
    }

    RK_TICK deadline = 0UL;

    if (timeout != RK_WAIT_FOREVER)
    {
        deadline = K_TICK_ADD(kTickGet(), timeout);
    }

    /*
     * prevent dispatch between releasing the Mutex and entering the
     * cond queue.
     */
    kPreemptDisable();

    RK_ERR const unlockErr = kMutexUnlock(lock);

    if (unlockErr != RK_ERR_SUCCESS)
    {
        kPreemptEnable();
        return (unlockErr);
    }

    RK_ERR waitErr;

    if (timeout == RK_WAIT_FOREVER)
    {
        waitErr = kSleepQueueSleep(cond, RK_WAIT_FOREVER);
    }
    else
    {
        RK_STICK const remaining =
            K_TICK_DIFF(deadline, kTickGet());

        if (remaining > 0)
        {
            waitErr = kSleepQueueSleep(cond, (RK_TICK)remaining);
        }
        else
        {
            /*
             * not call kSleepQueueSleep() with RK_NO_WAIT.
             * The Mutex must still be reacquired below.
             */
            waitErr = RK_ERR_TIMEOUT;
        }
    }

    /*
     * reacquire while the scheduler lock is still owned. If the Mutex
     * is held, this call blocks normally; other tasks execute with their
     * own scheduler-lock state.
     */
    RK_ERR const lockErr =
        kMutexLock(lock, RK_WAIT_FOREVER);

    kPreemptEnable();

    if (waitErr == RK_ERR_TIMEOUT)
    {
        K_PANIC("Condition variable timed out");
    }

    return ((lockErr != RK_ERR_SUCCESS) ? lockErr : waitErr);
}

RK_ERR kCondVarSignal(RK_SLEEP_QUEUE *const cond)
{
    return (kSleepQueueSignal(cond));
}

RK_ERR kCondVarBroadcast(RK_SLEEP_QUEUE *const cond)
{
    return (kSleepQueueWake(cond, 0U, NULL));
}
#endif

#endif /* RK_CONF_SLEEP_QUEUE */
