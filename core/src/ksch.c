/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.4
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
 * 	Provides to      :  APPLICATION
 *  Depends  on      :  LOW-LEVEL SCHEDULER
 *  Public API 		 :  YES
 *
 ******************************************************************************/

#define RK_CODE
#include <kenv.h>
#include <kservices.h>
#include <kstring.h>

/* scheduler globals */

RK_TCBQ readyQueue[RK_CONF_MIN_PRIO + 2];
RK_TCB *runPtr;
RK_TCB tcbs[RK_NTHREADS];
RK_TASK_HANDLE timTaskHandle;
RK_TASK_HANDLE idleTaskHandle;
volatile struct kRunTime runTime;
/* local globals  */
static RK_PRIO highestPrio = 0;
static RK_PRIO const lowestPrio = RK_CONF_MIN_PRIO;
static RK_PRIO nextTaskPrio = 0;
static RK_PRIO const idleTaskPrio = RK_CONF_MIN_PRIO + 1;
ULONG readyQBitMask;
ULONG readyQRightMask;
volatile UINT isPendingCtxtSwtch = 0;
static ULONG version;
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
static RK_ERR kInitStack_(UINT *const stackPtr, UINT const stackSize,
                          RK_TASKENTRY const taskFunc, VOID *argsPtr)
{

    if (stackPtr == NULL || stackSize == 0 || taskFunc == NULL)
    {
        return (RK_ERROR);
    }
    stackPtr[stackSize - PSR_OFFSET] = 0x01000000;
    stackPtr[stackSize - PC_OFFSET] = (UINT)taskFunc;
    stackPtr[stackSize - LR_OFFSET] = 0x14141414;
    stackPtr[stackSize - R12_OFFSET] = 0x12121212;
    stackPtr[stackSize - R3_OFFSET] = 0x03030303;
    stackPtr[stackSize - R2_OFFSET] = 0x02020202;
    stackPtr[stackSize - R1_OFFSET] = 0x01010101;
    stackPtr[stackSize - R0_OFFSET] = (UINT)(argsPtr);
    stackPtr[stackSize - R11_OFFSET] = 0x11111111;
    stackPtr[stackSize - R10_OFFSET] = 0x10101010;
    stackPtr[stackSize - R9_OFFSET] = 0x09090909;
    stackPtr[stackSize - R8_OFFSET] = 0x08080808;
    stackPtr[stackSize - R7_OFFSET] = 0x07070707;
    stackPtr[stackSize - R6_OFFSET] = 0x06060606;
    stackPtr[stackSize - R5_OFFSET] = 0x05050505;
    stackPtr[stackSize - R4_OFFSET] = 0x04040404;
    /*stack painting*/
    for (ULONG j = 17; j < stackSize; j++)
    {
        stackPtr[stackSize - j] = RK_STACK_PATTERN;
    }
    stackPtr[0] = RK_STACK_GUARD;
    return (RK_SUCCESS);
}

static RK_ERR kInitTcb_(RK_TASKENTRY const taskFunc, VOID *argsPtr,
                        UINT *const stackPtr, UINT const stackSize)
{
    if (kInitStack_(stackPtr, stackSize, taskFunc,
                    argsPtr) == RK_SUCCESS)
    {
        tcbs[pPid].stackPtr = stackPtr;
        tcbs[pPid].sp = &stackPtr[stackSize - R4_OFFSET];
        tcbs[pPid].stackSize = stackSize;
        tcbs[pPid].status = RK_READY;
        tcbs[pPid].pid = pPid;
        tcbs[pPid].savedLR = 0xFFFFFFFD;
#if (RK_CONF_MUTEX == ON)
        kListInit(&tcbs[pPid].ownedMutexList);
        tcbs[pPid].waitingForMutexPtr = NULL;
#endif
        return (RK_SUCCESS);
    }
    return (RK_ERROR);
}

RK_ERR kCreateTask(RK_TASK_HANDLE *taskHandlePtr,
                   const RK_TASKENTRY taskFunc, VOID *argsPtr,
                   CHAR *const taskName, RK_STACK *const stackPtr,
                   const UINT stackSize, const RK_PRIO priority,
                   const ULONG preempt)
{

    /* if private PID is 0, system tasks hasn't been started yet */
    if (pPid == 0)
    {
        /* initialise IDLE TASK */
        kInitTcb_(IdleTask, argsPtr, idleStack, RK_CONF_IDLE_STACKSIZE);
        tcbs[pPid].priority = idleTaskPrio;
        tcbs[pPid].prioReal = idleTaskPrio;
        RK_MEMCPY(tcbs[pPid].taskName, "IdlTask", RK_OBJ_MAX_NAME_LEN);
        tcbs[pPid].preempt = RK_PREEMPT;
        tcbs[pPid].schLock = 0UL;
        idleTaskHandle = &tcbs[pPid];
        pPid += 1;

        /* initialise TIMER HANDLER TASK */
        kInitTcb_(TimerHandlerTask, argsPtr, timerHandlerStack,
                  RK_CONF_TIMHANDLER_STACKSIZE);
        tcbs[pPid].priority = 0;
        tcbs[pPid].prioReal = 0;
        RK_MEMCPY(tcbs[pPid].taskName, "SyTmrTsk", RK_OBJ_MAX_NAME_LEN);
        tcbs[pPid].preempt = RK_NO_PREEMPT;
        tcbs[pPid].schLock = 0UL;
        timTaskHandle = &tcbs[pPid];
        pPid += 1;
    }
    /* initialise user tasks */
    if (kInitTcb_(taskFunc, argsPtr, stackPtr, stackSize) == RK_SUCCESS)
    {
        if (priority > idleTaskPrio)
        {
            kErrHandler(RK_FAULT_TASK_INVALID_PRIO);
        }
        tcbs[pPid].priority = priority;
        tcbs[pPid].prioReal = priority;
        tcbs[pPid].wakeTime = 0UL;
        tcbs[pPid].preempt = preempt;
        tcbs[pPid].schLock = 0UL;
        RK_MEMCPY(tcbs[pPid].taskName, taskName, RK_OBJ_MAX_NAME_LEN);

        *taskHandlePtr = &tcbs[pPid];
        pPid += 1;
        return (RK_SUCCESS);
    }

    return (RK_ERROR);
}

