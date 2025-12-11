/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.4                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************
 *
 * 	COMPONENT        :  HIGH-LEVEL SCHEDULER
 * 	PROVIDES TO      :  APPLICATION
 *  DEPENDS ON       :  LOW-LEVEL SCHEDULER
 *  PUBLIC API 		 :  YES
 *
 ******************************************************************************/

#define RK_SOURCE_CODE

#include <ksch.h>

/* scheduler globals */
RK_TCBQ RK_gReadyQueue[RK_CONF_MIN_PRIO + 2];
RK_TCB *RK_gRunPtr;
RK_TCB RK_gTcbs[RK_NTHREADS];
RK_TASK_HANDLE RK_gPostProcTaskHandle;
RK_TASK_HANDLE RK_gIdleTaskHandle;
volatile struct RK_OBJ_RUNTIME RK_gRunTime;
volatile ULONG RK_gReadyBitmask;
volatile ULONG RK_gReadyPos;
volatile UINT RK_gPendingCtxtSwtch = 0;

/* local globals  */
static RK_PRIO highestPrio = 0;
static RK_PRIO const lowestPrio = RK_CONF_MIN_PRIO;
static RK_PRIO nextTaskPrio = 0;
static RK_PRIO const idleTaskPrio = RK_CONF_MIN_PRIO + 1;
static ULONG version;

/******************************************************************************/
/* DOUBLY LINKED LIST.                                                        */
/******************************************************************************/

RK_ERR kListInit(RK_LIST *const kobj)
{
    K_ASSERT(kobj != NULL);
    kobj->listDummy.nextPtr = &(kobj->listDummy);
    kobj->listDummy.prevPtr = &(kobj->listDummy);
    kobj->size = 0U;
    return (RK_ERR_SUCCESS);
}

RK_ERR kListInsertAfter(RK_LIST *const kobj, RK_NODE *const refNodePtr,
                                      RK_NODE *const newNodePtr)
{
    K_ASSERT(kobj != NULL && newNodePtr != NULL && refNodePtr != NULL);
    newNodePtr->nextPtr = refNodePtr->nextPtr;
    refNodePtr->nextPtr->prevPtr = newNodePtr;
    newNodePtr->prevPtr = refNodePtr;
    refNodePtr->nextPtr = newNodePtr;
    kobj->size += 1U;

    return (RK_ERR_SUCCESS);
}

RK_ERR kListRemove(RK_LIST *const kobj, RK_NODE *const remNodePtr)
{
    K_ASSERT(kobj != NULL && remNodePtr != NULL);
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }
    KLISTNODEDEL(remNodePtr);
    kobj->size -= 1U;
    return (RK_ERR_SUCCESS);
}

RK_ERR kListRemoveHead(RK_LIST *const kobj,
                                     RK_NODE **const remNodePPtr)
{

    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE *currHeadPtr = kobj->listDummy.nextPtr;
    *remNodePPtr = currHeadPtr;
    KLISTNODEDEL(currHeadPtr);
    kobj->size -= 1U;
    return (RK_ERR_SUCCESS);
}

RK_ERR kListAddTail(RK_LIST *const kobj, RK_NODE *const newNodePtr)
{
    return (kListInsertAfter(kobj, kobj->listDummy.prevPtr, newNodePtr));
}

RK_ERR kListAddHead(RK_LIST *const kobj, RK_NODE *const newNodePtr)
{
    return (kListInsertAfter(kobj, &kobj->listDummy, newNodePtr));
}

RK_ERR kListRemoveTail(RK_LIST *const kobj, RK_NODE **remNodePPtr)
{
    if (kobj->size == 0)
    {
        return (RK_ERR_LIST_EMPTY);
    }

    RK_NODE *currTailPtr = kobj->listDummy.prevPtr;
    K_ASSERT(currTailPtr != NULL);
    *remNodePPtr = currTailPtr;
    KLISTNODEDEL(currTailPtr);
    kobj->size -= 1U;
    return (RK_ERR_SUCCESS);
}

/******************************************************************************/
/* TASK QUEUE MANAGEMENT                                                      */
/******************************************************************************/
RK_ERR kTCBQInit(RK_TCBQ *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

    K_ASSERT(kobj != NULL);

    RK_ERR err = kListInit(kobj);

    RK_CR_EXIT

    return (err);

}

