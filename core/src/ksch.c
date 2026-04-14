/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.18.0 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: HIGH-LEVEL SCHEDULER                                            */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ksch.h>
#include <kcoredefs.h>
#include <kmem.h>
#include <ksystasks.h>
#include <ktimer.h>

/* scheduler globals */
RK_TCBQ RK_gReadyQueue[RK_CONF_MIN_PRIO + RK_N_SYSTASKS];
RK_TCB *RK_gRunPtr;
RK_TCB RK_gTcbs[RK_NTHREADS];
RK_TASK_HANDLE RK_gPostProcTaskHandle;
RK_TASK_HANDLE RK_gIdleTaskHandle;
volatile struct RK_STRUCT_RUNTIME RK_gRunTime;
volatile ULONG RK_gReadyBitmask;
volatile ULONG RK_gReadyPos;
volatile UINT RK_gPendingCtxtSwtch = 0;
volatile UINT RK_gSchLock = 0;

/* local globals  */
static RK_PRIO highestPrio = 0;
static RK_PRIO const lowestPrio = RK_CONF_MIN_PRIO;
static volatile RK_PRIO nextTaskPrio = 0;
static RK_PRIO const idleTaskPrio = RK_CONF_MIN_PRIO + 1;
static RK_PID pPid = 0; /* number of active tasks */
static RK_BOOL RK_gTaskPoolInit = RK_FALSE;
static RK_BOOL RK_gSystemTasksInit = RK_FALSE;
static RK_MEM_PARTITION RK_gTaskPool;
static RK_MEM_PARTITION *RK_gTaskDynStackPartByPid[RK_NTHREADS];
static RK_TASK_HANDLE RK_gTaskHandleByPid[RK_NTHREADS];

/* compile-time assertions trick */
typedef char RK_TCB_SP_OFFSET_ASSERT[(offsetof(RK_TCB, sp) == 0U) ? 1 : -1];
typedef char
    RK_TCB_STATUS_OFFSET_ASSERT[(offsetof(RK_TCB, status) == 4U) ? 1 : -1];
typedef char
    RK_TCB_RUNCNT_OFFSET_ASSERT[(offsetof(RK_TCB, runCnt) == 8U) ? 1 : -1];
typedef char
    RK_TCB_SAVEDLR_OFFSET_ASSERT[(offsetof(RK_TCB, savedLR) == 12U) ? 1 : -1];
typedef char
    RK_TCB_STACKADDR_OFFSET_ASSERT[(offsetof(RK_TCB, stackBufPtr) == 16U) ? 1
                                                                          : -1];


/******************************************************************************/
/* SCHEDULER LOCK                                                             */
/******************************************************************************/
VOID kSchLock(VOID)
{
    if (RK_gRunPtr->preempt == 0UL)
    {
        return;
    }
    RK_CR_AREA
    RK_CR_ENTER
    RK_gSchLock++;
    RK_DSB
    RK_ISB
    RK_CR_EXIT
}

VOID kSchUnlock(VOID)
{
    if (RK_gSchLock == 0UL)
    {
        return;
    }
    RK_CR_AREA
    RK_CR_ENTER
    if ((--RK_gSchLock == 0U) && (RK_gPendingCtxtSwtch != 0U))
    {
        RK_gPendingCtxtSwtch = 0U;
        RK_DSB
        RK_PEND_CTXTSWTCH
        RK_ISB
    }
    RK_CR_EXIT
}

/******************************************************************************/
/* TASK QUEUE MANAGEMENT                                                      */
/******************************************************************************/
RK_ERR kTCBQInit(RK_TCBQ *const kobj)
{

    RK_ERR err = kListInit(kobj);

    return (err);
}

