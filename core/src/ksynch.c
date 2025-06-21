/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.2
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Module              : INTER-TASK SYNCHRONISATION
 *  Depends on          : TIMER, LOW-LEVEL SCHEDULER
 *  Provides to         : APPLICATION
 *  Public API  	    : YES
 *
 *****************************************************************************/

#define RK_CODE

#include "kservices.h"

/* Timeout Node Setup */

/* this is for blocking with timeout within an object queue (e.g., semaphore)*/
#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP                 \
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
    runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif

/* this is for blocking with timeout on a service with no associated object despite the task
itself (e.g., signals) */
#ifndef RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP               \
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_ELAPSING; \
    runPtr->timeoutNode.waitingQueuePtr = NULL;
#endif

/*****************************************************************************/
/* SIGNAL FLAGS                                                              */
/*****************************************************************************/
/* the procedure for blocking-timeout is commented in detail here, once,
as the remaining services follow it with little to no modification */
RK_ERR kSignalGet(ULONG const required, UINT const options,
                  ULONG *const gotFlagsPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    /* check for invalid parameters and return specific error */
    /* an ISR has no task control block */
    if (kIsISR())
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    /* check for invalid options, including required flags == 0 */
    if ((options != RK_FLAGS_ALL && options != RK_FLAGS_ANY) || required == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    runPtr->flagsReq = required;
    runPtr->flagsOpt = options;

    /* inspecting the flags upon returning is optional */
    if (gotFlagsPtr != NULL)
        *gotFlagsPtr = runPtr->flagsCurr;

    BOOL andLogic = (options == RK_FLAGS_ALL);
    BOOL conditionMet = 0;

    /* check if ANY or ALL flags establish a waiting condition */
    if (andLogic) /* ALL */
    {
        conditionMet = ((runPtr->flagsCurr & required) == (runPtr->flagsReq));
    }
    else
    {
        conditionMet = (runPtr->flagsCurr & required);
    }

    /* if condition is met, clear flags and return */
    if (conditionMet)
    {
        runPtr->flagsCurr &= ~runPtr->flagsReq;
        runPtr->flagsReq = 0UL;
        runPtr->flagsOpt = 0UL;
        RK_CR_EXIT
        return (RK_SUCCESS);
    }
    /* condition not met, and non-blocking call, return FLAGS_NOT_MET */
    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_FLAGS_NOT_MET);
    }

    /* start suspension */

    runPtr->status = RK_PENDING;

    /* if bounded timeout, enqueue task on timeout list with no
        associated waiting queue */
    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP

        kTimeOut(&runPtr->timeoutNode, timeout);
    }
    /* swtch ctxt */
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT

    /* suspension is resumed here */
    RK_CR_ENTER
    /* if resuming reason is timeout return ERR_TIMEOUT */
    if (runPtr->timeOut)
    {
        runPtr->timeOut = FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    /* resuming reason is a Set with condition met */

    /* if bounded waiting, remove task from timeout list */
    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        kRemoveTimeoutNode(&runPtr->timeoutNode);

    /* store current flags if asked */
    if (gotFlagsPtr != NULL)
        *gotFlagsPtr = runPtr->flagsCurr;

    /* clear flags on the TCB and return SUCCESS */
    runPtr->flagsCurr &= ~runPtr->flagsReq;
    runPtr->flagsReq = 0UL;
    runPtr->flagsOpt = 0UL;
    RK_CR_EXIT

    return (RK_SUCCESS);
}

