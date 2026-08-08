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

#include "ktimer_private.h"

#if (RK_CONF_CALLOUT_TIMER == ON)

RK_ERR kTimerInitCore(RK_TIMER *const kobj, RK_TICK const phase,
                  RK_TICK const countTicks, RK_TIMER_CALLOUT const funPtr,
                  VOID *const argsPtr, RK_OPTION const reload)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if ((kobj == NULL) || (funPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    if ((countTicks == 0UL) || (countTicks > RK_MAX_PERIOD) ||
        (phase > RK_MAX_PERIOD) ||
        (phase > (RK_MAX_PERIOD - countTicks)) ||
        ((reload != RK_TIMER_ONESHOT) && (reload != RK_TIMER_RELOAD)))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

#endif

    if ((countTicks == 0UL) || (countTicks > RK_MAX_PERIOD) ||
        (phase > RK_MAX_PERIOD) ||
        (phase > (RK_MAX_PERIOD - countTicks)) ||
        ((reload != RK_TIMER_ONESHOT) && (reload != RK_TIMER_RELOAD)))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    RK_ERR err =
        kTimerListAdd_(kobj, phase, countTicks, funPtr, argsPtr, reload);
    if (err == 0)
    {
        kobj->init = RK_TRUE;
        kobj->objID = RK_TIMER_KOBJ_ID;
        kobj->objName[0] = '\0';
        kTraceRegisterObject(kobj, RK_TIMER_KOBJ_ID);
    }
    RK_CR_EXIT
    return (err);
}

#endif
