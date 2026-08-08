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

/* Called from timeout handling with scheduler data protected. */
VOID kChannelTimeoutRequest(RK_TCB *const callerPtr)
{
    RK_TCB *serverPtr = NULL;

    if (callerPtr == NULL)
    {
        return;
    }

    serverPtr = callerPtr->channelServerPtr;
    if (serverPtr == NULL)
    {
        return;
    }

    if (callerPtr->channelState == RK_CALL_QUEUED)
    {
        kChannelClearCaller_(callerPtr);
        return;
    }

    if ((callerPtr->channelState == RK_CALL_ACTIVE) &&
        (serverPtr->channelActiveCallerPtr == callerPtr))
    {
        callerPtr->channelReqPtr = NULL;
        callerPtr->channelRespPtr = NULL;
        callerPtr->channelReqSize = 0UL;
        callerPtr->channelState = RK_CALL_ABANDONED;
    }
}

#endif /* RK_CONF_CHANNEL */
