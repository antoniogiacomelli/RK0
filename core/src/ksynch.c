/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version              :   V0.4.0
 * Architecture         :   ARMv7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 *
 ******************************************************************************/
/******************************************************************************
 *
 *  Module              : INTER-TASK SYNCHRONISATION
 *  Depends on          : SCHEDULER, TIMER
 *  Provides to         : APPLICATION
 *  Public API  	    : YES
 *
 *****************************************************************************/

#define RK_CODE

#include "kmodules.h"

/* Timeout Node Setup */

#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP \
     runPtr->timeoutNode.timeoutType = RK_BLOCKING_TIMEOUT; \
     runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif
#ifndef RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP \
     runPtr->timeoutNode.timeoutType = RK_ELAPSING_TIMEOUT; \
     runPtr->timeoutNode.waitingQueuePtr = NULL;
#endif

/*****************************************************************************/
/* SIGNAL FLAGS                                                              */
/*****************************************************************************/
RK_ERR kSignalGet( ULONG const required, UINT const options,  ULONG *const gotFlagsPtr,
	 RK_TICK const timeout)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kIsISR())
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
    
	if ((options != RK_FLAGS_ALL && options != RK_FLAGS_ANY) || required == 0UL)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_PARAM);
	}

	runPtr->requiredTaskFlags = required;
	runPtr->taskFlagsOptions = options;

	if (gotFlagsPtr != NULL)
		*gotFlagsPtr = runPtr->currentTaskFlags;

	BOOL andLogic = (options == RK_FLAGS_ALL);
	BOOL conditionMet = 0;

	if (andLogic)
	{
		conditionMet = ((runPtr->currentTaskFlags & required)
				== (runPtr->requiredTaskFlags));
	}
	else
	{
		conditionMet = (runPtr->currentTaskFlags & required);
	}

	if (conditionMet)
	{
		runPtr->currentTaskFlags &= ~runPtr->requiredTaskFlags;
		RK_CR_EXIT
		return (RK_SUCCESS);
	}
	if (timeout == RK_NO_WAIT)
	{
		RK_CR_EXIT
		return (RK_ERR_FLAGS_NOT_MET);
	}

	runPtr->status = RK_PENDING_TASK_FLAGS;

	if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
	{
		RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP

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
	if (timeout > RK_NO_WAIT && timeout != RK_WAIT_FOREVER)
		kRemoveTimeoutNode( &runPtr->timeoutNode);

	if (gotFlagsPtr != NULL)
        *gotFlagsPtr = runPtr->currentTaskFlags;

	runPtr->currentTaskFlags &= ~runPtr->requiredTaskFlags;

	RK_CR_EXIT

	return (RK_SUCCESS);

}

