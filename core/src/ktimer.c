/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.19                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/

/******************************************************************************/
/* COMPONENT: TIMER                                                       */
/******************************************************************************/

#define RK_SOURCE_CODE
#include "ktimer.h"

/******************************************************************************
 * GLOBAL TICK RETURN
 *****************************************************************************/

ULONG RK_gSysTickInterval = 0;

RK_TICK kTickGet(void)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TICK ret = (RK_gRunTime.globalTick);
    RK_CR_EXIT
    return (ret);
}

RK_TICK kTickGetMs(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TICK ret = 0UL;
    if (RK_gSysTickInterval != 0)
    {
        ret = (RK_gRunTime.globalTick * RK_gSysTickInterval);
    }
    RK_CR_EXIT
    return (ret);
}

RK_ERR kDelay(RK_TICK const ticks)
{

    #if (RK_CONF_ERR_CHECK==ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (ticks > RK_MAX_PERIOD)
    {
        
        K_ERR_HANDLER(RK_ERR_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
    #endif

    volatile RK_TICK start = kTickGet();
    volatile RK_TICK deadline = K_TICK_ADD(start, ticks);


    while (!K_TICK_EXPIRED(deadline))
        ;

    return (RK_ERR_SUCCESS);
}

#if (RK_CONF_CALLOUT_TIMER == ON)

/******************************************************************************
 * CALLOUT TIMERS
 *****************************************************************************/
static inline RK_ERR kTimerListAdd_(RK_TIMER *kobj, RK_TICK phase,
                                    RK_TICK countTicks, RK_TIMER_CALLOUT funPtr, VOID *argsPtr, UINT reload)
{
    kobj->timeoutNode.dtick = countTicks;
    kobj->timeoutNode.timeout = countTicks;
    kobj->timeoutNode.timeoutType = RK_TIMEOUT_CALL;
    kobj->funPtr = funPtr;
    kobj->argsPtr = argsPtr;
    kobj->reload = reload;
    kobj->phase = phase;
    kobj->period = countTicks;
    kobj->nextTime = K_TICK_ADD(kTickGet(), phase + countTicks);
    return (kTimeoutNodeAdd(&kobj->timeoutNode, countTicks));
}

RK_ERR kTimerInit(RK_TIMER *const kobj, RK_TICK const phase,
                  RK_TICK const countTicks, RK_TIMER_CALLOUT const funPtr,
                  VOID *const argsPtr, UINT const reload)
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
    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#endif

    RK_ERR err = kTimerListAdd_(kobj, phase, countTicks, funPtr, argsPtr, reload);
    if (err == 0)
    {
        kobj->init = RK_TRUE;
        kobj->objID = RK_TIMER_KOBJ_ID;
    }
    RK_CR_EXIT
    return (err);
}

VOID kTimerReload(RK_TIMER *kobj, RK_TICK delay)
{
    kobj->phase = 0;
    kobj->timeoutNode.timeoutType = RK_TIMEOUT_CALL;
    kTimeoutNodeAdd(&kobj->timeoutNode, delay);
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
        RK_gTimerListHeadPtr = node->nextPtr;
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
        RK_gTimerListHeadPtr = node->nextPtr;
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
    if (RK_gRunPtr->status != RK_RUNNING)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }
    if (ticks > RK_MAX_PERIOD)
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
#endif
    if (ticks == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    RK_TASK_SLEEP_TIMEOUT_SETUP

    RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, ticks);
    if (err != RK_ERR_SUCCESS)
    {
        RK_gRunPtr->timeoutNode.timeoutType = 0;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        RK_CR_EXIT
        return (err);
    }
    RK_gRunPtr->status = RK_SLEEPING_DELAY;
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSleepRelease(RK_TICK period)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (period == 0)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if ((ULONG)(period) > RK_MAX_PERIOD)
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

    /* a call to sleep release means end cycle + schedule next activation */
    /* first waketime is 0, meaning the initial baseWake for every task is */
    /* when the schedular starts, so, on the first activation of every task */
    /* the accumulated delay until the end of its computation time is taken */
    /* into account. */
    /* offsetFactor is 1 for no overrun */
    /* then, offset is factor x period.    */
    /* delay = offset+current */

    RK_TICK current = kTickGet();
    RK_TICK baseWake = RK_gRunPtr->wakeTime;
    RK_TICK elapsed = K_TICK_DELTA(current, baseWake);

    /* elapsed/period is > 0 iff */
    RK_TICK offsetFactor = ((elapsed / period) + 1);
    if (offsetFactor > 1)
    {
        RK_gRunPtr->overrunCount += 1U;
    }
    RK_TICK offset = (RK_TICK)(offsetFactor * period);    
    RK_TICK nextWake = K_TICK_ADD(baseWake, offset);
    RK_TICK delay = 0;
    delay = K_TICK_DELTA(nextWake, current);
    if (delay == 0) 
    {
         #ifndef NDEBUG

        {
            K_PANIC("0 DELAY SLEEPRELEASE\r\n");
            return (RK_ERR_ERROR);
        }
        #else
        {
            /* this is not supposed to happen, but a possible fallback is 
            to set reassign delay to remaining time next phase */

            /* get remainder */
            RK_TICK rem = current - (current / period) * period;  
            delay = (rem == 0UL) ? period : (period - rem);
            nextWake = K_TICK_ADD(current, delay);        
        }
        #endif
    }
    RK_gRunPtr->wakeTime = nextWake;
    RK_TASK_SLEEP_TIMEOUT_SETUP
    RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, delay);
    if (err != RK_ERR_SUCCESS)
    {
        RK_gRunPtr->timeoutNode.timeoutType = 0;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        RK_CR_EXIT
        return (err);
    }
    RK_gRunPtr->status = RK_SLEEPING_RELEASE;
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* sleep for time, relative to local anchored tick */
RK_ERR kSleepUntil(RK_TICK *lastTickPtr, RK_TICK const ticks)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (lastTickPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return RK_ERR_OBJ_NULL;
    }
    if (ticks > RK_MAX_PERIOD)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return RK_ERR_INVALID_PARAM;
    }
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return RK_ERR_INVALID_ISR_PRIMITIVE;
    }
