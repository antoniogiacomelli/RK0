/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
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

#include <kenv.h>
#include <kdefs.h>
#include <kobjs.h>
#include <kcommondefs.h>

#if (RK_CONF_CALLOUT_TIMER == ON)
/* Timer Reload / Oneshot optionss */
#define RK_TIMER_RELOAD 1U
#define RK_TIMER_ONESHOT 0U
RK_ERR kTimerInit(RK_TIMER *, RK_TICK, RK_TICK, RK_TIMER_CALLOUT, VOID *, BOOL);
extern RK_TIMER *currTimerPtr;
VOID kRemoveTimerNode(RK_TIMEOUT_NODE *);
VOID kTimerReload(RK_TIMER *, RK_TICK);
#endif

extern volatile RK_TIMEOUT_NODE *timeOutListHeadPtr;
extern volatile RK_TIMEOUT_NODE *timerListHeadPtr;
RK_ERR kTimeOut(RK_TIMEOUT_NODE *, RK_TICK);
BOOL kHandleTimeoutList(VOID);
VOID kRemoveTimeoutNode(RK_TIMEOUT_NODE *);
extern volatile struct RK_OBJ_RUNTIME runTime; /* record of run time */
RK_ERR kSleepDelay(RK_TICK const);
RK_TICK kTickGet(VOID);
RK_ERR kSleepPeriod(RK_TICK const);
RK_TICK kTickGetMs(VOID);

RK_FORCE_INLINE
static inline unsigned kTickIsElapsed(RK_TICK then, RK_TICK now)
{
    return (((RK_STICK)(now - then)) >= 0);
}
#define K_TICK_EXPIRED(deadline) kTickIsElapsed(deadline, kTickGet())
#define K_TICK_ADD(base, offset) (RK_TICK)((base + offset))
#define K_TICK_DELAY(to, from) ((RK_TICK)(to - from))

#endif
