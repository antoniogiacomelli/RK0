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

RK_ERR kRendezvousInitCore(RK_TASK_HANDLE const taskHandle,
                       ULONG const mesgBytes)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
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

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((mesgBytes == 0UL) || ((mesgBytes % RK_WORD_SIZE) != 0UL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    if (taskHandle->rendezvousMesgBytes != 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }

    taskHandle->rendezvousMesgBytes = mesgBytes;
    taskHandle->rendezvousPendingMesgPtr = NULL;
    taskHandle->rendezvousPendingSenderPtr = NULL;
    taskHandle->rendezvousRecvBufPtr = NULL;
    taskHandle->rendezvousRecvStatus = RK_ERR_SUCCESS;
    RK_ERR err = kTCBQInit(&taskHandle->rendezvousSenders);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif
