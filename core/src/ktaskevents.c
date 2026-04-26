/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.19.2 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: TASK EVENT REGISTER                                             */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktaskevents.h>

/*****************************************************************************/
/* TASK EVENTS                                                               */
/*****************************************************************************/
RK_ERR kEventGet(ULONG const requiredFlags, UINT const getOptions,
                 ULONG *const gotFlagsPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    /* check for invalid parameters and return specific error */
    /* an ISR has no task control block */
    if (kIsISR())
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    /* check for invalid getOptions, including requiredFlags flags == 0 */
    if ((getOptions != RK_EVENT_FLAGS_ALL && getOptions != RK_EVENT_FLAGS_ANY) ||
        requiredFlags == 0UL)
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    RK_gRunPtr->flagsReq = requiredFlags;
    RK_gRunPtr->flagsOpt = getOptions;

    /* inspecting the flags upon returning is optional */
    if (gotFlagsPtr != NULL)
        *gotFlagsPtr = RK_gRunPtr->flagsCurr;

    UINT andLogic = (getOptions == RK_EVENT_FLAGS_ALL);
    UINT conditionMet = 0;

    /* check if ANY or ALL flags establish a waiting condition */
    if (andLogic) /* ALL */
    {
        conditionMet =
            ((RK_gRunPtr->flagsCurr & requiredFlags) == (RK_gRunPtr->flagsReq));
    }
    else
    {
        conditionMet = (RK_gRunPtr->flagsCurr & requiredFlags);
    }

    /* if condition is met, clear flags and return */
    if (conditionMet)
    {
        RK_gRunPtr->flagsCurr &= ~RK_gRunPtr->flagsReq;
        RK_gRunPtr->flagsReq = 0UL;
        RK_gRunPtr->flagsOpt = 0UL;
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }
    /* condition not met, and non-blocking call, return FLAGS_NOT_MET */
    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_FLAGS_NOT_MET);
    }

    /* start suspension */

    /* if bounded timeout, enqueue task on timeout list with no
        associated waiting queue */
    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_EVENTFLAGS

        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            RK_gRunPtr->timeoutNode.timeoutType = 0;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_CR_EXIT
            return (err);
        }
    }
    RK_gRunPtr->status = RK_SLEEPING_EV_FLAG;
    /* swtch ctxt */
    kPendCtxSwtch();

    RK_CR_EXIT

    /* suspension is resumed here */
    RK_CR_ENTER
    /* if resuming reason is timeout return ERR_TIMEOUT */
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    /* resuming reason is a Set with condition met */

    /* if bounded waiting, remove task from timeout list */
    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
        (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_EVENTFLAGS))
    {
        kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        RK_gRunPtr->timeoutNode.timeoutType = 0;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
    }

    /* store current flags if asked */
    if (gotFlagsPtr != NULL)
        *gotFlagsPtr = RK_gRunPtr->flagsCurr;

    /* clear flags on the TCB and return SUCCESS */
    RK_gRunPtr->flagsCurr &= ~RK_gRunPtr->flagsReq;
    RK_gRunPtr->flagsReq = 0UL;
    RK_gRunPtr->flagsOpt = 0UL;
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kEventSet(RK_TASK_HANDLE const receiverHandle, ULONG const setFlags)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    /* check for invalid parameters and return specific error */
    if (receiverHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (setFlags == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    /* OR mask to current flags */
    receiverHandle->flagsCurr |= setFlags;
    if ((receiverHandle->status == RK_SLEEPING_EV_FLAG))
    {
        UINT andLogic = 0;
        UINT conditionMet = 0;

        andLogic = (receiverHandle->flagsOpt == RK_EVENT_FLAGS_ALL);

        if (andLogic)
        {
            conditionMet = ((receiverHandle->flagsCurr & receiverHandle->flagsReq) ==
                            (receiverHandle->flagsReq));
        }
        else
        {
            conditionMet = (receiverHandle->flagsCurr & receiverHandle->flagsReq);
        }

        /* if condition is met and task is pending, ready task
        and return SUCCESS */
        if (conditionMet)
        {
            if (receiverHandle->timeoutNode.timeoutType == RK_TIMEOUT_EVENTFLAGS)
            {
                kRemoveTimeoutNode(&receiverHandle->timeoutNode);
                receiverHandle->timeoutNode.timeoutType = 0;
                receiverHandle->timeoutNode.waitingQueuePtr = NULL;
            }
            kReadySwtch(receiverHandle);
        }
    }


    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}


RK_ERR kEventClear(RK_TASK_HANDLE taskHandle, ULONG const flagsToClear)
{
    /* a clear cannot be interrupted */
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    /* an ISR has no TCB */
    if (kIsISR() && (taskHandle == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (flagsToClear == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_FAULT_INVALID_PARAM);
    }
#endif

    RK_TCB *taskPtr = (taskHandle) ? taskHandle : RK_gRunPtr;

    taskPtr->flagsCurr &= ~flagsToClear;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kEventQuery(RK_TASK_HANDLE const taskHandle, ULONG *const queryFlagsPtr)
{

    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
    if (queryFlagsPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kIsISR() && (taskHandle == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif
    RK_TASK_HANDLE handle = (taskHandle) ? (taskHandle) : (RK_gRunPtr);
    (*queryFlagsPtr = handle->flagsCurr);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
