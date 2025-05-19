/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

RK_ERR kTimeOut( RK_TIMEOUT_NODE*, RK_TICK);
BOOL kHandleTimeoutList( VOID);
VOID kRemoveTimeoutNode( RK_TIMEOUT_NODE*);
extern volatile struct kRunTime runTime; /* record of run time */

RK_ERR kSleep( RK_TICK const);
RK_TICK kTickGet( VOID);
RK_WALL_TICK kWallclockGetTicks( VOID);

#if (RK_CONF_SCH_TSLICE==OFF)

RK_ERR kSleepUntil( RK_TICK const);

#endif

#endif
