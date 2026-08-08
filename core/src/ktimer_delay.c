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

RK_ERR kDelayCore(RK_TICK const ticks)
{

#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (ticks > RK_MAX_PERIOD)
    {

        K_ERR_HANDLER(RK_ERR_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_TICK remaining = ticks;
    RK_TICK lastObservedTick = kTickGet();

    while (remaining > 0UL)
    {
        RK_TICK now = kTickGet();
        if (now == lastObservedTick)
        {
            continue;
        }

        /*
         * emulates CPU workload. Count one unit per observed tick
         * change
         */
        lastObservedTick = now;
        --remaining;
    }

    return (RK_ERR_SUCCESS);
}