RK_ERR kSignalSet(RK_TASK_HANDLE const taskHandle, ULONG const mask)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    /* check for invalid parameters and return specific error */
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (mask == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    /* OR mask to current flags */
    taskHandle->flagsCurr |= mask;
    if (taskHandle->status == RK_PENDING)
    {
        BOOL andLogic = 0;
        BOOL conditionMet = 0;

        andLogic = (taskHandle->flagsOpt == RK_FLAGS_ALL);

        if (andLogic)
        {
            conditionMet = ((taskHandle->flagsCurr & taskHandle->flagsReq) == (taskHandle->flagsReq));
        }
        else
        {
            conditionMet = (taskHandle->flagsCurr & taskHandle->flagsReq);
        }

        /* if condition is met and task is pending, ready task
        and return SUCCESS */
        if (conditionMet)
        {
            kReadyCtxtSwtch(&tcbs[taskHandle->pid]);
        }
    }
    /* if not, just return SUCCESS*/
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kSignalClear(VOID)
{
    /* a clear cannot be interrupted */
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    /* an ISR has no TCB */
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    (runPtr->flagsOpt = 0UL);
    _RK_DMB
    RK_CR_EXIT

    return (RK_SUCCESS);
}

RK_ERR kSignalQuery(RK_TASK_HANDLE const taskHandle, ULONG *const queryFlagsPtr)
{

    RK_CR_AREA
    RK_CR_ENTER
    RK_TASK_HANDLE handle = (taskHandle) ? (taskHandle) : (runPtr);
    if (queryFlagsPtr)
    {
        (*queryFlagsPtr = handle->flagsCurr);
        RK_CR_EXIT
        return (RK_SUCCESS);
    }
    RK_CR_EXIT
    return (RK_ERR_OBJ_NULL);
}

/******************************************************************************/
/* SLEEP/WAKE ON EVENTS                                                       */
/******************************************************************************/
#if (RK_CONF_EVENT == ON)
RK_ERR kEventInit(RK_EVENT *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = TRUE;
    kobj->objID = RK_EVENT_KOBJ_ID;

    RK_CR_EXIT

    return (RK_SUCCESS);
}
/*
 Sleep for a Signal/Wake Event
 Timeout in ticks.
 */
RK_ERR kEventSleep(RK_EVENT *const kobj, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_EVENT_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
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
        return (RK_ERR_INVALID_TIMEOUT);
    }

    kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);

    runPtr->status = RK_SLEEPING;

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

        kTimeOut(&runPtr->timeoutNode, timeout);
    }
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT
    /* resuming here, if time is out, return error */
    RK_CR_ENTER
    if (runPtr->timeOut)
    {
        runPtr->timeOut = FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        kRemoveTimeoutNode(&runPtr->timeoutNode);

    RK_CR_EXIT
    return (RK_SUCCESS);
}
/* Broadcast signal to an event
nTasks - number of tasks to unblock
uTasksPtr - pointer to store the effective
         number of unblocked tasks
*/
RK_ERR kEventWake(RK_EVENT *const kobj, UINT nTasks, UINT *uTasksPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_EVENT_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
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

    for (UINT i = 0; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyCtxtSwtch(nextTCBPtr);
    }
    if (uTasksPtr)
        *uTasksPtr = (UINT)kobj->waitingQueue.size;
    RK_CR_EXIT
    return RK_SUCCESS;
}

RK_ERR kEventSignal(RK_EVENT *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_EVENT_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->waitingQueue.size == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

#endif

    RK_TCB *nextTCBPtr = NULL;

    kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);

    kReadyCtxtSwtch(nextTCBPtr);
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kEventQuery(RK_EVENT const *const kobj, ULONG *const nTasksPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_EVENT_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (nTasksPtr != NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    *nTasksPtr = kobj->waitingQueue.size;
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#endif /* sleep-wake event */

#if (RK_CONF_SEMA == ON)
/******************************************************************************/
/* COUNTING/BIN SEMAPHORES                                                    */
/******************************************************************************/
/*  semaphores cannot initialise with a negative value */
RK_ERR kSemaInit(RK_SEMA *const kobj, UINT const semaType, const UINT value)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if ((semaType != RK_SEMA_COUNT) && (semaType != RK_SEMA_BIN))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if (kTCBQInit(&(kobj->waitingQueue)) != RK_SUCCESS)
    {
        RK_CR_EXIT
        return (RK_ERROR);
    }
#endif

    kobj->init = TRUE;
    kobj->objID = RK_SEMAPHORE_KOBJ_ID;
    kobj->semaType = semaType;
    kobj->value = (semaType == RK_SEMA_BIN && value > 1U) ? (1U) : value;
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kSemaPend(RK_SEMA *const kobj, const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_CHECK_PARMS == (ON))

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
        return (RK_ERR_OBJ_NULL);
    }

    if (K_IS_BLOCK_ON_ISR(timeout))
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
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
            return (RK_ERR_BLOCKED_SEMA);
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
    return (RK_SUCCESS);
}

RK_ERR kSemaPost(RK_SEMA *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

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
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->semaType == RK_SEMA_COUNT && kobj->value == RK_SEMA_MAX_VALUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OVERFLOW);
    }

    RK_TCB *nextTCBPtr = NULL;

    if (kobj->waitingQueue.size > 0)
    {
        kTCBQDeq(&(kobj->waitingQueue), &nextTCBPtr);
        kReadyCtxtSwtch(nextTCBPtr);
    }
    else
    {
        /* there are no waiting tasks, so the value inc */
        kobj->value = (kobj->semaType == RK_SEMA_BIN) ? (1) : (kobj->value + 1);
    }

    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kSemaWake(RK_SEMA *const kobj, UINT const nTasks, UINT *const uTasksPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

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
        return (RK_ERR_OBJ_NULL);
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

    for (UINT i = 0; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyCtxtSwtch(nextTCBPtr);
    }

    if (uTasksPtr)
        *uTasksPtr = (UINT)kobj->waitingQueue.size;
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kSemaQuery(RK_SEMA const *const kobj, INT *const countPtr)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

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
        return (RK_ERR_OBJ_NULL);
    }

    if (countPtr == NULL)
    {
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
    return (RK_SUCCESS);
}

