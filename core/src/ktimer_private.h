/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_TIMER_PRIVATE_H
#define RK_TIMER_PRIVATE_H

#include <ktimer.h>
#include <ktrace.h>

#if (RK_CONF_CALLOUT_TIMER == ON)

static inline RK_ERR kTimerListAdd_(RK_TIMER *kobj, RK_TICK phase,
                                    RK_TICK countTicks, RK_TIMER_CALLOUT funPtr,
                                    VOID *argsPtr, RK_OPTION reload)
{
    RK_TICK const initialDelay = phase + countTicks;

    kobj->timeoutNode.dtick = initialDelay;
    kobj->timeoutNode.timeout = initialDelay;
    kobj->timeoutNode.timeoutType = RK_TIMEOUT_CALL;
    kobj->funPtr = funPtr;
    kobj->argsPtr = argsPtr;
    kobj->reload = reload;
    kobj->phase = phase;
    kobj->period = countTicks;
    kobj->nextTime = K_TICK_ADD(kTickGet(), phase + countTicks);
    return (kTimeoutNodeAdd(&kobj->timeoutNode, initialDelay));
}

#endif /* RK_CONF_CALLOUT_TIMER */

#endif /* RK_TIMER_PRIVATE_H */
