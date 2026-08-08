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

VOID kRendezvousTimeoutSend(RK_TCB *const senderPtr)
{
    RK_TCB *const receiverPtr = senderPtr->rendezvousReceiverPtr;
    if (receiverPtr == NULL)
    {
        return;
    }

    if (receiverPtr->rendezvousPendingSenderPtr == senderPtr)
    {
        receiverPtr->rendezvousPendingMesgPtr = NULL;
        receiverPtr->rendezvousPendingSenderPtr = NULL;
    }

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverPtr->rendezvousSenders, &remPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);

    kRendezvousClearSender_(senderPtr);
    kRendezvousPromoteNext_(receiverPtr);
    kRendezvousUpdateReceiverPrio_(receiverPtr);
}




#endif /* RK_CONF_RENDEZVOUS */
