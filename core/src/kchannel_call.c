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

RK_ERR kChannelCallCore(RK_TASK_HANDLE const serverTask,
                        VOID *const reqPtr,
                        VOID *const respPtr,
                        ULONG const size,
                        RK_TICK const timeout)
{
    RK_CR_AREA

#if (RK_CONF_ERR_CHECK == ON)
    if (serverTask == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (timeout == RK_NO_WAIT)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        return (RK_ERR_INVALID_TIMEOUT);
    }

    if ((serverTask == RK_gRunPtr) ||
        (kChannelTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE) ||
        (RK_gRunPtr->channelServerPtr != NULL) ||
        (RK_gRunPtr->channelState != RK_CALL_IDLE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        return (RK_ERR_TASK_INVALID_ST);
    }

    RK_CR_ENTER

    RK_gRunPtr->channelServerPtr = serverTask;
    RK_gRunPtr->channelReqPtr = reqPtr;
    RK_gRunPtr->channelRespPtr = respPtr;
    RK_gRunPtr->channelReqSize = size;
    RK_gRunPtr->channelState = RK_CALL_QUEUED;
    RK_gRunPtr->timeoutNode.waitingQueuePtr = &serverTask->channelCallers;
    RK_gRunPtr->timeoutNode.waitInfo = 0U;

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
        RK_BARRIER
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            kChannelClearCaller_(RK_gRunPtr);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_RECEIVING;
    RK_ERR enqErr = kTCBQEnqByPrio(&serverTask->channelCallers, RK_gRunPtr);
    K_ASSERT(enqErr == RK_ERR_SUCCESS);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
        }
        RK_gRunPtr->status = RK_RUNNING;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        kChannelClearCaller_(RK_gRunPtr);
        RK_CR_EXIT
        return (enqErr);
    }

    kChannelWakeServer_(serverTask);
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
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_CHANNEL */
