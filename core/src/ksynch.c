/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP \
     runPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
     runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif

/* this is for blocking with timeout on a service with no associated object despite the task
itself (e.g., signals) */
#ifndef RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP \
     runPtr->timeoutNode.timeoutType = RK_TIMEOUT_ELAPSING; \
     runPtr->timeoutNode.waitingQueuePtr = NULL;
#endif


 /*****************************************************************************/
/* SIGNAL FLAGS                                                              */
/*****************************************************************************/
/* the procedure for blocking-timeout is commented in detail here, once, 
as the remaining services follow it with little to no modification */
RK_ERR kSignalGet( ULONG const required, UINT const options,  ULONG *const gotFlagsPtr,
	 RK_TICK const timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
    /* check for invalid parameters and return specific error */
    /* an ISR has no task control block */
	if (kIsISR())
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
    /* check for invalid options, including required flags == 0 */
	if ((options != RK_FLAGS_ALL && options != RK_FLAGS_ANY) || required == 0UL)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_PARAM);
	}
	_RK_DMB
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
		conditionMet = ((runPtr->flagsCurr & required)
				== (runPtr->flagsReq));
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
	if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
	{
		RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP

		kTimeOut( &runPtr->timeoutNode, timeout);
	}
    /* yield to return when unblocked */
	RK_PEND_CTXTSWTCH
	RK_CR_EXIT

    /* suspension is resumed here */
	RK_CR_ENTER
	_RK_DMB
    /* if resuming reason is timeout return ERR_TIMEOUT */
    if (runPtr->timeOut)
	{
		runPtr->timeOut = FALSE;
		RK_CR_EXIT
		return (RK_ERR_TIMEOUT);
	}

    /* resuming reason is a Set with condition met */

    /* if bounded waiting, remove task from timeout list */
	if (timeout > RK_NO_WAIT && timeout != RK_WAIT_FOREVER)
		kRemoveTimeoutNode( &runPtr->timeoutNode);

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

RK_ERR kSignalSet( RK_TASK_HANDLE const taskHandle, ULONG const mask)
{
	RK_CR_AREA
	RK_CR_ENTER
    /* check for invalid parameters and return specific error */
	if (taskHandle == NULL)
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (mask == 0UL)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_PARAM);
	}
    /* OR mask to current flags */
	taskHandle->flagsCurr |= mask;	
	_RK_DMB
	if (taskHandle->status == RK_PENDING)
	{
		BOOL andLogic = 0;
		BOOL conditionMet = 0;

		andLogic = (taskHandle->flagsOpt == RK_FLAGS_ALL);

		if (andLogic)
		{
			conditionMet = ((taskHandle->flagsCurr
					& taskHandle->flagsReq)
					== (taskHandle->flagsReq));
		}
		else
		{
			conditionMet = (taskHandle->flagsCurr
					& taskHandle->flagsReq);
		}

		/* if condition is met and task is pending, ready task 
		and return SUCCESS */
		if (conditionMet)
		{
				kReadyCtxtSwtch( &tcbs[taskHandle->pid]);
				RK_CR_EXIT
				return (RK_SUCCESS);
		}
	}
    /* if not, just return SUCCESS*/
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kSignalClear( VOID)
{
    /* a clear cannot be interrupted */
    RK_CR_AREA
    RK_CR_ENTER
 
    /* an ISR has no TCB */
	if (kIsISR())
	{
        RK_CR_EXIT
 		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}

    /* clear and return SUCCESS*/  
 	(runPtr->flagsCurr = 0UL);
	(runPtr->flagsReq = 0UL);
    (runPtr->flagsOpt = 0UL);
    
	RK_CR_EXIT

    return (RK_SUCCESS);
}

