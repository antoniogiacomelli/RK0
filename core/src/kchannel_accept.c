/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: PROCEDURE CALL CHANNEL                                          */
/******************************************************************************/

#define RK_SOURCE_CODE

#include "kchannel_private.h"

#if (RK_CONF_CHANNEL == ON)

RK_ERR kChannelAcceptCore(RK_CALL_DATA *const callPtr,
                          RK_TICK const timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (callPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    RK_CR_AREA
    RK_CR_ENTER

    if (kChannelTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (RK_gRunPtr->channelActiveCallerPtr != NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_CHANNEL_BUSY);
    }

    while (kChannelPeekQueuedCaller_(RK_gRunPtr) == NULL)
    {
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_gRunPtr->timeoutNode.waitingQueuePtr =
                &RK_gRunPtr->channelAcceptWaiters;
            RK_BARRIER
            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                RK_CR_EXIT
                return (err);
            }
        }

        RK_gRunPtr->status = RK_RECEIVING;
        RK_ERR err = kTCBQEnq(&RK_gRunPtr->channelAcceptWaiters, RK_gRunPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        if (err != RK_ERR_SUCCESS)
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
            }
            RK_gRunPtr->status = RK_RUNNING;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_CR_EXIT
            return (err);
        }

        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_gRunPtr->timeoutNode.waitInfo = 0U;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
        }
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        RK_gRunPtr->timeoutNode.waitInfo = 0U;
    }

    RK_TCB *callerPtr = kChannelPeekQueuedCaller_(RK_gRunPtr);
    K_ASSERT(callerPtr != NULL);
    if (callerPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    callerPtr->channelState = RK_CALL_ACTIVE;
    RK_gRunPtr->channelActiveCallerPtr = callerPtr;

    callPtr->caller = callerPtr;
    callPtr->reqPtr = callerPtr->channelReqPtr;
    callPtr->respPtr = callerPtr->channelRespPtr;
    callPtr->size = callerPtr->channelReqSize;

    kChannelAdoptClientPrio_(RK_gRunPtr, callerPtr);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_CHANNEL */
