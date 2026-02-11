/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.14                                              */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/*******************************************************************************
 * 	COMPONENT        : SYSTEM TASKS
 *  DEPENDS ON       : TASK EVENT FLAGS, TIMERS
 *  PUBLIC API   	 : YES
 ******************************************************************************/

#define RK_SOURCE_CODE
#include <ksystasks.h>
#include <kerr.h>

UINT RK_gIdleStack[RK_CONF_IDLE_STACKSIZE] K_ALIGN(8);
UINT RK_gPostProcStack[RK_CONF_TIMHANDLER_STACKSIZE] K_ALIGN(8);

typedef struct RK_PENDSV_JOB_ENTRY
{
    UINT jobType;
    VOID *objPtr;
    UINT nTasks;
} RK_PENDSV_JOB_ENTRY;

static RK_PENDSV_JOB_ENTRY RK_gPendSVQ[RK_PENDSV_DEFERQ_LEN];
static volatile UINT RK_gPendSVHead = 0U;
static volatile UINT RK_gPendSVTail = 0U;
static volatile UINT RK_gPendSVCount = 0U;

static RK_ERR kPendSVJobDeq_(RK_PENDSV_JOB_ENTRY *const jobPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if ((jobPtr == NULL) || (RK_gPendSVCount == 0U))
    {
        RK_CR_EXIT
        return (RK_ERR_LIST_EMPTY);
    }

    *jobPtr = RK_gPendSVQ[RK_gPendSVHead];
    RK_gPendSVHead = (RK_gPendSVHead + 1U) % RK_PENDSV_DEFERQ_LEN;
    RK_gPendSVCount -= 1U;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#if (RK_CONF_SEMAPHORE == ON)
static RK_ERR kSemaphoreFlushDeferred_(RK_SEMAPHORE *const kobj, UINT toWake)
{
    if (kobj == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }

    UINT nWaiting = (UINT)kobj->waitingQueue.size;
    if (nWaiting == 0U)
    {
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    if ((toWake == 0U) || (toWake > nWaiting))
    {
        toWake = nWaiting;
    }

    for (UINT i = 0U; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        RK_ERR err = kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadyNoSwtch(nextTCBPtr);
    }

    kobj->value = 0U;
    return (RK_ERR_SUCCESS);
}
#endif

#if (RK_CONF_SLEEP_QUEUE == ON)
static RK_ERR kSleepQueueWakeDeferred_(RK_SLEEP_QUEUE *const kobj, UINT toWake)
{
    if (kobj == NULL)
    {
        return (RK_ERR_OBJ_NULL);
    }

    UINT nWaiting = (UINT)kobj->waitingQueue.size;
    if (nWaiting == 0U)
    {
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    if ((toWake == 0U) || (toWake > nWaiting))
    {
        toWake = nWaiting;
    }

    for (UINT i = 0U; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        RK_ERR err = kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadyNoSwtch(nextTCBPtr);
    }

    return (RK_ERR_SUCCESS);
}
#endif

RK_ERR kPendSVJobEnq(UINT jobType, VOID *const objPtr, UINT nTasks)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (objPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    UINT validType = RK_FALSE;
#if (RK_CONF_SEMAPHORE == ON)
    if (jobType == RK_PENDSV_JOB_SEMA_FLUSH)
    {
        validType = RK_TRUE;
    }
#endif
#if (RK_CONF_SLEEP_QUEUE == ON)
    if (jobType == RK_PENDSV_JOB_SLEEPQ_WAKE)
    {
        validType = RK_TRUE;
    }
#endif
    if (validType == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_CR_AREA
    RK_CR_ENTER

    if (RK_gPendSVCount >= RK_PENDSV_DEFERQ_LEN)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gPendSVQ[RK_gPendSVTail].jobType = jobType;
    RK_gPendSVQ[RK_gPendSVTail].objPtr = objPtr;
    RK_gPendSVQ[RK_gPendSVTail].nTasks = nTasks;

    RK_gPendSVTail = (RK_gPendSVTail + 1U) % RK_PENDSV_DEFERQ_LEN;
    RK_gPendSVCount += 1U;

    RK_CR_EXIT
    RK_PEND_CTXTSWTCH
    return (RK_ERR_SUCCESS);
}

VOID kPendSVRunDeferred(VOID)
{
    UINT budget = RK_PENDSV_DEFER_BUDGET;

    while (budget > 0U)
    {
        RK_PENDSV_JOB_ENTRY job;
        if (kPendSVJobDeq_(&job) != RK_ERR_SUCCESS)
        {
            break;
        }

        switch (job.jobType)
        {
#if (RK_CONF_SEMAPHORE == ON)
            case RK_PENDSV_JOB_SEMA_FLUSH:
                kSemaphoreFlushDeferred_((RK_SEMAPHORE *)job.objPtr, job.nTasks);
                break;
#endif
#if (RK_CONF_SLEEP_QUEUE == ON)
            case RK_PENDSV_JOB_SLEEPQ_WAKE:
                kSleepQueueWakeDeferred_((RK_SLEEP_QUEUE *)job.objPtr, job.nTasks);
                break;
#endif
            default:
                break;
        }

        budget -= 1U;
    }

    RK_CR_AREA
    RK_CR_ENTER
    UINT pending = (RK_gPendSVCount > 0U);
    RK_CR_EXIT
    if (pending)
    {
        RK_PEND_CTXTSWTCH
    }
}


VOID IdleTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_ISB

        RK_WFI 

        RK_DSB
    }
}

#define TIMER_SIGNAL_RANGE 0x2
VOID TimHandlerSysTask(VOID *args)
{
    RK_UNUSEARGS

    RK_REG_SYSTICK_CTRL |= 0x01;
    
    while (1)
    {
        ULONG gotFlags = 0;

        kTaskEventGet(RK_TIMHANDLE_SIG, RK_EVENT_FLAGS_ALL, &gotFlags, RK_WAIT_FOREVER);

#if (RK_CONF_CALLOUT_TIMER == ON)
        if (gotFlags == RK_TIMHANDLE_SIG)
        {
            while (RK_gTimerListHeadPtr != NULL && RK_gTimerListHeadPtr->dtick == 0)
            {
                RK_TIMEOUT_NODE *node = (RK_TIMEOUT_NODE *)RK_gTimerListHeadPtr;
                RK_gTimerListHeadPtr = node->nextPtr;
                kRemoveTimerNode(node);

                RK_TIMER *timer = K_GET_CONTAINER_ADDR(node, RK_TIMER,
                                                       timeoutNode);
                if (timer->funPtr != NULL)
                {
                    timer->funPtr(timer->argsPtr);
                }
                if (timer->reload)
                {
                    RK_TICK now = kTickGet();
                    RK_TICK base = timer->nextTime;
                    RK_TICK elapsed = K_TICK_DELTA(now, base);
                    RK_TICK skips = ((elapsed / timer->period) + 1);
                    RK_TICK offset = (RK_TICK)(skips * timer->period);
                    timer->nextTime = K_TICK_ADD(base, offset);
                    RK_TICK delay = K_TICK_DELTA(timer->nextTime, now);
                    if (delay == 0)
                        K_PANIC("0 DELAY TIMER");
                    kTimerReload(timer, delay);
                }   
            }
        }
#endif
    }
}