RK_ERR kSignalQuery(RK_TASK_HANDLE const taskHandle, ULONG *const queryFlagsPtr)
{
	if (kIsISR())
	{
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
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
#if (RK_CONF_EVENT==ON)
RK_ERR kEventInit( RK_EVENT *const kobj)
{
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	RK_CR_AREA
	RK_CR_ENTER
	kassert( !kTCBQInit( &(kobj->waitingQueue), "eventQ"));
	kobj->init = TRUE;
	kobj->objID = RK_EVENT_KOBJ_ID;
	RK_CR_EXIT
	return (RK_SUCCESS);
}
/*
 Sleep for a Signal/Wake Event
 Timeout in ticks.
 */
RK_ERR kEventSleep( RK_EVENT *const kobj, RK_TICK const timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	RK_ERR err = RK_ERROR;
	if (kIsISR())
	{
		K_ERR_HANDLER( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (timeout == RK_NO_WAIT)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_TIMEOUT);
	}
	err = kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
	if (err < 0)
	{
		RK_CR_EXIT
		return (err);
	}
	runPtr->status = RK_SLEEPING;

	if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
	{
		RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

		kTimeOut( &runPtr->timeoutNode, timeout);
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

	if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		kRemoveTimeoutNode( &runPtr->timeoutNode);

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

    if (kobj == NULL) 
	{
        RK_CR_EXIT  
        return (RK_ERR_OBJ_NULL);
    }
    if (!kobj->init) 
	{
        RK_CR_EXIT 
        return (RK_ERR_OBJ_NOT_INIT);
    }
    
	UINT nWaiting = kobj->waitingQueue.size;
    if (nWaiting == 0) 
	{
		if (uTasksPtr)
			*uTasksPtr = 0;
        RK_CR_EXIT 
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    /* Wake up to nTasks, but no more than nWaiting */
    UINT toWake = (nTasks < nWaiting) ? (nTasks) : (nWaiting);
    for (UINT i = 0; i < toWake; i++) 
	{
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyCtxtSwtch(nextTCBPtr);
    }
	if (uTasksPtr)
    	*uTasksPtr = toWake;
	_RK_DMB
    RK_CR_EXIT 
    return RK_SUCCESS;
}

RK_ERR kEventSignal( RK_EVENT *const kobj)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->waitingQueue.size == 0)
	{
		RK_CR_EXIT
		return (RK_ERR_EMPTY_WAITING_QUEUE);
	}
	if (kobj->init == FALSE)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	RK_TCB *nextTCBPtr = NULL;
	kTCBQDeq( &kobj->waitingQueue, &nextTCBPtr);
	kReadyCtxtSwtch( nextTCBPtr);
	_RK_DMB
	RK_CR_EXIT
	return (RK_SUCCESS);
}
/* Returns the number of tasks sleeping for an event */
ULONG kEventQuery( RK_EVENT *const kobj)
{
	if (kobj == NULL)
	{
		return (0);
	}
	return (kobj->waitingQueue.size);
}

#endif /* sleep-wake event */


#if (RK_CONF_SEMA == ON)
/******************************************************************************/
/* COUNTER/BIN SEMAPHORES                                                         */
/******************************************************************************/
/*  semaphores cannot initialise with a negative value */
RK_ERR kSemaInit( RK_SEMA *const kobj, UINT const semaType, const INT value)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (value < 0)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_PARAM);
	}
	if ((semaType != RK_SEMA_COUNTER) && (semaType != RK_SEMA_BIN))
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_PARAM);
	}
	if (kTCBQInit( &(kobj->waitingQueue), "semaQ") != RK_SUCCESS)
	{
		RK_CR_EXIT
		return (RK_ERROR);
	}
	if (semaType == RK_SEMA_BIN)
	{
		if (value > 1)
		{
			RK_CR_EXIT
			return (RK_ERR_INVALID_PARAM);
		}
	}
	kobj->init = TRUE;
	kobj->objID = RK_SEMAPHORE_KOBJ_ID;
	kobj->semaType = semaType;
	kobj->value = value;
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kSemaPend( RK_SEMA *const kobj, const RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kIsISR())
	{
		K_ERR_HANDLER( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}

	if (kobj->init == FALSE)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}

	
	if (kobj->value > 0)
	{
		kobj->value --;
		_RK_DMB
	}
	else if (kobj->value == 0)
	{
	
		if (timeout == RK_NO_WAIT)
		{
			/* if it is the first pend when 0, 
			increase */
			if (kobj->waitingQueue.size == 0)
			{
				kobj->value ++;
				_RK_DMB
			}
			/* otherwise, there are tasks waiting, so
			the counter is still 0 */
			RK_CR_EXIT
			return (RK_ERR_BLOCKED_SEMA);

		}
		runPtr->status = RK_BLOCKED;
		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
		if (timeout > RK_NO_WAIT && timeout != RK_WAIT_FOREVER)
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);

		}
		RK_PEND_CTXTSWTCH
		RK_CR_EXIT
		RK_CR_ENTER
		if (runPtr->timeOut)
		{
			runPtr->timeOut = FALSE;

			/* if it is the first pend when 0, 
			increase */
			if (kobj->waitingQueue.size == 0)
			{
				kobj->value ++;
				_RK_DMB
			}
			/* otherwise, there are tasks waiting, so
			the counter is still 0 */
			RK_CR_EXIT
			return (RK_ERR_TIMEOUT);
		}

		if (timeout > RK_NO_WAIT && timeout != RK_WAIT_FOREVER)
			kRemoveTimeoutNode( &runPtr->timeoutNode);
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kSemaPost( RK_SEMA *const kobj)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	RK_TCB *nextTCBPtr = NULL;

	if (kobj->value == INT32_MAX - 1)
	{
		RK_CR_EXIT
		return (RK_ERR_OVERFLOW);
	}
	if (kobj->waitingQueue.size > 0)
	{		
		RK_ERR err = kTCBQDeq( &(kobj->waitingQueue), &nextTCBPtr);
		if (err < 0)
		{
			RK_CR_EXIT
			return (err);
		}
		err = kReadyCtxtSwtch( nextTCBPtr);
		if (err < 0)
		{
			RK_CR_EXIT
			return (err);
		}
	}
	else
	{
		/* there are no waiting tasks, so the value inc */
 		kobj->value = (kobj->semaType == RK_SEMA_BIN) ? (1) : (kobj->value + 1);
	}
	_RK_DMB
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kSemaWake( RK_SEMA *const kobj, UINT nTasks, UINT *uTasksPtr)
{
	if (kobj == NULL)
		return (RK_ERR_OBJ_NULL);
	if (!kobj->init)
		return (RK_ERR_OBJ_NOT_INIT);
	if (kobj->value > 0)
		return (RK_ERROR);

	RK_CR_AREA	
	RK_CR_ENTER
	
	UINT nWaiting = kobj->waitingQueue.size;
    
	if (nWaiting == 0) 
	{
        if (uTasksPtr)
			*uTasksPtr = 0;
		RK_CR_EXIT
		return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

  /* Wake up to nTasks, but no more than nWaiting */
    UINT toWake = (nTasks < nWaiting) ? (nTasks) : (nWaiting);
    for (UINT i = 0; i < toWake; i++) 
	{
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyCtxtSwtch(nextTCBPtr);
    }
	if (uTasksPtr)
    	*uTasksPtr = toWake;
	_RK_DMB
	RK_CR_EXIT
	return (RK_SUCCESS);	
}

INT kSemaQuery(RK_SEMA const *kobj)
{
	if (kobj == NULL)
		return (RK_ERR_OBJ_NULL);
	if (!kobj->init)
		return (RK_ERR_OBJ_NOT_INIT);
	RK_CR_AREA
	RK_CR_ENTER
	return (kobj->waitingQueue.size>0) ? ((INT)(-kobj->waitingQueue.size)) : (kobj->value);
	RK_CR_EXIT
}

#endif
#if (RK_CONF_MUTEX == ON)
/*******************************************************************************
 * MUTEX SEMAPHORE
 *******************************************************************************/
/* there is no recursive lock */
/* unlocking a mutex you do not own leads to hard fault */

RK_ERR kMutexInit( RK_MUTEX *const kobj)
{

	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		return (RK_ERROR);
	}
	kobj->lock = FALSE;
	if (kTCBQInit( &(kobj->waitingQueue), "mutexQ") != RK_SUCCESS)
	{
		return (RK_ERROR);
	}
	kobj->init = TRUE;
	kobj->objID = RK_MUTEX_KOBJ_ID;
	return (RK_SUCCESS);
}

