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
/* COMPONENT: SYNCHRONOUS TASK-TO-TASK RENDEZVOUS                             */
/******************************************************************************/

#define RK_SOURCE_CODE

#include "krendezvous_private.h"

#if (RK_CONF_RENDEZVOUS == ON)

RK_ERR kRendezvousSendCore(RK_TASK_HANDLE const taskHandle,
                       VOID const *const mesgPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((taskHandle == NULL) || (mesgPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (taskHandle == RK_gRunPtr)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (taskHandle->rendezvousMesgBytes == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (taskHandle->rendezvousMesgBytes == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kRendezvousTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if ((taskHandle->rendezvousRecvBufPtr != NULL) &&
        (taskHandle->rendezvousPendingMesgPtr == NULL))
    {
        RK_ERR const err = kRendezvousDirectRecv_(taskHandle, mesgPtr);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gRunPtr->rendezvousMesgPtr = mesgPtr;
    RK_gRunPtr->rendezvousStatus = RK_ERR_SUCCESS;
    RK_gRunPtr->rendezvousReceiverPtr = taskHandle;

    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_SEND;
        RK_gRunPtr->timeoutNode.waitingQueuePtr =
            &taskHandle->rendezvousSenders;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kRendezvousClearSender_(RK_gRunPtr);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_SENDING;
    RK_ERR enqErr = kTCBQEnqByPrio(&taskHandle->rendezvousSenders,
                                   RK_gRunPtr);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if (timeout != RK_WAIT_FOREVER)
        {
            kRendezvousDisarmTimeout_(RK_gRunPtr);
        }
        kRendezvousClearSender_(RK_gRunPtr);
        RK_CR_EXIT
        return (enqErr);
    }

    kRendezvousPromoteNext_(taskHandle);
    kRendezvousUpdateReceiverPrio_(taskHandle);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    RK_ERR const err = RK_gRunPtr->rendezvousStatus;
    RK_gRunPtr->rendezvousStatus = RK_ERR_SUCCESS;
    RK_CR_EXIT
    return (err);
}

#endif
