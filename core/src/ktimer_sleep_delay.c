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
/* COMPONENT: TIMER                                                           */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktimer.h>

RK_ERR kSleepDelayCore(RK_TICK ticks)
{
    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (RK_gRunPtr->status != RK_RUNNING)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }
    if (ticks > RK_MAX_PERIOD)
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
#endif
    if (ticks == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    RK_TASK_SLEEP_TIMEOUT_SETUP

    RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, ticks);
    if (err != RK_ERR_SUCCESS)
    {
        kTimeoutNodeReset(&RK_gRunPtr->timeoutNode);
        RK_CR_EXIT
        return (err);
    }
    RK_gRunPtr->status = RK_SLEEPING_DELAY;
    kPendCtxSwtch();
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