#endif

    RK_TICK now = kTickGet();

    /* advance   */
    *lastTickPtr = K_TICK_ADD(*lastTickPtr, ticks);

    /* late or on-time  */
    if (kTickIsElapsed(*lastTickPtr, now))
    {
        RK_CR_EXIT
        return (RK_ERR_ELAPSED_PERIOD);
    }

    RK_TICK remaining = K_TICK_DELTA(*lastTickPtr, now);
    RK_TASK_SLEEP_TIMEOUT_SETUP
    RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, remaining);
    if (err != RK_ERR_SUCCESS)
    {
        RK_gRunPtr->timeoutNode.timeoutType = 0;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        RK_CR_EXIT
        return (err);
    }
    RK_gRunPtr->status = RK_SLEEPING_UNTIL;
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

static void kTimeoutListInsertDelta_(RK_TIMEOUT_NODE **headPtr, RK_TIMEOUT_NODE *node)
{
    RK_TIMEOUT_NODE *currPtr = *headPtr;
    RK_TIMEOUT_NODE *prevPtr = NULL;

    while (currPtr != NULL && currPtr->dtick < node->dtick)
    {
        node->dtick -= currPtr->dtick;
        prevPtr = currPtr;
        currPtr = currPtr->nextPtr;
    }

    /* Link node in between prevPtr and currPtr */
    node->nextPtr = currPtr;
    node->prevPtr = prevPtr;

    if (currPtr != NULL)
    {
        currPtr->dtick -= node->dtick;
        currPtr->prevPtr = node;
    }

    if (prevPtr != NULL)
    {
        prevPtr->nextPtr = node;
    }
    else
    {
        *headPtr = node;
    }
}

/* add caller to timeout list (delta-list) */
RK_ERR kTimeoutNodeAdd(RK_TIMEOUT_NODE *timeOutNode, RK_TICK timeout)
{

#if (RK_CONF_ERR_CHECK == ON)
    if (timeout == 0)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        return (RK_ERR_INVALID_TIMEOUT);
    }
    if (timeout > RK_MAX_PERIOD)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        return (RK_ERR_INVALID_TIMEOUT);
    }
    if (timeOutNode == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif
    RK_gRunPtr->timeOut = RK_FALSE;
    timeOutNode->timeout = timeout;
    timeOutNode->prevPtr = NULL;
    timeOutNode->nextPtr = NULL;
    timeOutNode->dtick = timeout;

    if (timeOutNode->timeoutType == RK_TIMEOUT_CALL)
    {
        kTimeoutListInsertDelta_((RK_TIMEOUT_NODE **)&RK_gTimerListHeadPtr, timeOutNode);
    }
    else
    {
        kTimeoutListInsertDelta_((RK_TIMEOUT_NODE **)&RK_gTimeOutListHeadPtr, timeOutNode);
    }

    return (RK_ERR_SUCCESS);
}
/* Ready the task associated to a time-out node, accordingly to its time-out type */