RK_ERR kMutexLock( RK_MUTEX *const kobj, BOOL const prioInh, RK_TICK const timeout)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL)
	{
		RK_CR_EXIT
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	if (kIsISR())
	{
		K_ERR_HANDLER( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->lock == FALSE)
	{
		/* lock mutex and set the owner */
		kobj->lock = TRUE;
		kobj->ownerPtr = runPtr;
		_RK_DMB
		RK_CR_EXIT
		return (RK_SUCCESS);
	}
	if ((kobj->ownerPtr != runPtr) && (kobj->ownerPtr != NULL))
	{
		if (timeout == 0)
		{
			RK_CR_EXIT
			return (RK_ERR_MUTEX_LOCKED);
		}
		if (prioInh)
		{
			if (kobj->ownerPtr->priority > runPtr->priority)
			{
				kobj->ownerPtr->priority = runPtr->priority;
			}
		}
		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
		runPtr->status = RK_BLOCKED;

		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{

			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);
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
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
			kRemoveTimeoutNode( &runPtr->timeoutNode);
	}
	else
	{
		if (kobj->ownerPtr == runPtr)
		{/* recursive lock ? why ? WHYYYYYYYYYYY*/
			RK_CR_EXIT
			return (RK_ERR_MUTEX_REC_LOCK);
		}	
	}
 	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kMutexUnlock( RK_MUTEX *const kobj)
{
	RK_CR_AREA
	RK_CR_ENTER
	RK_TCB *tcbPtr;
	if (kIsISR())
	{
		/* an ISR cannot own anything */
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	if (kobj == NULL)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		K_ERR_HANDLER( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}

	if ((kobj->lock == FALSE ))
	{
		RK_CR_EXIT
		return (RK_ERR_MUTEX_NOT_LOCKED);
	}
	if (kobj->ownerPtr != runPtr)
	{
		K_ERR_HANDLER( RK_FAULT_UNLOCK_OWNED_MUTEX);
		RK_CR_EXIT
		return (RK_ERR_MUTEX_NOT_OWNER);
	}
	/* runPtr is the owner and mutex was locked */
	if (kobj->waitingQueue.size == 0)
	{
		_RK_DMB
		kobj->lock = FALSE;
		/* restore owner priority */
		kobj->ownerPtr->priority = kobj->ownerPtr->prioReal;
		kobj->ownerPtr = NULL;
		
	}
	else
	{
		/* there are waiters, unblock a waiter set new mutex owner.
		 * mutex is still locked */
 	
	    kTCBQDeq( &(kobj->waitingQueue), &tcbPtr);
		kassert(tcbPtr != NULL);
		/* here only runptr can unlock a mutex*/
		if (runPtr->priority < runPtr->prioReal)
		{
			runPtr->priority = runPtr->prioReal;
		}
		if (!kReadyCtxtSwtch( tcbPtr))
		{
			kobj->ownerPtr = tcbPtr;
		}
		else
		{
			K_ERR_HANDLER( RK_FAULT_READY_QUEUE);
			RK_CR_EXIT
			return (RK_ERR_READY_QUEUE);
		}
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

/* return mutex state - it checks for abnormal values */
LONG kMutexQuery( RK_MUTEX *const kobj)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		RK_CR_EXIT
		return (-1L);
	}
	if (!kobj->init)
	{
		RK_CR_EXIT
		return (-1L);

	}
	RK_CR_EXIT	
	return ((LONG)(kobj->lock));
}

#endif /* mutex */