#endif
#if (RK_CONF_MUTEX == ON)

/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/
void kMutexUpdateOwnerPriority(struct kTcb *ownerTcb)
{

    /* this implements the priority inheritance invariant: */
    /* task prio = max(prio of all tasks it is blocking)   */
    /* to generalise we start checking the nominal prio    */
    struct kTcb *currTcbPtr = ownerTcb;

    while (currTcbPtr != NULL)
    {
        RK_PRIO newPrio = currTcbPtr->prioReal;
        RK_NODE *node = currTcbPtr->ownedMutexList.listDummy.nextPtr;
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

        if (currTcbPtr->priority == newPrio)
        {
            break; /* no changes */
        }

        currTcbPtr->priority = newPrio;

        /* propagate.... */
        if (currTcbPtr->status == RK_BLOCKED &&
            currTcbPtr->waitingForMutexPtr != NULL &&
            currTcbPtr->waitingForMutexPtr->ownerPtr != NULL)
        {
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

#if (RK_CONF_CHECK_PARMS == (ON))

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERROR);
    }
    if (prioInh > 1U)
    {
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = TRUE;
    kobj->prioInh = prioInh;
    kobj->objID = RK_MUTEX_KOBJ_ID;
    kobj->lock = FALSE;
    return (RK_SUCCESS);
}

RK_ERR kMutexLock(RK_MUTEX *const kobj,
                  RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->lock == FALSE)
    {
        /* lock mutex and set the owner */
        kobj->lock = TRUE;
        kobj->ownerPtr = runPtr;
        kMQEnq(&runPtr->ownedMutexList, &kobj->mutexNode);
        RK_CR_EXIT
        return (RK_SUCCESS);
    }

    /* mutex is locked, verify if owner is not the locker
    as no recursive lock is supported */

    if ((kobj->ownerPtr != runPtr) && (kobj->ownerPtr != NULL))
    {
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_MUTEX_LOCKED);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);

        runPtr->status = RK_BLOCKED;
        runPtr->waitingForMutexPtr = kobj;
        /* apply priority inheritance */

        if (kobj->prioInh)
        {
            kMutexUpdateOwnerPriority(kobj->ownerPtr);
        }

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
            if (kobj->prioInh)
                kMutexUpdateOwnerPriority(kobj->ownerPtr);

            runPtr->timeOut = FALSE;

            RK_CR_EXIT

            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            kRemoveTimeoutNode(&runPtr->timeoutNode);
    }
    else
    {
        if (kobj->ownerPtr == runPtr)
        {
            K_ERR_HANDLER(RK_FAULT_MUTEX_REC_LOCK);
            RK_CR_EXIT
            return (RK_ERR_MUTEX_REC_LOCK);
        }
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kMutexUnlock(RK_MUTEX *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TCB *tcbPtr;

#if (RK_CONF_CHECK_PARMS == (ON))

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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kIsISR())
    {
        /* an ISR cannot own anything */
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    if ((kobj->lock == FALSE))
    {
        K_ERR_HANDLER(RK_FAULT_MUTEX_NOT_LOCKED);
        RK_CR_EXIT
        return (RK_ERR_MUTEX_NOT_LOCKED);
    }

    if (kobj->ownerPtr != runPtr)
    {
        K_ERR_HANDLER(RK_FAULT_UNLOCK_OWNED_MUTEX);
        RK_CR_EXIT
        return (RK_ERR_MUTEX_NOT_OWNER);
    }

    kMQRem(&(runPtr->ownedMutexList), &(kobj->mutexNode));

    /* runPtr is the owner and mutex was locked */

    if (kobj->waitingQueue.size == 0)
    {

        kobj->lock = FALSE;

        if (kobj->prioInh)
        { /* restore owner priority */

            kobj->ownerPtr->priority = kobj->ownerPtr->prioReal;
        }
        kobj->ownerPtr = NULL;
    }

    /* there are wTcbPtrs, unblock a wTcbPtr set new mutex owner */
    /* mutex is still locked */

    else
    {

        kTCBQDeq(&(kobj->waitingQueue), &tcbPtr);
        kobj->ownerPtr = tcbPtr;
        kMQEnq(&(tcbPtr->ownedMutexList), &(kobj->mutexNode));
        kobj->lock = TRUE;
        tcbPtr->waitingForMutexPtr = NULL;
        if (kobj->prioInh)
        {
            kMutexUpdateOwnerPriority(runPtr);
            kMutexUpdateOwnerPriority(tcbPtr);
        }
        kReadyCtxtSwtch(tcbPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

/* return mutex state - it checks for abnormal values */
RK_ERR kMutexQuery(RK_MUTEX const *const kobj, UINT *const statePtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_CHECK_PARMS == (ON))

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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (statePtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    *statePtr = ((UINT)kobj->lock);
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#endif /* mutex */
