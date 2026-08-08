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
#include <ktrace.h>

RK_ERR kSleepUntilCore(RK_TICK *lastTickPtr, RK_TICK const ticks)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (lastTickPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return RK_ERR_OBJ_NULL;
    }
    if (ticks > RK_MAX_PERIOD)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return RK_ERR_INVALID_PARAM;
    }
    if (ticks == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return RK_ERR_INVALID_PARAM;
    }
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return RK_ERR_INVALID_ISR_PRIMITIVE;
    }
#endif

    if ((ticks == 0UL) || (ticks > RK_MAX_PERIOD))
    {
        RK_CR_EXIT
        return RK_ERR_INVALID_PARAM;
    }

    RK_TICK now = kTickGet();

    /* advance   */
    *lastTickPtr = K_TICK_ADD(*lastTickPtr, ticks);

    /* late or on-time  */
    if (kTickIsElapsed(*lastTickPtr, now))
    {
        RK_gRunPtr->overrunCount += 1U;
        kTraceRecordTaskOverrun(RK_TRACE_OVERRUN_UNTIL, ticks,
                                K_TICK_DELTA(now, *lastTickPtr), 0UL);
        RK_CR_EXIT
        return (RK_ERR_ELAPSED_PERIOD);
    }

    RK_TICK remaining = K_TICK_DELTA(*lastTickPtr, now);
    RK_TASK_SLEEP_TIMEOUT_SETUP
    RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, remaining);
    if (err != RK_ERR_SUCCESS)
    {
        kTimeoutNodeReset(&RK_gRunPtr->timeoutNode);
        RK_CR_EXIT
        return (err);
    }
    RK_gRunPtr->status = RK_SLEEPING_UNTIL;
    kPendCtxSwtch();
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
