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

RK_ERR kChannelDoneCore(RK_CALL_DATA const *const callPtr)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((callPtr == NULL) || (callPtr->caller == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif

    RK_TCB *callerPtr = callPtr->caller;
    RK_TCB *requesterToReady = NULL;
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

    if ((RK_gRunPtr->channelActiveCallerPtr != callerPtr) ||
        (callerPtr->channelServerPtr != RK_gRunPtr) ||
        ((callerPtr->channelState != RK_CALL_ACTIVE) &&
         (callerPtr->channelState != RK_CALL_ABANDONED)))
    {
        RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_CHANNEL_NOT_ACTIVE);
#endif
        return (RK_ERR_CHANNEL_NOT_ACTIVE);
    }

    if ((callerPtr->status == RK_RECEIVING) &&
        (callerPtr->timeoutNode.waitingQueuePtr == &RK_gRunPtr->channelCallers))
    {
        if (callerPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&callerPtr->timeoutNode);
            callerPtr->timeoutNode.timeoutType = 0U;
        }
        callerPtr->timeoutNode.waitingQueuePtr = NULL;
        callerPtr->timeoutNode.waitInfo = 0U;

        requesterToReady = callerPtr;
        RK_ERR err = kTCBQRem(&RK_gRunPtr->channelCallers, &requesterToReady);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }

    RK_gRunPtr->channelActiveCallerPtr = NULL;
    kChannelClearCaller_(callerPtr);
    kChannelRestoreServerPrio_(RK_gRunPtr);

    if (requesterToReady != NULL)
    {
        kReadySwtch(requesterToReady);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_CHANNEL */
