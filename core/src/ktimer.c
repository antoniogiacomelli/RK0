/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
 
/*******************************************************************************
 * 	COMPONENT        : TIMER
 * 	DEPENDS          : LOW-LEVEL SCHEDULER
 *  PROVIDES TO      : HIGH-LEVEL SCHEDULER
 *  PUBLIC API   	 : YES
 ******************************************************************************/

#define RK_SOURCE_CODE
#define RK_USE_HAL
#include "ktimer.h"
#ifndef RK_TASK_SLEEP_TIMEOUT_SETUP
#define RK_TASK_SLEEP_TIMEOUT_SETUP                     \
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_SLEEP; \
    runPtr->timeoutNode.waitingQueuePtr = NULL;
#endif

/******************************************************************************
 * GLOBAL TICK RETURN
 *****************************************************************************/


ULONG RKVAL_SysTickInterval = 0;

RK_TICK kTickGet(void)
{
    return (runTime.globalTick);
}

RK_TICK kTickGetMs(VOID)
{
    if (RKVAL_SysTickInterval != 0)
    {
        return (runTime.globalTick * RKVAL_SysTickInterval);
    }
    return (0UL);
}

RK_ERR kDelay(RK_TICK const ticks)
{
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (ticks == 0)
        return (RK_ERR_INVALID_PARAM);

    RK_TICK start = kTickGet();
    RK_TICK deadline = K_TICK_ADD(start, ticks);

    while (!K_TICK_EXPIRED(deadline))
        ;

    return (RK_ERR_SUCCESS);
}

#if (RK_CONF_CALLOUT_TIMER == ON)
/******************************************************************************
 * CALLOUT TIMERS
 *****************************************************************************/
RK_TIMER *currTimerPtr = NULL;

static inline VOID kTimerListAdd_(RK_TIMER *kobj, RK_TICK phase,
                                  RK_TICK countTicks, RK_TIMER_CALLOUT funPtr, VOID *argsPtr, BOOL reload)
{
    kobj->timeoutNode.dtick = countTicks;
    kobj->timeoutNode.timeout = countTicks;
    kobj->timeoutNode.timeoutType = RK_TIMEOUT_TIMER;
    kobj->funPtr = funPtr;
    kobj->argsPtr = argsPtr;
    kobj->reload = reload;
    kobj->phase = phase;
    kobj->period = countTicks;
    kobj->nextTime = K_TICK_ADD(kTickGet(), phase + countTicks);
    kTimeOut(&kobj->timeoutNode, countTicks);
}
    

