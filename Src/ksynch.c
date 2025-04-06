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

 #include "kexecutive.h"
 
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
#if(RK_CONF_SIGNAL_FLAGS==ON)

RK_ERR kFlagsPend( ULONG const required, ULONG *const gotFlagsPtr,
        ULONG const options, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (kIsISR())
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (options != RK_FLAGS_ALL && options != RK_FLAGS_ANY)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (gotFlagsPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    runPtr->requiredTaskFlags = required;
    runPtr->taskFlagsOptions = options;

    *gotFlagsPtr = runPtr->currentTaskFlags;

    BOOL andLogic = 0;
    BOOL conditionMet = 0;
    if (options == RK_FLAGS_ALL)
    {
        andLogic = 1;
    }

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
    if (timeout > RK_NO_WAIT && timeout < RK_WAIT_FOREVER)
        kRemoveTimeoutNode( &runPtr->timeoutNode);

    *gotFlagsPtr = runPtr->currentTaskFlags;

    runPtr->currentTaskFlags &= ~runPtr->requiredTaskFlags;

    RK_CR_EXIT

    return (RK_SUCCESS);

}

RK_ERR kFlagsPost( RK_TASK_HANDLE const taskHandle, ULONG const mask)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (taskHandle == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
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

RK_ERR kFlagsClear( VOID)
{
    if (kIsISR())
    {
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    (runPtr->currentTaskFlags = 0UL);
    return (RK_SUCCESS);
}

RK_ERR kFlagsQuery( ULONG *const queryFlagsPtr)
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

 #endif
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
     RK_ERR err = RK_ERROR;
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
         RK_TCB *nextTCBPtr;
         err = kTCBQDeq( &kobj->waitingQueue, &nextTCBPtr);
         if (err < 0)
             return (err);
         err = kTCBQEnq( &readyQueue[nextTCBPtr->priority], nextTCBPtr);
         if (err == 0)
             nextTCBPtr->status = RK_READY;
 
     }
     RK_CR_EXIT
     return (RK_SUCCESS);
 }
 /*
  Signal an Event - a Single Task Switches to READY
  Dequeued by priority
  */
 
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
     else
     {
         RK_TCB *nextTCBPtr;
         err = kTCBQDeq( &kobj->waitingQueue, &nextTCBPtr);
         if (err < 0)
             return (err);
     }
     RK_CR_EXIT
     return (RK_SUCCESS);
 }
 /* Returns the number of tasks sleeping for an event */
 ULONG kEventQuery( RK_EVENT *const kobj)
 {
     if (kobj == NULL)
     {
         KERR( RK_FAULT_OBJ_NULL);
         return (0);
     }
     return (kobj->waitingQueue.size);
 }
 #if (RK_CONF_EVENT_FLAGS==(ON))

 /* Returns the current event flags of an Event Object */
 ULONG kEventFlagsQuery( RK_EVENT *const kobj)
 {
     return (kobj->eventFlags);
 }
 /*
  *  When a task pends on a combination of flags associated to a EVENT_FLAGS
  *  object, first it checks if the current flags meet the asked combination.
  *  If so, task proceeds.
  *  if not, the task  switches to SLEEPING state.
  */
 RK_ERR kEventFlagsPend( RK_EVENT *const kobj, ULONG const requiredFlags,
         ULONG *const gotFlagsPtr, ULONG const options, RK_TICK const timeout)
 {
     RK_CR_AREA
     RK_CR_ENTER
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
 
     RK_ERR err = -1;
     BOOL clear = 0;
     BOOL all = 0;
     ULONG currFlags = kobj->eventFlags;
     runPtr->requiredEventFlags = requiredFlags;
     runPtr->eventFlagsOptions = options;
     switch (options)
     {
         case RK_FLAGS_ANY_KEEP:
             clear = 0;
             all = 0;
             break;
         case RK_FLAGS_ALL_KEEP:
             clear = 0;
             all = 1;
             break;
         case RK_FLAGS_ANY_CONSUME:
             clear = 1;
             all = 0;
             break;
         case RK_FLAGS_ALL_CONSUME:
             clear = 1;
             all = 1;
             break;
         default:
             goto EXIT;
     }
 
     runPtr->gotEventFlags = kobj->eventFlags;
     *gotFlagsPtr = runPtr->gotEventFlags;
 
 /* Check if event condition is already met */
     if ((all && ((currFlags & requiredFlags) == requiredFlags))
             || (!all && (currFlags & requiredFlags)))
     {
         err = RK_SUCCESS;
 
         if (clear)
         {
             kobj->eventFlags &= ~requiredFlags;
         }
     }
     else
     {
         if (timeout == RK_NO_WAIT)
         {
             runPtr->eventFlagsOptions = 0UL;
             runPtr->gotEventFlags = 0UL;
             RK_CR_EXIT
             return (RK_ERR_FLAGS_NOT_MET);
 
         }
         err = kMutexLock( &kobj->queueLock, timeout);
         if (err != RK_SUCCESS)
             kassert(0);
 
         RK_CR_EXIT
 
         err = kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
         if (!err)
             runPtr->status = RK_PENDING_EV_FLAGS;
         else
             kassert(0);
         if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
         {
             RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
 
             kTimeOut( &runPtr->timeoutNode, timeout);
         }

         RK_CR_EXIT
         
         kMutexUnlock(&kobj->queueLock);
 
 /* resuming here, if time is out, return error */
 
         if (runPtr->timeOut)
         {
             runPtr->timeOut = FALSE;
             err = RK_ERR_TIMEOUT;
             goto EXIT;
         }
 
         if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
             kRemoveTimeoutNode( &runPtr->timeoutNode);
 /* snap of the flags taken when task was made ready */
 
 
         *gotFlagsPtr = runPtr->gotEventFlags;
 
         err = RK_SUCCESS;
 
     }
     EXIT:
 
     RK_CR_EXIT
 
     runPtr->eventFlagsOptions = 0UL;
 
     runPtr->gotEventFlags = 0UL;
 
 
     return (err);
 }
 
 RK_ERR kEventFlagsPost( RK_EVENT *const kobj, ULONG const flagMask,
         ULONG *const updatedFlags, ULONG const options)
 {
 
    if (runPtr != kobj->ownerPtr)
    {
        retun (RK_ERR_OWNERSHIP);
    }

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
         return (RK_ERR_OBJ_NULL);
     }
 
 /* update the event flags */
     if (options == RK_FLAGS_OR)
     {
         kobj->eventFlags |= flagMask;
 
     }
     else if (options == RK_FLAGS_AND)
     {
         kobj->eventFlags &= flagMask;
     }
     else
     {
         *updatedFlags = kobj->eventFlags;
         RK_CR_EXIT
         return (RK_ERR_INVALID_PARAM);
     }
  
     *updatedFlags = kobj->eventFlags;
     BOOL conditionMet = 0;
     UINT nListeners;
 /* process waiting tasks */
     if (kobj->waitingQueue.size > 0)
     {
         RK_CR_EXIT
         RK_TCB *currTcbPtr = NULL;
         currTcbPtr = kTCBQPeek( &(kobj->waitingQueue));
         RK_NODE *currNodePtr = &currTcbPtr->tcbNode;
         kMutexLock(&kobj->queueLock, RK_WAIT_FOREVER);
         while (currNodePtr != &kobj->waitingQueue.listDummy && nListeners < kobj->nListeners)
         {
 
             BOOL all = (currTcbPtr->eventFlagsOptions & RK_FLAGS_ALL_KEEP)
                     || (currTcbPtr->eventFlagsOptions & RK_FLAGS_ALL_CONSUME);
 
             BOOL clear = (currTcbPtr->eventFlagsOptions & RK_FLAGS_ANY_CONSUME)
                     || (currTcbPtr->eventFlagsOptions & RK_FLAGS_ALL_CONSUME);
 
 /* wake all tasks which condition is met */
             if (all)
             {
                 conditionMet =  ((kobj->eventFlags & currTcbPtr->requiredEventFlags)
                                     == (currTcbPtr->requiredEventFlags));
             }
             else
             {
                 conditionMet =  (kobj->eventFlags
                         & currTcbPtr->requiredEventFlags);
             }
            if (conditionMet)
 
             {
                 currTcbPtr->gotEventFlags = kobj->eventFlags;
                 if (clear)
                 {
                     kobj->eventFlags &= ~currTcbPtr->requiredEventFlags;
                 }
                 kListRemove( &kobj->waitingQueue, currNodePtr);
                 kReadyCtxtSwtch( currTcbPtr);
                 currTcbPtr = kTCBQPeek( &(kobj->waitingQueue));
                 currNodePtr = &currTcbPtr->tcbNode;
             }
             else
             {
                 currNodePtr = currNodePtr->nextPtr;
                 currTcbPtr = RK_LIST_GET_TCB_NODE( currNodePtr, RK_TCB);
             }
         }
         kMutexUnlock(&kobj->queueLock);
 
     }
     runPtr->priority = runPtr->realPrio;
     kMutexUnlock(&kobj->eventLock);
     RK_CR_EXIT
     return (RK_SUCCESS);
 }
 #endif
 

 #endif
 
 #if (RK_CONF_SEMA == ON)
 /******************************************************************************
  * COUNTER SEMAPHORES
  ******************************************************************************/
 /* counter semaphores cannot initialise with a negative value */
 RK_ERR kSemaInit( RK_SEMA *const kobj, const LONG value)
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
     kobj->value --;
     _RK_DMB
     if (kobj->value < 0)
     {
         if (timeout == RK_NO_WAIT)
         {
 /* restore value and return */
             kobj->value ++;
             RK_CR_EXIT
             return (RK_ERR_BLOCKED_SEMA);
 
         }
         runPtr->status = RK_BLOCKED;
         kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
         if (timeout > RK_NO_WAIT && timeout < RK_WAIT_FOREVER)
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
 
         if (timeout > RK_NO_WAIT && timeout < RK_WAIT_FOREVER)
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
     if (kobj->value == RK_LONG_MAX - 1)
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
 #endif
 
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
 
 RK_ERR kMutexLock( RK_MUTEX *const kobj, RK_TICK const timeout)
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
 #if(RK_CONF_MUTEX_PRIO_INH==(ON))
         if (kobj->ownerPtr->priority > runPtr->priority)
         {
             kobj->ownerPtr->priority = runPtr->priority;
         }
 #endif
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
 #if (RK_CONF_MUTEX_PRIO_INH==(ON))
 /* restore owner priority */
         kobj->ownerPtr->priority = kobj->ownerPtr->realPrio;
 #endif
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
 #if ((RK_CONF_MUTEX_PRIO_INH==ON) )
 
 /* here only runptr can unlock a mutex*/
         if (runPtr->priority < runPtr->realPrio)
         {
             runPtr->priority = runPtr->realPrio;
         }
 #endif
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
 RK_ERR kMutexQuery( RK_MUTEX *const kobj)
 {
     RK_CR_AREA
     RK_CR_ENTER
     if (kobj == NULL)
     {
         RK_CR_EXIT
         return (RK_ERR_OBJ_NULL);
     }
     if (kobj->init == FALSE)
     {
         RK_CR_EXIT
         return (RK_ERR_OBJ_NOT_INIT);
     }
     if (kobj->lock == TRUE)
     {
         RK_CR_EXIT
         return (RK_QUERY_MUTEX_LOCKED);
     }
     if (kobj->lock == FALSE)
     {
         RK_CR_EXIT
         return (RK_QUERY_MUTEX_UNLOCKED);
     }
     RK_CR_EXIT
     return (RK_ERROR);
 }
 
 #endif /* mutex */
 