RK_ERR kSignalSet( RK_TASK_HANDLE const taskHandle, ULONG const mask)
{
	RK_CR_AREA
	RK_CR_ENTER

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

	taskHandle->currentTaskFlags |= mask;

	BOOL andLogic = 0;
	BOOL conditionMet = 0;

	andLogic = (taskHandle->taskFlagsOptions == RK_FLAGS_ALL);

	if (andLogic)
	{
		conditionMet = ((taskHandle->currentTaskFlags
				& taskHandle->requiredTaskFlags)
				== (taskHandle->requiredTaskFlags));
	}
	else
	{
		conditionMet = (taskHandle->currentTaskFlags
				& taskHandle->requiredTaskFlags);
	}

	if (conditionMet)
	{
		if (taskHandle->status == RK_PENDING_TASK_FLAGS)
		{
			kReadyCtxtSwtch( &tcbs[taskHandle->pid]);
			RK_CR_EXIT
			return (RK_SUCCESS);
		}
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kSignalClear( VOID)
{
	if (kIsISR())
	{
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	(runPtr->currentTaskFlags = 0UL);
	return (RK_SUCCESS);
}

RK_ERR kSignalQuery( ULONG *const queryFlagsPtr)
{
	if (kIsISR())
	{
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	RK_CR_AREA
	RK_CR_ENTER
	if (queryFlagsPtr)
	{
		(*queryFlagsPtr = runPtr->currentTaskFlags);
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
		KERR( RK_FAULT_OBJ_NULL);
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
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
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
/* Broadcast signal to an event - all tasks will switch to READY */
RK_ERR kEventWake( RK_EVENT *const kobj)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	if (kobj->waitingQueue.size == 0)
	{
		RK_CR_EXIT
		return (RK_ERR_EMPTY_WAITING_QUEUE);
	}
	while (kobj->waitingQueue.size)
	{
		RK_TCB *nextTCBPtr = NULL;
		kTCBQDeq( &kobj->waitingQueue, &nextTCBPtr);
		kReadyCtxtSwtch( nextTCBPtr);
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kEventSignal( RK_EVENT *const kobj)
{
	RK_CR_AREA
	RK_CR_ENTER
	RK_ERR err = RK_ERROR;
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->waitingQueue.size == 0)
		err = (RK_ERR_EMPTY_WAITING_QUEUE);
	RK_CR_EXIT
	return (err);
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		err = (RK_ERR_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (err);
	}
	RK_TCB *nextTCBPtr = NULL;
	kTCBQDeq( &kobj->waitingQueue, &nextTCBPtr);
	kReadyCtxtSwtch( nextTCBPtr);
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
/* COUNTER SEMAPHORES                                                         */
/******************************************************************************/
/* counter semaphores cannot initialise with a negative value */
RK_ERR kSemaInit( RK_SEMA *const kobj, const INT value)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (value < 0)
		KERR( RK_GENERIC_FAULT);
	kobj->value = value;
	if (kTCBQInit( &(kobj->waitingQueue), "semaQ") != RK_SUCCESS)
	{
		RK_CR_EXIT
		return (RK_ERROR);
	}
	kobj->init = TRUE;
	kobj->objID = RK_SEMAPHORE_KOBJ_ID;
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kSemaPend( RK_SEMA *const kobj, const RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kIsISR())
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	kobj->value--;
	_RK_DMB
	if (kobj->value < 0)
	{
		if (timeout == RK_NO_WAIT)
		{
			/* restore value and return */
			kobj->value++;
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
			kobj->value += 1;
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
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	RK_TCB *nextTCBPtr = NULL;
	(kobj->value) = (kobj->value) + 1;
	if (kobj->value == RK_INT_MAX - 1)
	{
		RK_CR_EXIT
		return (RK_ERR_OVERFLOW);
	}
	_RK_DMB
	if ((kobj->value) <= 0)
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
	RK_CR_EXIT
	return (RK_SUCCESS);
}

INT kSemaQuery( RK_SEMA *const kobj)
{
	if (kobj && kobj->init)
	{
		return (kobj->value);
	}
	return (RK_INT_MAX);
}

#endif /* semaphore */

#if (RK_CONF_MUTEX == ON)
/*******************************************************************************
 * MUTEX SEMAPHORE
 *******************************************************************************/
/* mutex handle priority inversion by default */
/* there is no recursive lock */
/* unlocking a mutex you do not own leads to hard fault */
/* queue discipline is either priority (default) or fifo */

RK_ERR kMutexInit( RK_MUTEX *const kobj)
{

	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
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

RK_ERR kMutexLock( RK_MUTEX *const kobj, BOOL prioInh, RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj->init == FALSE)
	{
		kassert( 0);
	}
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
	}
	if (kIsISR())
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
	}
	if (kobj->lock == FALSE)
	{
		/* lock mutex and set the owner */
		kobj->lock = TRUE;
		kobj->ownerPtr = runPtr;
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
		KERR( RK_FAULT_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
	}
	if ((kobj->lock == FALSE ))
	{
		return (RK_ERR_MUTEX_NOT_LOCKED);
	}
	if (kobj->ownerPtr != runPtr)
	{
		KERR( RK_FAULT_UNLOCK_OWNED_MUTEX);
		RK_CR_EXIT
		return (RK_ERR_MUTEX_NOT_OWNER);
	}
	/* runPtr is the owner and mutex was locked */
	if (kobj->waitingQueue.size == 0)
	{
		kobj->lock = FALSE;
		if (kobj->ownerPtr->priority != kobj->ownerPtr->realPrio)
		{
			/* restore owner priority */
			kobj->ownerPtr->priority = kobj->ownerPtr->realPrio;
		}
		tcbPtr = kobj->ownerPtr;
		kobj->ownerPtr = NULL;
	}
	else
	{
		/* there are waiters, unblock a waiter set new mutex owner.
		 * mutex is still locked */
		kTCBQDeq( &(kobj->waitingQueue), &tcbPtr);
		if (RK_IS_NULL_PTR( tcbPtr))
			KERR( RK_FAULT_OBJ_NULL);
		/* here only runptr can unlock a mutex*/
		if (runPtr->priority < runPtr->realPrio)
		{
			runPtr->priority = runPtr->realPrio;
		}
		if (!kReadyCtxtSwtch( tcbPtr))
		{
			kobj->ownerPtr = tcbPtr;
		}
		else
		{
			KERR( RK_FAULT_READY_QUEUE);
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
	if (!kobj->init || kobj == NULL)
	{
		RK_CR_EXIT
		return (-1L);
	}
		RK_CR_EXIT	
		return ((LONG)(kobj->lock));
}

#endif /* mutex */
