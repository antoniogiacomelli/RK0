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
 * 	Module           :  HIGH-LEVEL SCHEDULER
 * 	Provides to      :  SERVICES/APPLICATION
 *  Depends  on      :  LOW-LEVEL SCHEDULER
 *  Public API 		 :  YES
 *
 ******************************************************************************/

#define RK_CODE

#include "kservices.h"

/* scheduler globals */

RK_TCBQ readyQueue[RK_CONF_MIN_PRIO + 2];
RK_TCB *runPtr;
RK_TCB tcbs[RK_NTHREADS] __K_ALIGN(8);
RK_TASK_HANDLE timTaskHandle;
RK_TASK_HANDLE idleTaskHandle;
volatile struct kRunTime runTime;
/* local globals  */
static RK_PRIO highestPrio = 0;
static RK_PRIO const lowestPrio = RK_CONF_MIN_PRIO;
static RK_PRIO nextTaskPrio = 0;
static RK_PRIO idleTaskPrio = RK_CONF_MIN_PRIO + 1;
static ULONG readyQBitMask;
static ULONG readyQRightMask;
static ULONG version;
/* fwded private helpers */
static inline VOID kPreemptRunningTask_( VOID);
static inline VOID kYieldRunningTask_( VOID);
static inline RK_PRIO kCalcNextTaskPrio_();
 
/*******************************************************************************
 * YIELD
 *******************************************************************************/

VOID kYield( VOID)/*  <- USE THIS =) */
{
	RK_CR_AREA
	RK_CR_ENTER
	kYieldRunningTask_();
	RK_PEND_CTXTSWTCH
	RK_CR_EXIT

}

/*******************************************************************************
 TASK QUEUE MANAGEMENT
 *******************************************************************************/

