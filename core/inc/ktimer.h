/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.7                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_TIMER_H
#define RK_TIMER_H

#include <kdefs.h>
#include <kobjs.h>
#include <kcommondefs.h>

#if (RK_CONF_CALLOUT_TIMER == ON)
/* Timer Reload / Oneshot optionss */
#define RK_TIMER_RELOAD 1U
#define RK_TIMER_ONESHOT 0U
RK_ERR kTimerInit(RK_TIMER *, RK_TICK, RK_TICK, RK_TIMER_CALLOUT, VOID *, UINT);
VOID kRemoveTimerNode(RK_TIMEOUT_NODE *);
VOID kTimerReload(RK_TIMER *, RK_TICK);
#endif

extern volatile RK_TIMEOUT_NODE *RK_gTimeOutListHeadPtr;
extern volatile RK_TIMEOUT_NODE *RK_gTimerListHeadPtr;
RK_ERR kTimeoutNodeAdd(RK_TIMEOUT_NODE *, RK_TICK);
UINT kHandleTimeoutList(VOID);
RK_ERR kRemoveTimeoutNode(RK_TIMEOUT_NODE *);
extern volatile struct RK_OBJ_RUNTIME RK_gRunTime; /* record of run time */
RK_ERR kSleepDelay(RK_TICK const);
RK_TICK kTickGet(VOID);
RK_ERR kSleepPeriodic(RK_TICK const);
RK_TICK kTickGetMs(VOID);
RK_ERR kSleepUntil(RK_TICK *, RK_TICK const);

RK_FORCE_INLINE
static inline unsigned kTickIsElapsed(RK_TICK then, RK_TICK now)
{
    return (((RK_STICK)(now - then)) >= 0);
}
#define K_TICK_ADD(base, offset) (RK_TICK)((base + offset))

/* signed diff helper */
#define K_TICK_DIFF(a, b)   ((RK_STICK)((RK_TICK)(a) - (RK_TICK)(b)))

/* linux-like wrap-safe ordering (half-range) */
#define K_TICK_IS_AFTER(a, b)      (K_TICK_DIFF((a), (b)) >  0)
#define K_TICK_IS_AFTER_EQ(a, b)   (K_TICK_DIFF((a), (b)) >= 0)
#define K_TICK_IS_BEFORE(a, b)     (K_TICK_DIFF((a), (b)) <  0)
#define K_TICK_IS_BEFORE_EQ(a, b)  (K_TICK_DIFF((a), (b)) <= 0)

/* expired */
#define K_TICK_EXPIRED(deadline)   K_TICK_IS_AFTER_EQ(kTickGet(), (deadline))

/* Delta magnitude */
#define K_TICK_DELTA(to, from)     ((RK_TICK)((RK_TICK)(to) - (RK_TICK)(from)))
#endif