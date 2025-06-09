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
 #if (0)
 /* not being used */
 RK_WALL_TICK kWallclockGetTicks( VOID)
{
    UINT high1, high2;
    INT low;

    do {
        high1 = runTime.nWraps;
        low   = runTime.globalTick;
        high2 = runTime.nWraps;
    } while (high1 != high2);

    return (((RK_WALL_TICK)high1 << 32) | (UINT)low);
}
#endif
RK_ERR kBusyWait(RK_TICK const ticks)
{
    if (kIsISR())
	{
		K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
    if (ticks <= 0)
        return (RK_ERR_INVALID_PARAM);

    RK_TICK start = kTickGet();
    RK_TICK deadline = K_TICK_ADD(start, ticks);

    while (!K_TICK_EXPIRED(deadline))
        ;

    return (RK_SUCCESS);
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
		 K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
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
	 	K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
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
 /*******************************************************************************
  * SLEEP TIMER AND BLOCKING TIME-OUT
  *******************************************************************************/
 RK_ERR kSleep( RK_TICK ticks)
 {
	 RK_CR_AREA
	 RK_CR_ENTER
 
	if (kIsISR())
	{
		K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
	}
	 if (runPtr->status != RK_RUNNING)
	 {
		 K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
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
 
  
 
RK_ERR kSleepUntil(RK_TICK period)
{
     if (period <= 0) 
	 {
        return (RK_ERR_INVALID_PARAM);
    }
	if ((UINT)(period) > RK_MAX_PERIOD)
	{
		return (RK_ERR_INVALID_PARAM);
	}
	
    RK_CR_AREA
    RK_CR_ENTER

    if (kIsISR()) 
	{
		K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    RK_TICK current   = kTickGet();
    RK_TICK baseWake  = runPtr->wakeTime;
    RK_TICK elapsed   = K_TICK_DELAY(current, baseWake);
	/* we late? if so, skips > 1 */
    RK_TICK skips     = (elapsed / period) + 1;
    /* next wake = base + skips*period  */
    RK_TICK offset    = (RK_TICK)(skips * period);
    RK_TICK nextWake  = K_TICK_ADD(baseWake, offset);
    RK_TICK delay     = K_TICK_DELAY(nextWake, current);
    if (delay > 0) 
	{
        RK_TASK_SLEEP_TIMEOUT_SETUP
         RK_ERR err = kTimeOut(&runPtr->timeoutNode, delay);
         kassert(err==0);
		 runPtr->status = RK_SLEEPING;
         RK_PEND_CTXTSWTCH
      
    }
    /* update for the next cycle */
    runPtr->wakeTime = nextWake;
    RK_CR_EXIT
    return RK_SUCCESS;
}

  /* add caller to timeout list (delta-list) */
 RK_ERR kTimeOut( RK_TIMEOUT_NODE *timeOutNode, RK_TICK timeout)
 {
 
	 if (timeout <= 0)
		 return (RK_ERR_INVALID_PARAM);

	 if ((UINT)(timeout) > RK_MAX_PERIOD)
	 {
		return (RK_ERR_INVALID_PARAM);
 	 }
	 if (timeOutNode == NULL)
	 {
		 K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
		 return (RK_ERR_OBJ_NULL);
	 }
	 runPtr->timeOut = FALSE;
	 timeOutNode->timeout = timeout;
	 timeOutNode->prevPtr = NULL;
	 timeOutNode->nextPtr = NULL; 
	 timeOutNode->dtick = timeout;
	 timeOutNode->checkInTime = kTickGet();
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
	 RK_TCB *taskPtr = K_GET_CONTAINER_ADDR( node, RK_TCB, timeoutNode);
 
	 
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
				 taskPtr->timeoutNode.checkOutTime = kTickGet();

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
			 taskPtr->timeoutNode.checkOutTime = kTickGet();

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
			 taskPtr->timeoutNode.checkOutTime = kTickGet();
			 return (RK_SUCCESS);
		 }
	 }
	 return (RK_ERROR);
 }
 
 /* runs @ systick */
 static volatile RK_TIMEOUT_NODE *nodeg;
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
		 nodeg = timeOutListHeadPtr;
		 /* Remove the expired nodeg from the list */
		 timeOutListHeadPtr = nodeg->nextPtr;
		 kRemoveTimeoutNode( (RK_TIMEOUT_NODE*) nodeg);
 
		 err = kTimeOutReadyTask( nodeg);
 
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
 