RK_ERR kTCBQInit( RK_TCBQ *const kobj, CHAR *listName)
{
	if (kobj == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	return (kListInit( kobj, listName));
}

RK_ERR kTCBQEnq( RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{

	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL || tcbPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
		
	}
	RK_ERR err = kListAddTail( kobj, &(tcbPtr->tcbNode));
	if (err == 0)
	{
		if (kobj == &readyQueue[tcbPtr->priority])
			readyQBitMask |= 1 << tcbPtr->priority;
	}
	RK_CR_EXIT
	return (err);
}
RK_ERR kTCBQJam( RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{

	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL || tcbPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
	    return (RK_ERR_OBJ_NULL);

	}
	RK_ERR err = kListAddHead( kobj, &(tcbPtr->tcbNode));
	if (err == 0)
	{
		if (kobj == &readyQueue[tcbPtr->priority])
			readyQBitMask |= 1 << tcbPtr->priority;
	}
	RK_CR_EXIT
	return (err);
}
RK_ERR kTCBQDeq( RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
	if (kobj == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	RK_NODE *dequeuedNodePtr = NULL;
	RK_ERR err = kListRemoveHead( kobj, &dequeuedNodePtr);

	if (err != RK_SUCCESS)
	{
		return (err);
	}
	*tcbPPtr = RK_LIST_GET_TCB_NODE( dequeuedNodePtr, RK_TCB);

	if (*tcbPPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	RK_TCB const *tcbPtr_ = *tcbPPtr;
	RK_PRIO prio_ = tcbPtr_->priority;
	if ((kobj == &readyQueue[prio_]) && (kobj->size == 0))
		readyQBitMask &= ~(1U << prio_);
	return (RK_SUCCESS);
}

RK_ERR kTCBQRem( RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
	if (kobj == NULL || tcbPPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	RK_NODE *dequeuedNodePtr = &((*tcbPPtr)->tcbNode);
	RK_ERR err = kListRemove( kobj, dequeuedNodePtr);
	if (err != RK_SUCCESS)
	{
		return (err);
	}
	*tcbPPtr = RK_LIST_GET_TCB_NODE( dequeuedNodePtr, RK_TCB);
	if (*tcbPPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	RK_TCB const *tcbPtr_ = *tcbPPtr;
	RK_PRIO prio_ = tcbPtr_->priority;
	if ((kobj == &readyQueue[prio_]) && (kobj->size == 0))
		readyQBitMask &= ~(1U << prio_);
	return (RK_SUCCESS);
}

RK_TCB* kTCBQPeek( RK_TCBQ *const kobj)
{
	if (kobj == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (NULL);
	}
	RK_NODE *nodePtr = kobj->listDummy.nextPtr;
	return (K_GET_CONTAINER_ADDR( nodePtr, RK_TCB, tcbNode));
}

RK_ERR kTCBQEnqByPrio( RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
	RK_ERR err;
	if (kobj == NULL || tcbPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->size == 0)
	{
		/* enq on tail */
		err = kTCBQEnq( kobj, tcbPtr);
		return (err);
	}
	/* start on the tail and traverse with > cond,    */
	/*  so we use a single insertafter.                */
	RK_NODE *currNodePtr = kobj->listDummy.prevPtr;
	RK_TCB const *currTcbPtr = RK_LIST_GET_TCB_NODE( currNodePtr, RK_TCB);
	while (currTcbPtr->priority > tcbPtr->priority)
	{
		currNodePtr = currNodePtr->nextPtr;
		/* list end */
		if (currNodePtr == &(kobj->listDummy))
		{
			break;
		}
		currTcbPtr = RK_LIST_GET_TCB_NODE( currNodePtr, RK_TCB);
	}
	err = kListInsertAfter( kobj, currNodePtr, &(tcbPtr->tcbNode));
	kassert( err == 0);
	return (err);
}

/* this function enq ready and pend ctxt swtch if ready > running */
RK_ERR kReadyCtxtSwtch( RK_TCB *const tcbPtr)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (tcbPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	kassert(kTCBQEnq( &readyQueue[tcbPtr->priority], tcbPtr) == RK_SUCCESS);	
	tcbPtr->status = RK_READY;
	if (runPtr->priority > tcbPtr->priority)
	{
			kassert( !kTCBQJam( &readyQueue[runPtr->priority], runPtr));
			runPtr->status = RK_READY;
			RK_PEND_CTXTSWTCH
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kReadyQDeq( RK_TCB **const tcbPPtr, RK_PRIO priority)
{

	RK_ERR err = kTCBQDeq( &readyQueue[priority], tcbPPtr);
	kassert( err == 0);
	return (err);
}

/*******************************************************************************
 * TASK CONTROL BLOCK MANAGEMENT
 *******************************************************************************/
static RK_PID pPid = 0;/** system pid for each task   */

static RK_ERR kInitStack_( UINT *const stackAddrPtr, UINT const stackSize,
		RK_TASKENTRY const taskFuncPtr, VOID *argsPtr)
{

	if (stackAddrPtr == NULL || stackSize == 0 || taskFuncPtr == NULL)
	{
		return (RK_ERROR);
	}
	stackAddrPtr[stackSize - PSR_OFFSET] = 0x01000000;
	stackAddrPtr[stackSize - PC_OFFSET] = (UINT) taskFuncPtr;
	stackAddrPtr[stackSize - LR_OFFSET] = 0x14141414;
	stackAddrPtr[stackSize - R12_OFFSET] = 0x12121212;
	stackAddrPtr[stackSize - R3_OFFSET] = 0x03030303;
	stackAddrPtr[stackSize - R2_OFFSET] = 0x02020202;
	stackAddrPtr[stackSize - R1_OFFSET] = 0x01010101;
	stackAddrPtr[stackSize - R0_OFFSET] = (UINT) (argsPtr);
	stackAddrPtr[stackSize - R11_OFFSET] = 0x11111111;
	stackAddrPtr[stackSize - R10_OFFSET] = 0x10101010;
	stackAddrPtr[stackSize - R9_OFFSET] = 0x09090909;
	stackAddrPtr[stackSize - R8_OFFSET] = 0x08080808;
	stackAddrPtr[stackSize - R7_OFFSET] = 0x07070707;
	stackAddrPtr[stackSize - R6_OFFSET] = 0x06060606;
	stackAddrPtr[stackSize - R5_OFFSET] = 0x05050505;
	stackAddrPtr[stackSize - R4_OFFSET] = 0x04040404;
	/*stack painting*/
	for (ULONG j = 17; j < stackSize; j++)
	{
		stackAddrPtr[stackSize - j] =  RK_STACK_PATTERN;
	}
	stackAddrPtr[0] = RK_STACK_GUARD;
	return (RK_SUCCESS);
}

static RK_ERR kInitTcb_( RK_TASKENTRY const taskFuncPtr, VOID *argsPtr,
		UINT *const stackAddrPtr, UINT const stackSize)
{
	if (kInitStack_( stackAddrPtr, stackSize, taskFuncPtr,
			argsPtr) == RK_SUCCESS)
	{
		tcbs[pPid].stackAddrPtr = stackAddrPtr;
		tcbs[pPid].sp = &stackAddrPtr[stackSize - R4_OFFSET];
		tcbs[pPid].stackSize = stackSize;
		tcbs[pPid].status = RK_READY;
		tcbs[pPid].pid = pPid;
		tcbs[pPid].savedLR = 0xFFFFFFFD;
		return (RK_SUCCESS);
	}
	return (RK_ERROR);
}

RK_ERR kCreateTask( RK_TASK_HANDLE *taskHandlePtr,
		const RK_TASKENTRY taskFuncPtr, CHAR *const taskName,
		UINT *const stackAddrPtr, const UINT stackSize, VOID *argsPtr,
		const RK_PRIO priority, const BOOL runToCompl)
{

	/* if private PID is 0, system tasks hasn't been started yet */
	if (pPid == 0)
	{
		/* initialise IDLE TASK */
		kassert(
				kInitTcb_(IdleTask, argsPtr, idleStack, RK_CONF_IDLE_STACKSIZE) == RK_SUCCESS);

		tcbs[pPid].priority = idleTaskPrio;
		tcbs[pPid].prioReal = idleTaskPrio;
		tcbs[pPid].taskName = "IdleTask";
		tcbs[pPid].runToCompl = FALSE;
 
		idleTaskHandle = &tcbs[pPid];
		pPid += 1;

		/* initialise TIMER HANDLER TASK */
		kassert(
				kInitTcb_(TimerHandlerTask, argsPtr, timerHandlerStack, RK_CONF_TIMHANDLER_STACKSIZE) == RK_SUCCESS);

		tcbs[pPid].priority = 0;
		tcbs[pPid].prioReal = 0;
		tcbs[pPid].taskName = "TimHandlerTask";
		tcbs[pPid].runToCompl = TRUE;
		timTaskHandle = &tcbs[pPid];
		pPid += 1;

	}
	/* initialise user tasks */
	if (kInitTcb_( taskFuncPtr, argsPtr, stackAddrPtr, stackSize) == RK_SUCCESS)
	{
		if (priority > idleTaskPrio)
		{
			kErrHandler( RK_FAULT_TASK_INVALID_PRIO);
		}
		tcbs[pPid].priority = priority;
		tcbs[pPid].prioReal = priority;
		tcbs[pPid].taskName = taskName;
		tcbs[pPid].wakeTime = 0;
		tcbs[pPid].runToCompl = runToCompl;
		*taskHandlePtr = &tcbs[pPid];
		pPid += 1;
		return (RK_SUCCESS);
	}

	return (RK_ERROR);
}

/******************************************************************************
 * KERNEL INITIALISATION
 *******************************************************************************/

static VOID kInitRunTime_( VOID)
{
	runTime.globalTick = 0;
	runTime.nWraps = 0;
}
static RK_ERR kInitQueues_( VOID)
{
	RK_ERR err = 0;
	for (RK_PRIO prio = 0; prio < RK_NPRIO + 1; prio++)
	{
		err |= kTCBQInit( &readyQueue[prio], "ReadyQ");
	}
	kassert( err == 0);
	return (err);
}

VOID kInit( VOID)
{

	version = kGetVersion();
	if (version != RK_VALID_VERSION)
		kErrHandler( RK_FAULT_KERNEL_VERSION);
	

	kApplicationInit();
	
	kInitQueues_();
	kInitRunTime_();
	highestPrio = tcbs[0].priority;
	for (ULONG i = 0; i < RK_NTHREADS; i++)
	{
		if (tcbs[i].priority < highestPrio)
		{
			highestPrio = tcbs[i].priority;
		}
	}

	for (ULONG i = 0; i < RK_NTHREADS; i++)
	{
		kTCBQEnq( &readyQueue[tcbs[i].priority], &tcbs[i]);
	}
	kReadyQDeq( &runPtr, highestPrio);
	kassert( runPtr->status == RK_READY);
	kassert( tcbs[RK_IDLETASK_ID].priority == lowestPrio+1);
	_RK_DSB
    __ASM volatile ("cpsie i" : : : "memory");
	_RK_ISB
	/* calls low-level scheduler for start-up */
	_RK_STUP
	while (1)
		;

}

/*******************************************************************************
 TASK SWITCHING LOGIC
 *******************************************************************************/
static inline RK_PRIO kCalcNextTaskPrio_()
{
	if (readyQBitMask == 0U)
	{
		return (idleTaskPrio);
	}
	readyQRightMask = readyQBitMask & -readyQBitMask;
	RK_PRIO prio = (RK_PRIO) (__getReadyPrio( readyQRightMask));

	return (prio);
}
VOID kSchSwtch( VOID)
{

	RK_TCB *nextRunPtr = NULL;
	RK_TCB *prevRunPtr = runPtr;
	if (runPtr->status == RK_RUNNING)
	{

		kPreemptRunningTask_();
	}
	nextTaskPrio = kCalcNextTaskPrio_();/* get the next task priority */
	kTCBQDeq( &readyQueue[nextTaskPrio], &nextRunPtr);
	if (nextRunPtr == NULL)
	{
		kErrHandler( RK_FAULT_OBJ_NULL);
		return; /* suppress static analyser warning */
	}
	runPtr = nextRunPtr;
	if (nextRunPtr->pid != prevRunPtr->pid)
	{
		runPtr->nPreempted += 1U;
		prevRunPtr->preemptedBy = runPtr->pid;
	}
}

/*******************************************************************************
 *  TICK MANAGEMENT
 *******************************************************************************/
static inline VOID kPreemptRunningTask_( VOID)
{
	if (runPtr->status == RK_RUNNING)
	{

		kassert(
				(kTCBQJam( &readyQueue[runPtr->priority], runPtr) == (RK_SUCCESS)));
		runPtr->status = RK_READY;

	}
}

static inline VOID kYieldRunningTask_( VOID)
{
	if (runPtr->status == RK_RUNNING)
	{

		kassert(
				(kTCBQEnq( &readyQueue[runPtr->priority], runPtr) == (RK_SUCCESS)));
		runPtr->status = RK_READY;

	}
}

 volatile RK_TIMEOUT_NODE *timeOutListHeadPtr = NULL;
volatile RK_TIMEOUT_NODE *timerListHeadPtr = NULL;
volatile RK_TIMER *headTimPtr;


BOOL kTickHandler( VOID)
{
	/* return is short-circuit to !runToCompl & */
	BOOL runToCompl = FALSE;
	BOOL timeOutTask = FALSE;
	BOOL isPending = FALSE;
	BOOL ret = FALSE;
	isPending = kCoreGetPendingInterrupt( RK_CORE_PENDSV_IRQN);

	runTime.globalTick += 1;
	if (runTime.globalTick == RK_TICK_TYPE_MAX)
	{
		runTime.globalTick = 0U;
		runTime.nWraps += 1U;
	}
	/* handle time out and sleeping list */
	/* the list is not empty, decrement only the head  */
	if (timeOutListHeadPtr != NULL)
	{
		timeOutTask = kHandleTimeoutList();
	}
	/* a run-to-completion task is a first-class citizen not prone to tick
	 truculence.*/
	if (runPtr->runToCompl && (runPtr->status == RK_RUNNING)
			&& (runPtr->pid != RK_IDLETASK_ID))
	{
		/* this flag toggles, short-circuiting the */
		/* return value  to FALSE                  */
		runToCompl = TRUE;
	}
 
#if (RK_CONF_CALLOUT_TIMER==ON)
	RK_TIMER *headTimPtr__ = K_GET_CONTAINER_ADDR( timerListHeadPtr, RK_TIMER,
			timeoutNode);

	if (timerListHeadPtr != NULL)
	{

		if (headTimPtr__->phase > 0)
		{
			headTimPtr__->phase--;
		}
		else
		{
			if (timerListHeadPtr->dtick > 0)
				((RK_TIMEOUT_NODE*) timerListHeadPtr)->dtick--;
		}
	}
	if (timerListHeadPtr != NULL && timerListHeadPtr->dtick == 0)
	{
		kSignalSet(timTaskHandle, RK_SIG_TIMER);
		timeOutTask = TRUE;
	}
#endif
	/* finally we check for any higher priority ready tasks */
	/* if the current is not ready */
	if (runPtr->status == RK_RUNNING)
	{
		RK_PRIO highestReadyPrio = kCalcNextTaskPrio_();
		if (highestReadyPrio < runPtr->priority)
		{
			kPreemptRunningTask_();
		}
	}
	ret = ((!runToCompl) && ((runPtr->status == RK_READY) || timeOutTask || isPending));

	return (ret);
}