RK_ERR kTCBQEnq(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{

    RK_ERR err = kListAddTail(kobj, &(tcbPtr->tcbNode));
    if (kobj == &RK_gReadyQueue[tcbPtr->priority])
        RK_gReadyBitmask |= 1 << tcbPtr->priority;

    return (err);
}

RK_ERR kTCBQJam(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{

    RK_ERR err = kListAddHead(kobj, &(tcbPtr->tcbNode));
    if (kobj == &RK_gReadyQueue[tcbPtr->priority])
    {
        RK_gReadyBitmask |= 1 << tcbPtr->priority;
        RK_DMB
    }
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
    {
        RK_gReadyBitmask &= ~(1U << prio_);
        RK_DMB
    }
    return (err);
}

RK_ERR kTCBQRem(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr)
{
    RK_NODE *dequeuedNodePtr = &((*tcbPPtr)->tcbNode);
    kListRemove(kobj, dequeuedNodePtr);
    *tcbPPtr = K_GET_TCB_ADDR(dequeuedNodePtr);
    RK_TCB const *tcbPtr_ = *tcbPPtr;
    RK_PRIO prio_ = tcbPtr_->priority;
    if ((kobj == &RK_gReadyQueue[prio_]) && (kobj->size == 0))
    {
        RK_gReadyBitmask &= ~(1U << prio_);
        RK_DMB
    }
    return (RK_ERR_SUCCESS);
}

RK_TCB *kTCBQPeek(RK_TCBQ *const kobj)
{
    RK_NODE *nodePtr = kobj->listDummy.nextPtr;
    RK_TCB *retPtr = (K_GET_CONTAINER_ADDR(nodePtr, RK_TCB, tcbNode));
    return (retPtr);
}

RK_ERR kTCBQEnqByPrio(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
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

    return (err);
}

/* reeschedule a task based on current running priority, preemptibility and
scheduler lock state */
RK_ERR kReschedTask(RK_TCB *tcbPtr)
{

    if ((RK_gRunPtr->priority > tcbPtr->priority) && RK_gRunPtr->preempt == 1UL)
    {
        if (RK_gSchLock == 0UL)
        {
            RK_PEND_CTXTSWTCH
            return (RK_ERR_SUCCESS); /* RUNNING prio is lower*/
        }
        else
        {
            RK_gPendingCtxtSwtch = 1;
            RK_BARRIER
            return (RK_ERR_RESCHED_PENDING); /* RUNNING prio is lower but
                                                scheduler is locked */
        }
    }
    return (RK_ERR_RESCHED_NOT_NEEDED); /* RUNNING prio is higher */
}

RK_ERR kReadySwtch(RK_TCB *const tcbPtr)
{
    RK_ERR err = -1;
    if (tcbPtr->pid == RK_POSTPROC_TASK_ID)
    {
        err = kTCBQJam(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {
        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    if (err == RK_ERR_SUCCESS)
    {
        tcbPtr->status = RK_READY;
        return (kReschedTask(tcbPtr));
    }
    return (err);
}
/* ready a task without testing if switch is needed */
RK_ERR kReadyNoSwtch(RK_TCB *const tcbPtr)
{
    K_ASSERT(tcbPtr != NULL);
    RK_ERR err = -1;
    if (tcbPtr->pid == RK_POSTPROC_TASK_ID)
    {
        err = kTCBQJam(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    else
    {
        err = kTCBQEnq(&RK_gReadyQueue[tcbPtr->priority], tcbPtr);
    }
    K_ASSERT(err == RK_ERR_SUCCESS);
    tcbPtr->status = RK_READY;
    RK_DMB
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
    RK_CR_EXIT
}

/******************************************************************************/
/* TASK CONTROL BLOCK MANAGEMENT                                              */
/******************************************************************************/
static inline RK_ERR kTaskPoolInit_(ULONG const nTcbs);

static inline RK_ERR kTaskPoolEnsureInit_(ULONG const nTcbs)
{
    if (RK_gTaskPoolInit == RK_TRUE)
    {
        return (RK_ERR_SUCCESS);
    }
    return (kTaskPoolInit_(nTcbs));
}

static RK_ERR kTaskPoolInit_(ULONG const nTcbs)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (RK_gTaskPoolInit == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    if ((nTcbs < RK_N_SYSTASKS) || (nTcbs > RK_NTHREADS))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    RK_ERR err =
        kMemPartitionInit(&RK_gTaskPool, RK_gTcbs, sizeof(RK_TCB), nTcbs);
    if (err == RK_ERR_SUCCESS)
    {
        RK_gTaskPoolInit = RK_TRUE;
    }
    RK_CR_EXIT
    return (err);
}
/* initialises a task control block */
static RK_ERR kTaskInitTcb_(RK_TCB *const tcbPtr, RK_PID const pid,
                            RK_TASKENTRY const taskFunc, VOID *argsPtr,
                            RK_STACK *const stackBufPtr, ULONG const stackSize)
{
    if (kInitStack_(stackBufPtr, stackSize, taskFunc, argsPtr) !=
        RK_ERR_SUCCESS)
    {
        return (RK_ERR_ERROR);
    }

    RK_MEMSET(tcbPtr, 0, sizeof(RK_TCB));
    tcbPtr->stackBufPtr = stackBufPtr;
    tcbPtr->sp = &stackBufPtr[stackSize - R4_OFFSET];
    tcbPtr->stackSize = stackSize;
    tcbPtr->status = RK_READY;
    tcbPtr->pid = pid;
    tcbPtr->savedLR = 0xFFFFFFFDU;
    tcbPtr->wakeTime = 0UL;
    tcbPtr->overrunCount = 0UL;
    tcbPtr->mailPtr = NULL;
    tcbPtr->init = RK_TRUE;

#if (RK_CONF_MESG_QUEUE == ON)
    tcbPtr->queuePortPtr = NULL;
#endif
#if (RK_CONF_CHANNEL == ON)
    tcbPtr->serverChannelPtr = NULL;
#endif

#if (RK_CONF_MUTEX == ON)
    kListInit(&tcbPtr->ownedMutexList);
    tcbPtr->waitingForMutexPtr = NULL;
#endif

    return (RK_ERR_SUCCESS);
}
/* grabs a task control block from the task memory pool */
static RK_ERR
kTaskCreateFromPool_(RK_TASK_HANDLE *taskHandlePtr, RK_TASKENTRY const taskFunc,
                     VOID *argsPtr, CHAR *const taskName,
                     RK_STACK *const stackBufPtr, ULONG const stackSize,
                     RK_PRIO const priority, RK_OPTION const preempt)
{
    RK_TCB *newTcbPtr = (RK_TCB *)kMemPartitionAlloc(&RK_gTaskPool);
    if (newTcbPtr == NULL)
    {
        return (RK_ERR_TASK_POOL_EMPTY);
    }

    RK_PID pid = (RK_PID)(newTcbPtr - RK_gTcbs);
    if (pid >= RK_NTHREADS)
    {
        kMemPartitionFree(&RK_gTaskPool, newTcbPtr);
        return (RK_ERR_ERROR);
    }

    RK_ERR err = kTaskInitTcb_(newTcbPtr, pid, taskFunc, argsPtr, stackBufPtr,
                               stackSize);
    if (err != RK_ERR_SUCCESS)
    {
        kMemPartitionFree(&RK_gTaskPool, newTcbPtr);
        return (err);
    }

    newTcbPtr->priority = priority;
    newTcbPtr->prioNominal = priority;
    newTcbPtr->preempt = preempt;
    RK_MEMCPY(newTcbPtr->taskName, taskName, RK_OBJ_MAX_NAME_LEN);

    *taskHandlePtr = newTcbPtr;
    pPid += 1U;

    if (RK_gRunPtr != NULL)
    {
        RK_ERR readyErr = kReadySwtch(newTcbPtr);
        if (readyErr < 0)
        {
            RK_MEMSET(newTcbPtr, 0, sizeof(RK_TCB));
            kMemPartitionFree(&RK_gTaskPool, newTcbPtr);
            *taskHandlePtr = NULL;
            pPid -= 1U;
            return (readyErr);
        }
    }

    RK_gTaskHandleByPid[pid] = newTcbPtr;
    RK_gTaskDynStackPartByPid[pid] = NULL;

    return (RK_ERR_SUCCESS);
}
#if (RK_CONF_DYNAMIC_TASK == ON)
/* checks if a task can be terminated without affecting progress */
static RK_BOOL kTaskHasDependents_(RK_TCB const *taskPtr)
{
#if (RK_CONF_MESG_QUEUE == ON)
    if ((taskPtr->queuePortPtr != NULL) &&
        (taskPtr->queuePortPtr->ownerTask == taskPtr))
    {
        if ((taskPtr->queuePortPtr->waitingSenders.size > 0U) ||
            (taskPtr->queuePortPtr->waitingReceivers.size > 0U) ||
            (taskPtr->queuePortPtr->ringBuf.nFull > 0U))
        {
            return (RK_TRUE);
        }
    }
#endif

#if (RK_CONF_CHANNEL == ON)
    if ((taskPtr->serverChannelPtr != NULL) &&
        (taskPtr->serverChannelPtr->serverTask == taskPtr))
    {
        if ((taskPtr->serverChannelPtr->waitingRequesters.size > 0U) ||
            (taskPtr->serverChannelPtr->ringBuf.nFull > 0U))
        {
            return (RK_TRUE);
        }
    }
#endif

    return (RK_FALSE);
}
#endif
RK_ERR kTaskInit(RK_TASK_HANDLE *taskHandlePtr, const RK_TASKENTRY taskFunc,
                 VOID *argsPtr, CHAR *const taskName,
                 RK_STACK *const stackBufPtr, const ULONG stackSize,
                 const RK_PRIO priority, const RK_OPTION preempt)
{
    if ((taskHandlePtr == NULL) || (taskFunc == NULL) || (taskName == NULL) ||
        (stackBufPtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    if (kIsISR())
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_ISR_PRIMITIVE);
#endif
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((preempt != RK_PREEMPT) && (preempt != RK_NO_PREEMPT))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_INVALID_PARAM);
    }

    if (priority > idleTaskPrio)
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_TASK_INVALID_PRIO);
#endif
        return (RK_ERR_INVALID_PRIO);
    }

    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR err = kTaskPoolEnsureInit_(RK_NTHREADS);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (RK_gSystemTasksInit == RK_FALSE)
    {
        err = kTaskCreateFromPool_
        (
            &RK_gIdleTaskHandle, IdleTask, RK_NO_ARGS, "IdlTask", RK_gIdleStack,
            RK_CONF_IDLE_STACKSIZE, idleTaskPrio, RK_PREEMPT
        );
        if (err != RK_ERR_SUCCESS)
        {
            K_PANIC("Failed to create idle system task");
            RK_CR_EXIT
            return (err);
        }

        err = kTaskCreateFromPool_
        (
            &RK_gPostProcTaskHandle, PostProcSysTask, RK_NO_ARGS, "PostProc",
            RK_gPostProcStack, RK_CONF_POSTPROC_STACKSIZE, 0U, RK_NO_PREEMPT
        );
        if (err != RK_ERR_SUCCESS)
        {
            K_PANIC("Failed to create post-processing system task");
            RK_CR_EXIT

            return (err);
        }
        /* mark system tasks as initialised */
        RK_gSystemTasksInit = RK_TRUE;
    }

    err = kTaskCreateFromPool_(taskHandlePtr, taskFunc, argsPtr, taskName,
                               stackBufPtr, stackSize,
                               priority, preempt);
    RK_CR_EXIT
    return (err);
}

#if (RK_CONF_DYNAMIC_TASK == ON)
RK_ERR kTaskSpawn(RK_DYNAMIC_TASK_ATTR const *taskAttrPtr,
                  RK_TASK_HANDLE *taskHandlePtr)
{
    if ((taskAttrPtr == NULL) || (taskHandlePtr == NULL) ||
        (taskAttrPtr->taskFunc == NULL) || (taskAttrPtr->taskName == NULL) ||
        (taskAttrPtr->stackMemPtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    if (kIsISR())
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_ISR_PRIMITIVE);
#endif
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((taskAttrPtr->preempt != RK_PREEMPT) &&
        (taskAttrPtr->preempt != RK_NO_PREEMPT))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_INVALID_PARAM);
    }

    if (taskAttrPtr->priority > idleTaskPrio)
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_TASK_INVALID_PRIO);
#endif
        return (RK_ERR_INVALID_PRIO);
    }

    RK_MEM_PARTITION *stackMemPtr = taskAttrPtr->stackMemPtr;
    if ((stackMemPtr->objID != RK_MEMALLOC_KOBJ_ID) ||
        (stackMemPtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_OBJ);
#endif
        return (RK_ERR_INVALID_OBJ);
    }

    ULONG const stackSize = (stackMemPtr->blkSize / RK_WORD_SIZE);
    if ((stackSize < RK_MIN_STACKSIZE) || ((stackSize & 1U) != 0U))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_INVALID_PARAM);
    }

    RK_STACK *stackBufPtr = (RK_STACK *)kMemPartitionAlloc(stackMemPtr);
    if (stackBufPtr == NULL)
    {
        return (RK_ERR_TASK_POOL_EMPTY);
    }

    RK_BOOL locked = RK_FALSE;
    if ((RK_gRunPtr != NULL) && (RK_gRunPtr->preempt != RK_NO_PREEMPT))
    {
        kSchLock();
        locked = RK_TRUE;
    }

    *taskHandlePtr = NULL;
    RK_ERR err = kTaskInit(taskHandlePtr,
                           taskAttrPtr->taskFunc,
                           taskAttrPtr->argsPtr,
                           taskAttrPtr->taskName,
                           stackBufPtr,
                           stackSize,
                           taskAttrPtr->priority,
                           taskAttrPtr->preempt);

    if (err == RK_ERR_SUCCESS)
    {
        RK_PID pid = (*taskHandlePtr)->pid;
        if (pid >= RK_NTHREADS)
        {
            err = RK_ERR_ERROR;
        }
        else
        {
            RK_gTaskDynStackPartByPid[pid] = stackMemPtr;
        }
    }

    if (locked == RK_TRUE)
    {
        kSchUnlock();
    }

    if (err != RK_ERR_SUCCESS)
    {
        kMemPartitionFree(stackMemPtr, stackBufPtr);
    }
    return (err);
}