RK_ERR kTimerInit(RK_TIMER *const kobj, RK_TICK const phase,
                  RK_TICK const countTicks, RK_TIMER_CALLOUT const funPtr,
                  VOID *const argsPtr, BOOL const reload)
{
    RK_CR_AREA
    RK_CR_ENTER
    
#if (RK_CONF_ERR_CHECK == ON)

    if ((kobj == NULL) || (funPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->init == TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#endif

    kTimerListAdd_(kobj, phase, countTicks, funPtr, argsPtr, reload);
    kobj->init = TRUE;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

VOID kTimerReload(RK_TIMER *kobj, RK_TICK delay)
{
    kobj->phase = 0;
    kobj->timeoutNode.timeoutType = RK_TIMEOUT_TIMER;
    kTimeOut(&kobj->timeoutNode, delay);
}


VOID kRemoveTimerNode(RK_TIMEOUT_NODE *node)
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

RK_ERR kTimerCancel(RK_TIMER *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    RK_TIMEOUT_NODE *node = (RK_TIMEOUT_NODE *)&kobj->timeoutNode;

    if ((node->nextPtr == NULL) && (node->prevPtr == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_ERROR);
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

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
#endif
/*******************************************************************************
 * SLEEP TIMER AND BLOCKING TIME-OUT
 ******************************************************************************/
RK_ERR kSleepDelay(RK_TICK ticks)
{
    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
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
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_TASK_SLEEP_TIMEOUT_SETUP

    kTimeOut(&runPtr->timeoutNode, ticks);
    runPtr->status = RK_SLEEPING_DELAY;
    RK_CR_EXIT
    RK_PEND_CTXTSWTCH
    return (RK_ERR_SUCCESS);
}

RK_ERR kSleepPeriod(RK_TICK period)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (period <= 0)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if ((UINT)(period) > RK_MAX_PERIOD)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    RK_TICK current = kTickGet();
    RK_TICK baseWake = runPtr->wakeTime;
    RK_TICK elapsed = K_TICK_DELAY(current, baseWake);
    /* we late? if so, skips > 1 */
    RK_TICK skips = ((elapsed / period) + 1);
    /* next wake = base + skips*period  */
    RK_TICK offset = (RK_TICK)(skips * period);
    RK_TICK nextWake = K_TICK_ADD(baseWake, offset);
    RK_TICK delay = K_TICK_DELAY(nextWake, current);
    if (delay > 0)
    {
        RK_TASK_SLEEP_TIMEOUT_SETUP
        RK_ERR err = kTimeOut(&runPtr->timeoutNode, delay);
        kassert(err == 0);
        runPtr->status = RK_SLEEPING_PERIOD;
        RK_PEND_CTXTSWTCH
    }
    /* update for the next cycle */
    runPtr->wakeTime = nextWake;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* add caller to timeout list (delta-list) */
RK_ERR kTimeOut(RK_TIMEOUT_NODE *timeOutNode, RK_TICK timeout)
{

#if (RK_CONF_ERR_CHECK == ON)
    if (timeout <= 0)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
    if ((UINT)(timeout) > RK_MAX_PERIOD)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
    if (timeOutNode == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif
    runPtr->timeOut = FALSE;
    timeOutNode->timeout = timeout;
    timeOutNode->prevPtr = NULL;
    timeOutNode->nextPtr = NULL;
    timeOutNode->dtick = timeout;
    if (timeOutNode->timeoutType == RK_TIMEOUT_TIMER)
    {
        RK_TIMEOUT_NODE *currPtr = (RK_TIMEOUT_NODE *)timerListHeadPtr;
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
        RK_TIMEOUT_NODE *currPtr = (RK_TIMEOUT_NODE *)timeOutListHeadPtr;
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
    return (RK_ERR_SUCCESS);
}

/* Ready the task associated to a time-out node, accordingly to its time-out type */

RK_ERR kTimeOutReadyTask(volatile RK_TIMEOUT_NODE *node)
{
    RK_TCB *taskPtr = K_GET_CONTAINER_ADDR(node, RK_TCB, timeoutNode);

    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        RK_ERR err = kTCBQRem(taskPtr->timeoutNode.waitingQueuePtr, &taskPtr);
        if (err == RK_ERR_SUCCESS)
        {
            if (!kTCBQEnq(&readyQueue[taskPtr->priority], taskPtr))
            {
                taskPtr->timeOut = TRUE;
                taskPtr->status = RK_READY;
                taskPtr->timeoutNode.timeoutType = 0;
                taskPtr->timeoutNode.waitingQueuePtr = NULL;
            }

            return (RK_ERR_SUCCESS);
        }
    }
    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_SLEEP)
    {
        if ((taskPtr->status == RK_SLEEPING_DELAY) ||
            (taskPtr->status == RK_SLEEPING_PERIOD))
        {
            if (!kTCBQEnq(&readyQueue[taskPtr->priority], taskPtr))
            {
                taskPtr->status = RK_READY;
                taskPtr->timeoutNode.timeoutType = 0;
                return (RK_ERR_SUCCESS);
            }
        }
    }

    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_ELAPSING)
    {
        if (!kTCBQEnq(&readyQueue[taskPtr->priority], taskPtr))
        {
            taskPtr->timeOut = TRUE;
            taskPtr->status = RK_READY;
            taskPtr->timeoutNode.timeoutType = 0;
            return (RK_ERR_SUCCESS);
        }
    }
    return (RK_ERR_ERROR);
}

/* runs @ systick */
static volatile RK_TIMEOUT_NODE *nodeg;
BOOL kHandleTimeoutList(VOID)
{
    RK_ERR err = RK_ERR_ERROR;

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
        kRemoveTimeoutNode((RK_TIMEOUT_NODE *)nodeg);

        err = kTimeOutReadyTask(nodeg);
    }

    return (err == RK_ERR_SUCCESS);
}
VOID kRemoveTimeoutNode(RK_TIMEOUT_NODE *node)
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