/******************************************************************************/
/* KERNEL INITIALISATION                                                      */
/******************************************************************************/
static VOID kInitRunTime_(VOID)
{
    runTime.globalTick = 0UL;
    runTime.nWraps = 0UL;
}
static RK_ERR kInitQueues_(VOID)
{
    RK_ERR err = 0;
    for (RK_PRIO prio = 0; prio < RK_NPRIO + 1; prio++)
    {
        err |= kTCBQInit(&readyQueue[prio]);
    }
    kassert(err == 0);
    return (err);
}

volatile ULONG nTcbs = 0;
volatile ULONG tcbSize = 0;

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
    highestPrio = tcbs[0].priority;

    for (ULONG i = 0; i < RK_NTHREADS; i++)
    {
        if (tcbs[i].priority < highestPrio)
        {
            highestPrio = tcbs[i].priority;
        }
    }

    if (pPid != RK_NTHREADS)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_COUNT_MISMATCH);
    }

    for (ULONG i = 0; i < RK_NTHREADS; i++)
    {
        kTCBQEnq(&readyQueue[tcbs[i].priority], &tcbs[i]);
    }
    
    kTCBQDeq(&readyQueue[highestPrio], &runPtr);
    kassert(tcbs[RK_IDLETASK_ID].priority == lowestPrio + 1);
    _RK_DSB
    __ASM volatile("cpsie i" : : : "memory");
    _RK_ISB
    /* calls low-level scheduler for start-up */
    _RK_STUP
}

/******************************************************************************/
/* TASK SWITCHING LOGIC                                                       */
/******************************************************************************/
static inline RK_PRIO kCalcNextTaskPrio_()
{
    if (readyQBitMask == 0U)
    {
        return (idleTaskPrio);
    }
    readyQRightMask = readyQBitMask & -readyQBitMask;
    RK_PRIO prio = (RK_PRIO)(__getReadyPrio(readyQRightMask));

    return (prio);
}
VOID kSchSwtch(VOID)
{

    RK_TCB *nextRunPtr = NULL;
    
#ifndef NDEBUG
    RK_TCB *prevRunPtr = runPtr;
#endif

    if (runPtr->status == RK_RUNNING)
    {

        kPreemptRunningTask_();
    }
    nextTaskPrio = kCalcNextTaskPrio_(); /* get the next task priority */
    kTCBQDeq(&readyQueue[nextTaskPrio], &nextRunPtr);
    if (nextRunPtr == NULL)
    {
        kErrHandler(RK_FAULT_OBJ_NULL);
        return; /* suppress static analyser warning */
    }
    runPtr = nextRunPtr;
#ifndef NDEBUG
    if (nextRunPtr->pid != prevRunPtr->pid)
    {
        runPtr->nPreempted += 1U;
        prevRunPtr->preemptedBy = runPtr->pid;
    }
#endif
}

static inline VOID kPreemptRunningTask_(VOID)
{
    kassert(runPtr->status == RK_RUNNING);
    kTCBQJam(&readyQueue[runPtr->priority], runPtr);
    runPtr->status = RK_READY;
}

static inline VOID kYieldRunningTask_(VOID)
{
    kassert(runPtr->status == RK_RUNNING);
    kTCBQEnq(&readyQueue[runPtr->priority], runPtr);
    runPtr->status = RK_READY;
    
}
/******************************************************************************/
/* TICK MANAGEMENT                                                            */
/******************************************************************************/
volatile RK_TIMEOUT_NODE *timeOutListHeadPtr = NULL;
volatile RK_TIMEOUT_NODE *timerListHeadPtr = NULL;
volatile int counter = 0;

BOOL kTickHandler(VOID)
{
    BOOL nonPreempt = FALSE;
    BOOL timeOutTask = FALSE;
    BOOL ret = FALSE;

    runTime.globalTick += 1UL;
    if (runTime.globalTick == RK_TICK_TYPE_MAX)
    {
        runTime.globalTick = 0UL;
        runTime.nWraps += 1UL;
    }
    /* handle time out and sleeping list */
    /* the list is not empty, decrement only the head  */
    if (timeOutListHeadPtr != NULL)
    {
        timeOutTask = kHandleTimeoutList();
    }
    if ((runPtr->preempt == RK_NO_PREEMPT || runPtr->schLock > 0UL) &&
        (runPtr->status == RK_RUNNING) &&
        (runPtr->pid != RK_IDLETASK_ID))
    {
        /* this flag toggles, short-circuiting the */
        /* return value  to FALSE                  */
        nonPreempt = TRUE;
    }

#if (RK_CONF_CALLOUT_TIMER == ON)
    if (timerListHeadPtr != NULL)
    {
        RK_TIMER *headTimPtr = K_GET_CONTAINER_ADDR(timerListHeadPtr, RK_TIMER,
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
        if (timerListHeadPtr->dtick == 0UL)
        {
            kSignalSet(timTaskHandle, RK_SIG_TIMER);
            timeOutTask = TRUE;
        }
    }
#endif
    ret = ((!nonPreempt) &&
           ((runPtr->status == RK_READY) || timeOutTask));

    return (ret);
}
