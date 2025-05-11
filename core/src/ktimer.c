/******************************************************************************
 *                     RK0 - Real-Time Kernel '0'
 * Version           :   V0.4.0
 * Target            :   ARMv6/7-M
 *
 * Copyright (c) 2025 Antonio Giacomelli
 ******************************************************************************/
/******************************************************************************
 * 	Module           : APPLICATION TIMER
 * 	Depends on       : LOW-LEVEL SCHEDULER
 *  Provides to      : APPLICATION, SYNCHRONISATION, COMMUNICATION
 *  Public API   	 : YES
 ******************************************************************************/

 #define RK_CODE

 #include "kservices.h"
 #ifndef RK_TASK_SLEEP_TIMEOUT_SETUP
 #define RK_TASK_SLEEP_TIMEOUT_SETUP \
	  runPtr->timeoutNode.timeoutType = RK_TIMEOUT_SLEEP; \
	  runPtr->timeoutNode.waitingQueuePtr = NULL;
 #endif
 
 /******************************************************************************
  * GLOBAL TICK RETURN
  *****************************************************************************/
 RK_TICK kTickGet( void)
 {
	 return (runTime.globalTick);
 }
 
 #if (RK_CONF_CALLOUT_TIMER==ON)
 /******************************************************************************
  * CALLOUT TIMERS
  *****************************************************************************/
 RK_TIMER *currTimerPtr = NULL;
 
 static inline VOID kTimerListAdd_( RK_TIMER *kobj, RK_TICK phase,
		 RK_TICK duration, RK_TIMER_CALLOUT funPtr, VOID * argsPtr, BOOL reload)
 {
	 kobj->timeoutNode.dtick = duration;
	 kobj->timeoutNode.timeout = duration;
	 kobj->timeoutNode.timeoutType = RK_TIMEOUT_TIMER;
	 kobj->funPtr = funPtr;
	 kobj->argsPtr = argsPtr;
	 kobj->reload = reload;
	 kobj->phase = phase;
	 kTimeOut( &kobj->timeoutNode, duration);
 }
 
 RK_ERR kTimerInit( RK_TIMER *const kobj, RK_TICK const phase,
		 RK_TICK const duration, RK_TIMER_CALLOUT const funPtr,
		 VOID * const argsPtr, BOOL const reload)
 {
	 if ((kobj == NULL) || (funPtr == NULL))
	 {
		 return (RK_ERR_OBJ_NULL);
	 }
	 RK_CR_AREA
	 RK_CR_ENTER
	 kTimerListAdd_( kobj, phase, duration, funPtr, argsPtr, reload);
	 RK_CR_EXIT
	 return (RK_SUCCESS);
 }
 
 
 VOID kRemoveTimerNode( RK_TIMEOUT_NODE *node)
 {
	 if (node == NULL)
		 return;
 
	 if (node->nextPtr != NULL)
	 {
		 node->nextPtr->dtick += node->dtick;
		 node->nextPtr->prevPtr = node->prevPtr;
	 }
 
	 if (node->prevPtr != NULL)
	 {
		 node->prevPtr->nextPtr = node->nextPtr;
	 }
	 else
	 {
		 timerListHeadPtr = node->nextPtr;
	 }
 
	 node->nextPtr = NULL;
	 node->prevPtr = NULL;
 }
 
 
 RK_ERR kTimerCancel( RK_TIMER *const kobj)
 {
	 RK_CR_AREA
	 RK_CR_ENTER
	 
	 if (kobj == NULL)
	 {
		 RK_CR_EXIT
		 return (RK_ERR_OBJ_NULL);
	 }
	 else
	 {
 
		 RK_TIMEOUT_NODE*  node = (RK_TIMEOUT_NODE*)&kobj->timeoutNode;
		
		 if ((node->nextPtr == NULL) && (node->prevPtr == NULL))
		 {
			 RK_CR_EXIT
			 return (RK_ERROR);
		 }
		 if (node->nextPtr != NULL)
		 {
			 node->nextPtr->dtick += node->dtick;
			 node->nextPtr->prevPtr = node->prevPtr;
		 }
	 
		 if (node->prevPtr != NULL)
		 {
			 node->prevPtr->nextPtr = node->nextPtr;
		 }
		 else
		 {	
			 timerListHeadPtr = node->nextPtr;
		 }
		 node->nextPtr = NULL;
		 node->prevPtr = NULL;
	 }
	 RK_CR_EXIT
	 return (RK_SUCCESS);
 }
 #endif
