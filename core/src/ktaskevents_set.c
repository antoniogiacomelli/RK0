/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0 */
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

RK_ERR kEventSetCore(RK_TASK_HANDLE const receiverHandle, ULONG const setFlags)
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

        andLogic = (receiverHandle->flagsOpt == RK_OPT_EVENT_ALL);

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