RK_ERR kTCBQEnq(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
    RK_CR_AREA 
    RK_CR_ENTER

    K_ASSERT(kobj != NULL && tcbPtr != NULL);
    RK_ERR err = kListAddTail(kobj, &(tcbPtr->tcbNode));
    if (kobj == &RK_gReadyQueue[tcbPtr->priority])
        RK_gReadyBitmask |= 1 << tcbPtr->priority;
    
    RK_CR_EXIT 

    return (err);
}

RK_ERR kTCBQJam(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
    RK_CR_AREA
    RK_CR_ENTER 

    K_ASSERT(kobj != NULL && tcbPtr != NULL);
    RK_ERR err = kListAddHead(kobj, &(tcbPtr->tcbNode));
    if (kobj == &RK_gReadyQueue[tcbPtr->priority])
        RK_gReadyBitmask |= 1 << tcbPtr->priority;
    
    RK_CR_EXIT

    return (err);
}

RK_ERR kTCBQDeq(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
    RK_NODE *dequeuedNodePtr = NULL;
    RK_ERR err = kListRemoveHead(kobj, &dequeuedNodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(dequeuedNodePtr);
    K_ASSERT(*tcbPPtr != NULL);
    RK_TCB const *tcbPtr_ = *tcbPPtr;
    RK_PRIO prio_ = tcbPtr_->priority;
    if ((kobj == &RK_gReadyQueue[prio_]) && (kobj->size == 0))
        RK_gReadyBitmask &= ~(1U << prio_);
    return (err);
}

RK_ERR kTCBQRem(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    K_ASSERT(kobj != NULL);
    RK_NODE *dequeuedNodePtr = &((*tcbPPtr)->tcbNode);
    kListRemove(kobj, dequeuedNodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(dequeuedNodePtr);
    K_ASSERT(*tcbPPtr != NULL);
    RK_TCB const *tcbPtr_ = *tcbPPtr;
    RK_PRIO prio_ = tcbPtr_->priority;
    if ((kobj == &RK_gReadyQueue[prio_]) && (kobj->size == 0))
        RK_gReadyBitmask &= ~(1U << prio_);
    
    RK_CR_EXIT    
    return (RK_ERR_SUCCESS);
}

RK_TCB *kTCBQPeek(RK_TCBQ *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
    K_ASSERT(kobj != NULL);
    RK_NODE *nodePtr = kobj->listDummy.nextPtr;
    RK_TCB* retPtr = (K_GET_CONTAINER_ADDR(nodePtr, RK_TCB, tcbNode));
    K_ASSERT(retPtr != NULL);
    RK_CR_EXIT
    return (retPtr);
}

RK_ERR kTCBQEnqByPrio(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
    RK_CR_AREA
    RK_CR_ENTER
    K_ASSERT(kobj != NULL && tcbPtr != NULL);
    RK_NODE *currNodePtr = &(kobj->listDummy);

    while (currNodePtr->nextPtr != &(kobj->listDummy))
    {
        RK_TCB const *currTcbPtr = K_GET_TCB_ADDR(currNodePtr->nextPtr);
        if (currTcbPtr->priority > tcbPtr->priority)
        {
            break;
        }
        currNodePtr = currNodePtr->nextPtr;
    }

    RK_ERR err = kListInsertAfter(kobj, currNodePtr, &(tcbPtr->tcbNode));

    RK_CR_EXIT

    return (err);

}

VOID kSchedTask(RK_TCB *tcbPtr)
{
    if (RK_gRunPtr->priority > tcbPtr->priority && RK_gRunPtr->preempt == 1UL)
    {
        if (RK_gRunPtr->schLock == 0UL)
        {
            RK_PEND_CTXTSWTCH
        }
        else
        {
            RK_gPendingCtxtSwtch = 1;
        }
    }
}

RK_ERR kReadySwtch(RK_TCB *const tcbPtr)
{
    RK_CR_AREA
    RK_CR_ENTER
   
    K_ASSERT(tcbPtr != NULL);
    RK_ERR err = -1;
    if (tcbPtr->pid == RK_TIMHANDLER_ID)
    {
        err = kTCBQJam(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {
        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    K_ASSERT(err == RK_ERR_SUCCESS);
    tcbPtr->status = RK_READY;
    kSchedTask(tcbPtr);
   
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kReadyNoSwtch(RK_TCB *const tcbPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    K_ASSERT(tcbPtr != NULL);
    RK_ERR err = -1;
    if (tcbPtr->pid == RK_TIMHANDLER_ID)
    {
        err = kTCBQJam(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {
        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    K_ASSERT(err == RK_ERR_SUCCESS);
    tcbPtr->status = RK_READY;
    
    RK_CR_EXIT
    
    return (RK_ERR_SUCCESS);
}

/* fwded private helpers */
static inline VOID kPreemptRunningTask_(VOID);
static inline VOID kYieldRunningTask_(VOID);
static inline RK_PRIO kCalcNextTaskPrio_();

/******************************************************************************/
/* YIELD/CTXT SWTCH RUNNING TASK                                              */
/******************************************************************************/
VOID kYield(VOID) 
{
    RK_CR_AREA
    RK_CR_ENTER
    kYieldRunningTask_();
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT
}

/******************************************************************************/
/* TASK CONTROL BLOCK MANAGEMENT                                              */
/******************************************************************************/
static RK_PID pPid = 0; /** system pid for each task   */
static RK_ERR kInitStack_(UINT *const stackBufPtr, UINT const stackSize,
                          RK_TASKENTRY const taskFunc, VOID *argsPtr)
{

    if (stackBufPtr == NULL || stackSize == 0 || taskFunc == NULL)
    {
        return (RK_ERR_ERROR);
    }
    stackBufPtr[stackSize - PSR_OFFSET] = 0x01000000;
    stackBufPtr[stackSize - PC_OFFSET] = (UINT)taskFunc;
    stackBufPtr[stackSize - LR_OFFSET] = 0x14141414;
    stackBufPtr[stackSize - R12_OFFSET] = 0x12121212;
    stackBufPtr[stackSize - R3_OFFSET] = 0x03030303;
    stackBufPtr[stackSize - R2_OFFSET] = 0x02020202;
    stackBufPtr[stackSize - R1_OFFSET] = 0x01010101;
    stackBufPtr[stackSize - R0_OFFSET] = (UINT)(argsPtr);
    stackBufPtr[stackSize - R11_OFFSET] = 0x11111111;
    stackBufPtr[stackSize - R10_OFFSET] = 0x10101010;
    stackBufPtr[stackSize - R9_OFFSET] = 0x09090909;
    stackBufPtr[stackSize - R8_OFFSET] = 0x08080808;
    stackBufPtr[stackSize - R7_OFFSET] = 0x07070707;
    stackBufPtr[stackSize - R6_OFFSET] = 0x06060606;
    stackBufPtr[stackSize - R5_OFFSET] = 0x05050505;
    stackBufPtr[stackSize - R4_OFFSET] = 0x04040404;
    /*stack painting*/
    for (ULONG j = 17; j < stackSize; j++)
    {
        stackBufPtr[stackSize - j] = RK_STACK_PATTERN;
    }
    stackBufPtr[0] = RK_STACK_GUARD;
    return (RK_ERR_SUCCESS);
}

static RK_ERR kInitTcb_(RK_TASKENTRY const taskFunc, VOID *argsPtr,
                        UINT *const stackBufPtr, UINT const stackSize)
{
    if (kInitStack_(stackBufPtr, stackSize, taskFunc,
                    argsPtr) == RK_ERR_SUCCESS)
    {
        RK_gTcbs[pPid].stackBufPtr = stackBufPtr;
        RK_gTcbs[pPid].sp = &stackBufPtr[stackSize - R4_OFFSET];
        RK_gTcbs[pPid].stackSize = stackSize;
        RK_gTcbs[pPid].status = RK_READY;
        RK_gTcbs[pPid].pid = pPid;
        RK_gTcbs[pPid].savedLR = 0xFFFFFFFD;
        
#if (RK_CONF_MUTEX == ON)
        kListInit(&RK_gTcbs[pPid].ownedMutexList);
        RK_gTcbs[pPid].waitingForMutexPtr = NULL;
#endif
        return (RK_ERR_SUCCESS);
    }
    return (RK_ERR_ERROR);
}

RK_ERR kCreateTask(RK_TASK_HANDLE *taskHandlePtr,
                   const RK_TASKENTRY taskFunc, VOID *argsPtr,
                   CHAR *const taskName, RK_STACK *const stackBufPtr,
                   const UINT stackSize, const RK_PRIO priority,
                   const ULONG preempt)
{

    /* if private PID is 0, system tasks hasn't been started yet */
    if (pPid == 0)
    {
        /* initialise IDLE TASK */
        kInitTcb_(IdleTask, argsPtr, RK_gIdleStack, RK_CONF_IDLE_STACKSIZE);
        RK_gTcbs[pPid].priority = idleTaskPrio;
        RK_gTcbs[pPid].prioReal = idleTaskPrio;
        RK_MEMCPY(RK_gTcbs[pPid].taskName, "IdlTask", RK_OBJ_MAX_NAME_LEN);
        RK_gTcbs[pPid].preempt = RK_PREEMPT;
        RK_gTcbs[pPid].schLock = 0UL;
        RK_gIdleTaskHandle = &RK_gTcbs[pPid];
        pPid += 1;

        /* initialise TIMER HANDLER TASK */
        kInitTcb_(PostProcSysTask, argsPtr, RK_gPostProcStack,
                  RK_CONF_TIMHANDLER_STACKSIZE);
        RK_gTcbs[pPid].priority = 0;
        RK_gTcbs[pPid].prioReal = 0;
        RK_MEMCPY(RK_gTcbs[pPid].taskName, "SyTmrTsk", RK_OBJ_MAX_NAME_LEN);
        RK_gTcbs[pPid].preempt = RK_NO_PREEMPT;
        RK_gTcbs[pPid].schLock = 0UL;
        RK_gPostProcTaskHandle = &RK_gTcbs[pPid];
        pPid += 1;
    }
    /* initialise user tasks */
    if (kInitTcb_(taskFunc, argsPtr, stackBufPtr, stackSize) == RK_ERR_SUCCESS)
    {
        if (priority > idleTaskPrio)
        {
            kErrHandler(RK_FAULT_TASK_INVALID_PRIO);
        }
        RK_gTcbs[pPid].priority = priority;
        RK_gTcbs[pPid].prioReal = priority;
        RK_gTcbs[pPid].wakeTime = 0UL;
        RK_gTcbs[pPid].preempt = preempt;
        RK_gTcbs[pPid].schLock = 0UL;
        RK_MEMCPY(RK_gTcbs[pPid].taskName, taskName, RK_OBJ_MAX_NAME_LEN);

        *taskHandlePtr = &RK_gTcbs[pPid];
        pPid += 1;
        return (RK_ERR_SUCCESS);
    }

    return (RK_ERR_ERROR);
}

/******************************************************************************/
/* KERNEL INITIALISATION                                                      */
/******************************************************************************/
static VOID kInitRunTime_(VOID)
{
    RK_gRunTime.globalTick = 0UL;
    RK_gRunTime.nWraps = 0UL;
}
static RK_ERR kInitQueues_(VOID)
{
    RK_ERR err = 0;
    for (RK_PRIO prio = 0; prio < RK_NPRIO + 1; prio++)
    {
        err |= kTCBQInit(&RK_gReadyQueue[prio]);
    }
    K_ASSERT(err == 0);
    return (err);
}

VOID kInit(VOID)
{

    version = kGetVersion();
    if (version != RK_VALID_VERSION)
    {
        K_ERR_HANDLER(RK_FAULT_KERNEL_VERSION);
    }
    kInitQueues_();

    kApplicationInit();

    kInitRunTime_();
    highestPrio = RK_gTcbs[0].priority;

    for (ULONG i = 0; i < RK_NTHREADS; i++)
    {
        if (RK_gTcbs[i].priority < highestPrio)
        {
            highestPrio = RK_gTcbs[i].priority;
        }
    }

    if (pPid != RK_NTHREADS)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_COUNT_MISMATCH);
    }

    for (ULONG i = 0; i < RK_NTHREADS; i++)
    {
        kTCBQEnq(&RK_gReadyQueue[RK_gTcbs[i].priority], &RK_gTcbs[i]);
    }
    
    kTCBQDeq(&RK_gReadyQueue[highestPrio], &RK_gRunPtr);
    K_ASSERT(RK_gTcbs[RK_IDLETASK_ID].priority == lowestPrio + 1);
    RK_DSB
    RK_EN_IRQ
    RK_ISB
    /* calls low-level scheduler for start-up */
    RK_STUP
}

/******************************************************************************/
/* TASK SWITCHING LOGIC                                                       */
/******************************************************************************/
static inline RK_PRIO kCalcNextTaskPrio_()
{
    if (RK_gReadyBitmask == 0U)
    {
        return (idleTaskPrio);
    }
    RK_gReadyPos = RK_gReadyBitmask & -RK_gReadyBitmask;
    RK_PRIO prio = (RK_PRIO)(__getReadyPrio(RK_gReadyPos));

    return (prio);
}
VOID kSwtch(VOID)
{

    RK_TCB *nextRK_gRunPtr = NULL;
    
#ifndef NDEBUG
    RK_TCB *prevRK_gRunPtr = RK_gRunPtr;
#endif

    if (RK_gRunPtr->status == RK_RUNNING)
    {

        kPreemptRunningTask_();
    }
    nextTaskPrio = kCalcNextTaskPrio_(); /* get the next task priority */
    kTCBQDeq(&RK_gReadyQueue[nextTaskPrio], &nextRK_gRunPtr);
    K_ASSERT(nextRK_gRunPtr != NULL);
    RK_gRunPtr = nextRK_gRunPtr;
#ifndef NDEBUG
    if (nextRK_gRunPtr->pid != prevRK_gRunPtr->pid)
    {
        RK_gRunPtr->nPreempted += 1U;
        prevRK_gRunPtr->preemptedBy = RK_gRunPtr->pid;
    }
#endif
}

static inline VOID kPreemptRunningTask_(VOID)
{
    K_ASSERT(RK_gRunPtr->status == RK_RUNNING);
    kTCBQJam(&RK_gReadyQueue[RK_gRunPtr->priority], RK_gRunPtr);
    RK_gRunPtr->status = RK_READY;
}

static inline VOID kYieldRunningTask_(VOID)
{
    K_ASSERT(RK_gRunPtr->status == RK_RUNNING);
    kTCBQEnq(&RK_gReadyQueue[RK_gRunPtr->priority], RK_gRunPtr);
    RK_gRunPtr->status = RK_READY;
    
}
/******************************************************************************/
/* TICK MANAGEMENT                                                            */
/******************************************************************************/

/* called from SysTick_Handler */

volatile RK_TIMEOUT_NODE *RK_gTimeOutListHeadPtr = NULL;
volatile RK_TIMEOUT_NODE *RK_gTimerListHeadPtr = NULL;

UINT kTickHandler(VOID)
{
    UINT nonPreempt = RK_FALSE;
    UINT timeOutTask = RK_FALSE;
    UINT ret = RK_FALSE;

    RK_gRunTime.globalTick += 1UL;
    if (RK_gRunTime.globalTick == RK_TICK_TYPE_MAX)
    {
        RK_gRunTime.globalTick = 0UL;
        RK_gRunTime.nWraps += 1UL;
    }
    /* handle time out and sleeping list */
    /* the list is not empty, decrement only the head  */
    if (RK_gTimeOutListHeadPtr != NULL)
    {
        timeOutTask = kHandleTimeoutList();
    }
    if ((RK_gRunPtr->preempt == RK_NO_PREEMPT || RK_gRunPtr->schLock > 0UL) &&
        (RK_gRunPtr->status == RK_RUNNING) &&
        (RK_gRunPtr->pid != RK_IDLETASK_ID))
    {
        /* this flag toggles, short-circuiting the */
        /* return value  to RK_FALSE                  */
        nonPreempt = RK_TRUE;
    }

#if (RK_CONF_CALLOUT_TIMER == ON)
    if (RK_gTimerListHeadPtr != NULL)
    {
        RK_TIMER *headTimPtr = K_GET_CONTAINER_ADDR(RK_gTimerListHeadPtr, RK_TIMER,
                                                    timeoutNode);

        if (headTimPtr->phase > 0UL)
        {
            headTimPtr->phase--;
        }
        else
        {
            if (headTimPtr->timeoutNode.dtick > 0UL)
                headTimPtr->timeoutNode.dtick--;
        }
        if (RK_gTimerListHeadPtr->dtick == 0UL)
        {
            kTaskFlagsSet(RK_gPostProcTaskHandle, RK_POSTPROC_SIG_TIMER);
            timeOutTask = RK_TRUE;
        }
    }
#endif
    ret = ((!nonPreempt) &&
           ((RK_gRunPtr->status == RK_READY) || timeOutTask));

    return (ret);
}