RK_ERR kBusyWait( RK_TICK const ticks)
{
	 if (ticks == 0)
  {
	return (RK_ERR_INVALID_PARAM);
  }
  
  RK_TICK tickstart = (RK_TICK) kCoreGetSysTickValue();
  RK_TICK wait = tickstart + ticks + 1;
  
 
  while((RK_TICK)(kCoreGetSysTickValue()) < wait)
  {
  }

  	return (RK_SUCCESS);

}
 /*******************************************************************************
  * SLEEP TIMER AND BLOCKING TIME-OUT
  *******************************************************************************/
 RK_ERR kSleep( RK_TICK ticks)
 {
	 RK_CR_AREA
	 RK_CR_ENTER
 
	 if (runPtr->status != RK_RUNNING)
	 {
		 RK_CR_EXIT
		 return (RK_ERR_TASK_INVALID_ST);
	 }
	 if (ticks <= 0)
	 {
		 RK_CR_EXIT
		 return (RK_ERR_INVALID_PARAM);
	 }
	 RK_TASK_SLEEP_TIMEOUT_SETUP
 
	 kTimeOut( &runPtr->timeoutNode, ticks);
	 runPtr->status = RK_SLEEPING;
	 RK_CR_EXIT
	 RK_PEND_CTXTSWTCH
	 return (RK_SUCCESS);
 }
 
 #if(RK_CONF_SCH_TSLICE!=ON)
 
 RK_ERR kSleepUntil( RK_TICK const period)
 {
	 if (period <= 0)
	 {
 
		 return (RK_ERR_INVALID_PARAM);
 
	 }
	 RK_CR_AREA
	 RK_CR_ENTER
	 RK_TICK nextWakeTime = 0;
	 RK_TICK currentTick = 0;
	 currentTick = kTickGet();
	 nextWakeTime = runPtr->lastWakeTime + period;
	 /*  the task missed its period deadline, adjust nextWakeTime to catch up */
	 if (currentTick > nextWakeTime)
	 {
		 nextWakeTime = currentTick + period;
	 }
	 /* calc delay */
	 volatile RK_TICK delay = nextWakeTime - currentTick;
	 /* if any */
	 if (delay > 0)
	 {
		 RK_TASK_SLEEP_TIMEOUT_SETUP
 
		 if (!kTimeOut( &runPtr->timeoutNode, delay))
		 {
			 runPtr->status = RK_SLEEPING;
			 RK_PEND_CTXTSWTCH
		 }
		 else
		 {
			 kassert( 0);
		 }
	 }
	 runPtr->lastWakeTime = nextWakeTime;
	 RK_CR_EXIT
	 return (RK_SUCCESS);
 }
 
 #endif
 
 /* add caller to timeout list (delta-list) */
 RK_ERR kTimeOut( RK_TIMEOUT_NODE *timeOutNode, RK_TICK timeout)
 {
 
	 if (timeout <= 0)
		 return (RK_ERR_INVALID_PARAM);
	 if (timeOutNode == NULL)
		 return (RK_ERR_OBJ_NULL);
 
	 runPtr->timeOut = FALSE;
	 timeOutNode->timeout = timeout;
	 timeOutNode->dtick = timeout;
	 timeOutNode->prevPtr = NULL;
	 timeOutNode->nextPtr = NULL;
 
	 timeOutNode->timeout = timeout;
	 timeOutNode->dtick = timeout;
 
	 if (timeOutNode->timeoutType == RK_TIMEOUT_TIMER)
	 {
		 RK_TIMEOUT_NODE *currPtr = (RK_TIMEOUT_NODE*) timerListHeadPtr;
		 RK_TIMEOUT_NODE *prevPtr = NULL;
 
		 while (currPtr != NULL && currPtr->dtick < timeOutNode->dtick)
		 {
			 timeOutNode->dtick -= currPtr->dtick;
			 prevPtr = currPtr;
			 currPtr = currPtr->nextPtr;
		 }
 
		 timeOutNode->nextPtr = currPtr;
		 if (currPtr != NULL)
		 {
			 currPtr->dtick -= timeOutNode->dtick;
			 timeOutNode->prevPtr = currPtr->prevPtr;
			 currPtr->prevPtr = timeOutNode;
		 }
		 else
		 {
			 timeOutNode->prevPtr = prevPtr;
		 }
 
		 if (prevPtr == NULL)
		 {
			 timerListHeadPtr = timeOutNode;
		 }
		 else
		 {
			 prevPtr->nextPtr = timeOutNode;
		 }
	 }
	 else
	 {
		 RK_TIMEOUT_NODE *currPtr = (RK_TIMEOUT_NODE*) timeOutListHeadPtr;
		 RK_TIMEOUT_NODE *prevPtr = NULL;
 
		 while (currPtr != NULL && currPtr->dtick < timeOutNode->dtick)
		 {
			 timeOutNode->dtick -= currPtr->dtick;
			 prevPtr = currPtr;
			 currPtr = currPtr->nextPtr;
		 }
 
		 timeOutNode->nextPtr = currPtr;
		 if (currPtr != NULL)
		 {
			 currPtr->dtick -= timeOutNode->dtick;
			 timeOutNode->prevPtr = currPtr->prevPtr;
			 currPtr->prevPtr = timeOutNode;
		 }
		 else
		 {
			 timeOutNode->prevPtr = prevPtr;
		 }
 
		 if (prevPtr == NULL)
		 {
			 timeOutListHeadPtr = timeOutNode;
		 }
		 else
		 {
			 prevPtr->nextPtr = timeOutNode;
		 }
	 }
	 return (RK_SUCCESS);
 }
 
 /* Ready the task associated to a time-out node, accordingly to its time-out type */
 
 RK_ERR kTimeOutReadyTask( volatile RK_TIMEOUT_NODE *node)
 {
	 RK_TCB *taskPtr = RK_GET_CONTAINER_ADDR( node, RK_TCB, timeoutNode);
 
	 if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
	 {
		 RK_ERR err = kTCBQRem( taskPtr->timeoutNode.waitingQueuePtr, &taskPtr);
		 if (err == RK_SUCCESS)
		 {
			 if (!kTCBQEnq( &readyQueue[taskPtr->priority], taskPtr))
			 {
				 taskPtr->timeOut = TRUE;
				 taskPtr->status = RK_READY;
				 taskPtr->timeoutNode.timeoutType = 0;
				 taskPtr->timeoutNode.waitingQueuePtr = NULL;
			 }
 
			 return (RK_SUCCESS);
		 }
	 }
	 if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_SLEEP)
	 {
		 if (!kTCBQEnq( &readyQueue[taskPtr->priority], taskPtr))
		 {
			 taskPtr->timeOut = FALSE; /* does not set timeout true*/
			 taskPtr->status = RK_READY;
			 taskPtr->timeoutNode.timeoutType = 0;
			 return (RK_SUCCESS);
		 }
	 }
 
	 if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_ELAPSING)
	 {
		 if (!kTCBQEnq( &readyQueue[taskPtr->priority], taskPtr))
		 {
			 taskPtr->timeOut = TRUE;
			 taskPtr->status = RK_READY;
			 taskPtr->timeoutNode.timeoutType = 0;
			 return (RK_SUCCESS);
		 }
	 }
	 return (RK_ERROR);
 }
 
 /* runs @ systick */
 static volatile RK_TIMEOUT_NODE *node;
 BOOL kHandleTimeoutList( VOID)
 {
	 RK_ERR err = RK_ERROR;
 
	 if (timeOutListHeadPtr->dtick > 0)
	 {
		 timeOutListHeadPtr->dtick--;
	 }
 
	 /*  possible to have a node which offset is already (dtick == 0) */
	 while (timeOutListHeadPtr != NULL && timeOutListHeadPtr->dtick == 0)
	 {
		 node = timeOutListHeadPtr;
		 /* Remove the expired node from the list */
		 timeOutListHeadPtr = node->nextPtr;
		 kRemoveTimeoutNode( (RK_TIMEOUT_NODE*) node);
 
		 err = kTimeOutReadyTask( node);
 
	 }
 
	 return (err == RK_SUCCESS);
 }
 VOID kRemoveTimeoutNode( RK_TIMEOUT_NODE *node)
 {
	 if (node == NULL)
		 return;
 
	 if (node->nextPtr != NULL)
	 {
		 node->nextPtr->dtick += node->dtick;
		 node->nextPtr->prevPtr = node->prevPtr;
	 }
 
	 if (node->prevPtr != NULL)
	 {
		 node->prevPtr->nextPtr = node->nextPtr;
	 }
	 else
	 {
		 timeOutListHeadPtr = node->nextPtr;
	 }
 
	 node->nextPtr = NULL;
	 node->prevPtr = NULL;
 }
 