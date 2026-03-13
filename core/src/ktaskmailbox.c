/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.13.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktaskmailbox.h>
#include <kapi.h>
#include <ktimer.h>
#include <ksch.h>

RK_ERR kMailSend(RK_TASK_HANDLE receiverTask, VOID *const sendPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((receiverTask == NULL) || (sendPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (receiverTask->pid >= RK_CONF_NTASKS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    receiverTask->mailbox = sendPtr;

    /* Wake receiver if it is blocking on its mailbox */
    if (receiverTask->status == RK_RECEIVING_TMAILBOX)
    {
        if (receiverTask->timeoutNode.timeoutType == RK_TIMEOUT_TMAILBOX)
        {
            kRemoveTimeoutNode(&receiverTask->timeoutNode);
            receiverTask->timeoutNode.timeoutType = 0;
            receiverTask->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadySwtch(receiverTask);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMailRecv(VOID **const recvPPtr, RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (recvPPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (RK_gRunPtr->mailbox != NULL)
    {
        *recvPPtr = RK_gRunPtr->mailbox;
        RK_gRunPtr->mailbox = NULL;
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_TASK_MBOX_EMPTY);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    do
    {
        RK_gRunPtr->status = RK_RECEIVING_TMAILBOX;

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_TMAILBOX;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;

            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->status = RK_RUNNING;
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
#if (RK_CONF_ERR_CHECK == ON)
                if (err == RK_ERR_INVALID_PARAM)
                {
                    K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
                }
#endif
                if (err == RK_ERR_INVALID_PARAM)
                {
                    err = RK_ERR_INVALID_TIMEOUT;
                }
                RK_CR_EXIT
                return (err);
            }
        }

        RK_PEND_CTXTSWTCH
        RK_CR_EXIT

        RK_CR_ENTER
        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_TMAILBOX))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        }
    } while (RK_gRunPtr->mailbox == NULL);

    *recvPPtr = RK_gRunPtr->mailbox;
    RK_gRunPtr->mailbox = NULL;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
RK_ERR kMailStatus(RK_TASK_HANDLE taskHandle)
{
    if (taskHandle == NULL)
        return (RK_ERR_OBJ_NULL);
    else 
    {
      return (taskHandle->mailbox == NULL : RK_ERR_TASK_MBOX_EMPTY ? RK_ERR_TASK_MBOX_FULL);
    }
}

RK_ERR kMailQuery(RK_TASK_HANDLE taskHandle);
{
    if (taskHandle == NULL)
        taskHandle == RK_gRunPtr;
      return ((taskHandle->mailbox == NULL) : RK_ERR_TASK_MBOX_EMPTY ? (RK_ERR_TASK_MBOX_FULL));
}