RK_FORCE_INLINE
static inline
RK_ERR kTCBQEnqByWakeTime(RK_TCBQ *const kobj, RK_TCB *const tcbPtr)
{
     RK_NODE *currNodePtr = &(kobj->listDummy);

    while (currNodePtr->nextPtr != &(kobj->listDummy))
    {
        RK_TCB const *currTcbPtr = K_GET_TCB_ADDR(currNodePtr->nextPtr);
        if (currTcbPtr->wakeTime < tcbPtr->wakeTime)
        {
            break;
        }
        currNodePtr = currNodePtr->nextPtr;
    }

    RK_ERR err = kListInsertAfter(kobj, currNodePtr, &(tcbPtr->tcbNode));

    return (err);

}
RK_ERR kTimeoutNodeReady(volatile RK_TIMEOUT_NODE *node)
{
    RK_TCB *taskPtr = K_GET_CONTAINER_ADDR(node, RK_TCB, timeoutNode);

    K_ASSERT(taskPtr != NULL);

    RK_ERR err = -1;

    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        err = kTCBQRem(taskPtr->timeoutNode.waitingQueuePtr, &taskPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
        err = kTCBQEnq(&RK_gReadyQueue[taskPtr->priority], taskPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }

        taskPtr->timeOut = RK_TRUE;
        taskPtr->status = RK_READY;
        taskPtr->timeoutNode.timeoutType = 0;
        taskPtr->timeoutNode.waitingQueuePtr = NULL;
        return (err);
    }
    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_TIME_EVENT)
    {
            err = kTCBQEnq(&RK_gReadyQueue[taskPtr->priority], taskPtr);
            
            K_ASSERT(err==0);
            
            if (err != RK_ERR_SUCCESS)
            {
                return (err);
            }
            /* a time event as any sleep does not change timeOut = TRUE */
            taskPtr->status = RK_READY;
            taskPtr->timeoutNode.timeoutType = 0;
            return (err);
    }
    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_EVENTFLAGS)
    {
        err = kTCBQEnq(&RK_gReadyQueue[taskPtr->priority], taskPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
        taskPtr->timeOut = RK_TRUE;
        taskPtr->status = RK_READY;
        taskPtr->timeoutNode.timeoutType = 0;
    }
    return (err);
}

/* runs @ systick */
static volatile RK_TIMEOUT_NODE *nodeg;
UINT kHandleTimeoutList(VOID)
{

    RK_ERR timerExp = -1;
    RK_ERR waitingExp = -1;

    if (RK_gTimeOutListHeadPtr->dtick > 0)
    {
        RK_gTimeOutListHeadPtr->dtick--;
    }

    /*  possible to have a node which offset is already (dtick == 0) */
    while (RK_gTimeOutListHeadPtr != NULL && RK_gTimeOutListHeadPtr->dtick == 0)
    {
        nodeg = RK_gTimeOutListHeadPtr;
        /* Remove the expired nodeg from the list */
        RK_gTimeOutListHeadPtr = nodeg->nextPtr;
        kRemoveTimeoutNode((RK_TIMEOUT_NODE *)nodeg);
        waitingExp = kTimeoutNodeReady(nodeg);
    }
#if (RK_CONF_CALLOUT_TIMER == 1)
    if (RK_gTimerListHeadPtr != NULL)
    {

        RK_TIMER *headTimPtr = K_GET_CONTAINER_ADDR(RK_gTimerListHeadPtr, RK_TIMER, timeoutNode);

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
            timerExp = kTaskEventSet(RK_gPostProcTaskHandle, RK_POSTPROC_TIMER_SIG);
        }
#endif
    }
    return ((timerExp == RK_ERR_SUCCESS) || (waitingExp == RK_ERR_SUCCESS));
}
RK_ERR kRemoveTimeoutNode(RK_TIMEOUT_NODE *node)
{
    if (node == NULL)
        return (RK_ERR_NULL_TIMEOUT_NODE);

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
        RK_gTimeOutListHeadPtr = node->nextPtr;
    }

    node->nextPtr = NULL;
    node->prevPtr = NULL;
    return (RK_ERR_SUCCESS);
}