RK_ERR kTaskTerminateSelf(VOID)
{
    if (kIsISR())
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_ISR_PRIMITIVE);
#endif
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((RK_gRunPtr == NULL) || (RK_gRunPtr->pid <= RK_POSTPROC_TASK_ID))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_OBJ);
#endif
        return (RK_ERR_INVALID_OBJ);
    }

    return (kTaskTerminate(&RK_gTaskHandleByPid[RK_gRunPtr->pid]));
}

RK_ERR kTaskTerminate(RK_TASK_HANDLE *taskHandlePtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if ((taskHandlePtr == NULL) || (*taskHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_OBJ_NULL);
#endif
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (RK_gTaskPoolInit == RK_FALSE)
    {
        K_PANIC("Task pool not initialised");
        kErrHandler(RK_FAULT_TASK_POOL_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_TASK_POOL_NOT_INIT);
    }

    if (kIsISR())
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_ISR_PRIMITIVE);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    RK_TCB *taskPtr = *taskHandlePtr;
    if (taskPtr->init != RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (taskPtr->pid <= RK_POSTPROC_TASK_ID)
    {
#if (RK_CONF_ERR_CHECK == ON)
        kErrHandler(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

#if (RK_CONF_MUTEX == ON)
    if ((taskPtr->ownedMutexList.size != 0U) ||
        (taskPtr->waitingForMutexPtr != NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }
#endif

    if (kTaskHasDependents_(taskPtr) == RK_TRUE)
    {
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    /* if task is running, defer termination to the post-processing sys task */
    if (taskPtr->status == RK_RUNNING)
    {
        if (taskPtr != RK_gRunPtr)
        {
            RK_CR_EXIT
            return (RK_ERR_TASK_INVALID_ST);
        }

        RK_TASK_HANDLE *slotHandlePtr = &RK_gTaskHandleByPid[taskPtr->pid];
        RK_ERR deferErr = kPostProcJobEnq(RK_POSTPROC_JOB_TASK_TERMINATE,
                                          (VOID *)slotHandlePtr, 0U);
        if (deferErr != RK_ERR_SUCCESS)
        {

            K_PANIC("Failed to defer task termination to\
                post-processing system task");

            RK_CR_EXIT

            return (deferErr);
        }

        if (taskHandlePtr != slotHandlePtr)
        {
            *taskHandlePtr = NULL;
        }

        taskPtr->status = RK_PENDING; /* pending deferred termination */
        RK_PEND_CTXTSWTCH
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    switch (taskPtr->status)
    {
        case RK_READY:
            if ((taskPtr->tcbNode.nextPtr != NULL) &&
                (taskPtr->tcbNode.prevPtr != NULL))
            {
                RK_TCB *remPtr = taskPtr;
                kTCBQRem(&RK_gReadyQueue[taskPtr->priority], &remPtr);
            }
            break;

        case RK_SLEEPING_DELAY:
        case RK_SLEEPING_RELEASE:
        case RK_SLEEPING_UNTIL:
        case RK_SLEEPING_EV_FLAG:
        case RK_RECEIVING_TMAILBOX:
            if (kTimeoutNodeIsArmed(&taskPtr->timeoutNode) == RK_TRUE)
            {
                RK_ERR err = kRemoveTimeoutNode(&taskPtr->timeoutNode);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_CR_EXIT
                    return (err);
                }
            }
            break;

        case RK_PENDING: /* deferred self-termination path */
            break;

        default:
            RK_CR_EXIT
            return (RK_ERR_TASK_INVALID_ST);
    }

#if (RK_CONF_MESG_QUEUE == ON)
    if ((taskPtr->queuePortPtr != NULL) &&
        (taskPtr->queuePortPtr->ownerTask == taskPtr))
    {
        taskPtr->queuePortPtr->ownerTask = NULL;
    }
    taskPtr->queuePortPtr = NULL;
#endif

#if (RK_CONF_CHANNEL == ON)
    if ((taskPtr->serverChannelPtr != NULL) &&
        (taskPtr->serverChannelPtr->serverTask == taskPtr))
    {
        taskPtr->serverChannelPtr->serverTask = NULL;
    }
    taskPtr->serverChannelPtr = NULL;
#endif

    RK_PID const slotPid = taskPtr->pid;
    RK_STACK *stackBufPtr = NULL;
    RK_MEM_PARTITION *stackMemPtr = NULL;
    if (slotPid < RK_NTHREADS)
    {
        stackMemPtr = RK_gTaskDynStackPartByPid[slotPid];
        if (stackMemPtr != NULL)
        {
            stackBufPtr = taskPtr->stackBufPtr;
        }
    }

    RK_MEMSET(taskPtr, 0, sizeof(RK_TCB));
    RK_ERR err = kMemPartitionFree(&RK_gTaskPool, taskPtr);
    if (err == RK_ERR_SUCCESS)
    {
        if ((stackBufPtr != NULL) && (stackMemPtr != NULL))
        {
            RK_ERR stackErr = kMemPartitionFree(stackMemPtr, stackBufPtr);
            if (stackErr != RK_ERR_SUCCESS)
            {
                K_PANIC("Failed to free task spawn stack block");
            }
        }

        RK_gTaskDynStackPartByPid[slotPid] = NULL;
        RK_gTaskHandleByPid[slotPid] = NULL;
        taskPtr->status = RK_TASK_TERMINATED;
        taskPtr->pid = slotPid;
        taskPtr->init = RK_FALSE;
        if (pPid > 0U)
        {
            pPid -= 1U;
        }
        *taskHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
}
#endif

RK_TASK_HANDLE kTaskGetRunningHandle(VOID)
{
    return (RK_gRunPtr);
}

RK_PID kTaskGetPID(RK_TASK_HANDLE taskHandle)
{
    return (taskHandle->pid);
}

const CHAR *kTaskGetRunningName(VOID)
{
    return (kTaskGetRunningHandle()->taskName);
}

RK_ERR kTaskGetName(RK_TASK_HANDLE taskHandle, CHAR *buf)
{
    if (buf == NULL || taskHandle == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }

    UINT i = 0;
    CHAR *name = &taskHandle->taskName[0];
    while (*name != '\0')
    {
        *buf++ = *name++;
        i++;
        if (i >= RK_OBJ_MAX_NAME_LEN)
        {
            break;
        }
    }
    return (RK_ERR_SUCCESS);
}

RK_PRIO kTaskGetPrio(RK_TASK_HANDLE taskHandle)
{
    return (taskHandle->priority);
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
    RK_gReadyBitmask = 0U;
    RK_gReadyPos = 0U;
    for (ULONG i = 0; i < (RK_CONF_MIN_PRIO + RK_N_SYSTASKS); i++)
    {
        RK_ERR err = kTCBQInit(&RK_gReadyQueue[i]);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
    }
    return (RK_ERR_SUCCESS);
}

VOID kInit(VOID)
{

    if (!kIsValidVersion())
    {
        K_PANIC("ERROR: INVALID KERNEL VERSION");
    }

    if (kInitQueues_() != RK_ERR_SUCCESS)
    {
        K_PANIC("FAILED TO INITIALISE READY QUEUES");
    }

    if (kTaskPoolEnsureInit_(RK_NTHREADS) != RK_ERR_SUCCESS)
    {
        K_PANIC("FAILED TO INITIALISE TASK POOL");
    }

    kApplicationInit();

    RK_DSB

    if ((RK_gSystemTasksInit == RK_FALSE) || (RK_gIdleTaskHandle == NULL) ||
        (RK_gPostProcTaskHandle == NULL))
    {
        K_PANIC("SYSTEM TASKS NOT INITIALISED");
    }

    RK_ISB

    kInitRunTime_();
    highestPrio = idleTaskPrio;

    for (ULONG i = 0; i < RK_NTHREADS; i++)
    {
        if ((RK_gTcbs[i].init == RK_TRUE) && (RK_gTcbs[i].status == RK_READY) &&
            (RK_gTcbs[i].priority < highestPrio))
        {
            highestPrio = RK_gTcbs[i].priority;
        }
    }

    for (ULONG i = 0; i < RK_NTHREADS; i++)
    {
        if ((RK_gTcbs[i].init == RK_TRUE) && (RK_gTcbs[i].status == RK_READY))
        {
            kTCBQEnq(&RK_gReadyQueue[RK_gTcbs[i].priority], &RK_gTcbs[i]);
        }
    }

    kTCBQDeq(&RK_gReadyQueue[highestPrio], &RK_gRunPtr);
    if (RK_gTcbs[RK_IDLETASK_ID].priority != lowestPrio + 1)
    {
        K_PANIC("INVALID PRIORITY: %s : %d\r\n", RK_gRunPtr->taskName,
                RK_gRunPtr->priority);
    }
    if (RK_gSysTickInterval == 0)
    {
        K_PANIC("INVALID SYSTICK INTERVAL: %ld\r\n", RK_gSysTickInterval);
    }
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
    volatile RK_PRIO prio = (RK_PRIO)(__getReadyPrio(RK_gReadyPos));
    return (prio);
}

VOID kSwtch(VOID)
{
    RK_TCB *nextRK_gRunPtr = NULL;

    if (RK_gRunPtr->status == RK_RUNNING)
    {
        kPreemptRunningTask_();
    }
    nextTaskPrio = kCalcNextTaskPrio_(); /* get the next task priority */

    kTCBQDeq(&RK_gReadyQueue[nextTaskPrio], &nextRK_gRunPtr);

    if (nextRK_gRunPtr == NULL)
    {
        K_PANIC("NULL READY TASK POINTER\r\n");
    }

    RK_gRunPtr = nextRK_gRunPtr;
}
static inline VOID kPreemptRunningTask_(VOID)
{
    kTCBQJam(&RK_gReadyQueue[RK_gRunPtr->priority], RK_gRunPtr);
    RK_BARRIER
    RK_gRunPtr->status = RK_READY;
}

static inline VOID kYieldRunningTask_(VOID)
{
    if (kCalcNextTaskPrio_() <= RK_gRunPtr->priority)
    {
        kTCBQEnq(&RK_gReadyQueue[RK_gRunPtr->priority], RK_gRunPtr);
        RK_gRunPtr->status = RK_READY;
        if (RK_gSchLock == 0U)
        {
            RK_PEND_CTXTSWTCH
        }
        else
        {
            RK_gPendingCtxtSwtch = 1U;
            RK_BARRIER
        }
    }
}
/******************************************************************************/
/* TICK MANAGEMENT                                                            */
/******************************************************************************/

/* called from SysTick_Handler */

volatile RK_TIMEOUT_NODE *RK_gTimeOutListHeadPtr = NULL;
volatile RK_TIMEOUT_NODE *RK_gTimerListHeadPtr = NULL;

UINT kTickHandler(VOID)
{
    volatile UINT nonPreempt = RK_FALSE;
    volatile UINT timeOutTask = RK_FALSE;
    RK_CR_AREA
    RK_CR_ENTER
    RK_gRunTime.globalTick += 1UL;
    if (RK_gRunTime.globalTick == RK_TICK_TYPE_MAX)
    {
        RK_gRunTime.globalTick = 0UL;
        RK_gRunTime.nWraps += 1UL;
    }
    RK_CR_EXIT
    /* handle time out and sleeping list */
    /* the list is not empty, decrement only the head  */
    if (RK_gTimeOutListHeadPtr != NULL)
    {
        RK_CR_ENTER

        timeOutTask = kHandleTimeoutList();

        RK_CR_EXIT
    }

    if ((RK_gRunPtr->preempt == RK_NO_PREEMPT || RK_gSchLock > 0UL) &&
        (RK_gRunPtr->status == RK_RUNNING))
    {
        /* this flag toggles, short-circuiting the */
        /* return value  to RK_FALSE                  */
        nonPreempt = RK_TRUE;
    }
#if (RK_CONF_CALLOUT_TIMER == ON)
    if (RK_gTimerListHeadPtr != NULL)
    {
        RK_CR_ENTER

        volatile RK_TIMER *headTimPtr =
            K_GET_CONTAINER_ADDR(RK_gTimerListHeadPtr, RK_TIMER, timeoutNode);

        if (headTimPtr->phase > 0UL)
        {
            --headTimPtr->phase;
        }
        else
        {
            if (headTimPtr->timeoutNode.dtick > 0UL)
                --headTimPtr->timeoutNode.dtick;
        }

        if (RK_gTimerListHeadPtr->dtick == 0UL)
        {
            kEventSet(RK_gPostProcTaskHandle, RK_POSTPROC_TIMER_SIG);
            timeOutTask = RK_TRUE;
        }

        RK_CR_EXIT
    }
#endif
    return ((!nonPreempt && (RK_gRunPtr->status == RK_READY)) || timeOutTask);
}
