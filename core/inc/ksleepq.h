/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.19                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_SLEEPQ_H
#define RK_SLEEPQ_H
#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_SLEEP_QUEUE == ON)
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const);
RK_ERR kSleepQueueWait(RK_SLEEP_QUEUE *const, RK_TICK const);
RK_ERR kSleepQueueSignal(RK_SLEEP_QUEUE *const);
RK_ERR kSleepQueueReady(RK_SLEEP_QUEUE *const, RK_TASK_HANDLE);
RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const *const, ULONG *const);
RK_ERR kSleepQueueWake(RK_SLEEP_QUEUE *const, UINT, UINT *);
RK_ERR kSleepQueueSuspend(RK_SLEEP_QUEUE *const, RK_TASK_HANDLE);
#endif

#ifdef __cplusplus
}
#endif

#endif
