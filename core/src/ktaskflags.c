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
/******************************************************************************/

/******************************************************************************/
/**                                                                           */
/**  COMPONENT        : TASK FLAGS                                            */
/**  DEPENDS          : LOW-LEVEL SCHEDULER, TIMER                            */
/**  PROVIDES TO      : HIGH-LEVEL SCHEDULER                                  */
/**  PUBLIC API   	  : YES                                                   */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktaskflags.h>

/* Timeout Node Setup */
#ifndef RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP               \
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_ELAPSING; \
    runPtr->timeoutNode.waitingQueuePtr = NULL;
#endif

/*****************************************************************************/
/* TASK FLAGS                                                                */
/*****************************************************************************/
RK_ERR kTaskFlagsGet(ULONG const required, UINT const options,
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
    /* check for invalid options, including required flags == 0 */
    if ((options != RK_FLAGS_ALL && options != RK_FLAGS_ANY) || required == 0UL)
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    runPtr->flagsReq = required;
    runPtr->flagsOpt = options;

    /* inspecting the flags upon returning is optional */
    if (gotFlagsPtr != NULL)
        *gotFlagsPtr = runPtr->flagsCurr;

    BOOL andLogic = (options == RK_FLAGS_ALL);
    BOOL conditionMet = 0;

    /* check if ANY or ALL flags establish a waiting condition */
    if (andLogic) /* ALL */
    {
        conditionMet = ((runPtr->flagsCurr & required) == (runPtr->flagsReq));
    }
    else
    {
        conditionMet = (runPtr->flagsCurr & required);
    }

    /* if condition is met, clear flags and return */
    if (conditionMet)
    {
        runPtr->flagsCurr &= ~runPtr->flagsReq;
        runPtr->flagsReq = 0UL;
        runPtr->flagsOpt = 0UL;
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

    runPtr->status = RK_PENDING;

    /* if bounded timeout, enqueue task on timeout list with no
        associated waiting queue */
    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_NOWAITINGQUEUE_SETUP

        kTimeOut(&runPtr->timeoutNode, timeout);
    }
    /* swtch ctxt */
    RK_PEND_CTXTSWTCH
    RK_CR_EXIT

    /* suspension is resumed here */
    RK_CR_ENTER
    /* if resuming reason is timeout return ERR_TIMEOUT */
    if (runPtr->timeOut)
    {
        runPtr->timeOut = FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    /* resuming reason is a Set with condition met */

    /* if bounded waiting, remove task from timeout list */
    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        kRemoveTimeoutNode(&runPtr->timeoutNode);

    /* store current flags if asked */
    if (gotFlagsPtr != NULL)
        *gotFlagsPtr = runPtr->flagsCurr;

    /* clear flags on the TCB and return SUCCESS */
    runPtr->flagsCurr &= ~runPtr->flagsReq;
    runPtr->flagsReq = 0UL;
    runPtr->flagsOpt = 0UL;
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kTaskFlagsSet(RK_TASK_HANDLE const taskHandle, ULONG const mask)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    /* check for invalid parameters and return specific error */
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (mask == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    /* OR mask to current flags */
    taskHandle->flagsCurr |= mask;
    if (taskHandle->status == RK_PENDING)
    {
        BOOL andLogic = 0;
        BOOL conditionMet = 0;

        andLogic = (taskHandle->flagsOpt == RK_FLAGS_ALL);

        if (andLogic)
        {
            conditionMet = ((taskHandle->flagsCurr & taskHandle->flagsReq) == (taskHandle->flagsReq));
        }
        else
        {
            conditionMet = (taskHandle->flagsCurr & taskHandle->flagsReq);
        }

        /* if condition is met and task is pending, ready task
        and return SUCCESS */
        if (conditionMet)
        {
            kReadySwtch(&tcbs[taskHandle->pid]);
        }
    }
    /* if not, just return SUCCESS*/
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kTaskFlagsClear(RK_TASK_HANDLE taskHandle, ULONG const flagsToClear)
{
    /* a clear cannot be interrupted */
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    /* an ISR has no TCB */
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    RK_TCB* taskPtr = (taskHandle) ? taskHandle : runPtr;

    taskPtr->flagsCurr &= ~flagsToClear;
    RK_NOP
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kTaskFlagsQuery(RK_TASK_HANDLE const taskHandle, ULONG *const queryFlagsPtr)
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
#endif
    RK_TASK_HANDLE handle = (taskHandle) ? (taskHandle) : (runPtr);
    (*queryFlagsPtr = handle->flagsCurr);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
    
}

