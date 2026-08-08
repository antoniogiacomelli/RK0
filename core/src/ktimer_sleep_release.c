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

RK_ERR kSleepReleaseCore(RK_TICK period)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (period == 0)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if ((ULONG)(period) > RK_MAX_PERIOD)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if ((period == 0UL) || ((ULONG)(period) > RK_MAX_PERIOD))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    /* a call to sleep release means end cycle + schedule next activation */
    /* first waketime is 0, meaning the initial baseWake for every task is */
    /* when the schedular starts, so, on the first activation of every task */
    /* the accumulated delay until the end of its computation time is taken */
    /* into account. */
    /* offsetFactor is 1 for no overrun */
    /* then, offset is factor x period.    */
    /* delay = offset+current */

    RK_TICK current = kTickGet();
    RK_TICK baseWake = RK_gRunPtr->wakeTime;
    RK_TICK elapsed = K_TICK_DELTA(current, baseWake);

    /* elapsed/period is > 0 iff */
    RK_TICK offsetFactor = ((elapsed / period) + 1);
    if (offsetFactor > 1)
    {
        RK_gRunPtr->overrunCount += 1U;
        kTraceRecordTaskOverrun(RK_TRACE_OVERRUN_RELEASE, period,
                                (elapsed - period),
                                (ULONG)(offsetFactor - 1U));
    }
    RK_TICK offset = (RK_TICK)(offsetFactor * period);
    RK_TICK nextWake = K_TICK_ADD(baseWake, offset);
    RK_TICK delay = 0;
    delay = K_TICK_DELTA(nextWake, current);
    if (delay == 0)
    {
#ifndef NDEBUG

        {
            K_PANIC("0 DELAY SLEEPRELEASE\r\n");
            return (RK_ERR_ERROR);
        }
#else
        {
            /* this is not supposed to happen, but a possible fallback is
            to set reassign delay to remaining time next phase */

            /* get remainder */
            RK_TICK rem = current - (current / period) * period;
            delay = (rem == 0UL) ? period : (period - rem);
            nextWake = K_TICK_ADD(current, delay);
        }
#endif
    }
    RK_gRunPtr->wakeTime = nextWake;
    RK_TASK_SLEEP_TIMEOUT_SETUP
    RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, delay);
    if (err != RK_ERR_SUCCESS)
    {
        kTimeoutNodeReset(&RK_gRunPtr->timeoutNode);
        RK_CR_EXIT
        return (err);
    }
    RK_gRunPtr->status = RK_SLEEPING_RELEASE;
    kPendCtxSwtch();
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
