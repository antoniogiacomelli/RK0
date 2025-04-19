/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv6/7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 * Header for the Module: TIMERS
 * 
 ******************************************************************************/


#ifndef RK_TIMER_H
#define RK_TIMER_H

#if (RK_CONF_CALLOUT_TIMER==(ON))
/* Timer Reload / Oneshot optionss */
#define RK_TIMER_RELOAD      1
#define RK_TIMER_ONESHOT     0

BOOL kTimerHandler( VOID*);
RK_ERR kTimerInit( RK_TIMER*, RK_TICK, RK_TICK, RK_TIMER_CALLOUT, VOID *, BOOL);
extern RK_TIMER* currTimerPtr;
VOID kRemoveTimerNode( RK_TIMEOUT_NODE *);

#endif

extern volatile RK_TIMEOUT_NODE *timeOutListHeadPtr;
extern volatile RK_TIMEOUT_NODE *timerListHeadPtr;
extern volatile RK_TIMEOUT_NODE *timerListHeadPtrSaved;

RK_ERR kTimeOut( RK_TIMEOUT_NODE*, RK_TICK);
BOOL kHandleTimeoutList( VOID);
VOID kRemoveTimeoutNode( RK_TIMEOUT_NODE*);
extern struct kRunTime runTime; /* record of run time */

#define BUSY(t) kBusyDelay(t)

RK_ERR kSleep( RK_TICK const);

#define SLEEP(t) kSleep(t)

RK_TICK kTickGet( VOID);

#if (RK_CONF_SCH_TSLICE==OFF)

RK_ERR kSleepUntil( RK_TICK const);

#endif

#endif
