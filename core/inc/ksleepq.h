/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.1                                                           */
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
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE* const);

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kSleepQueueCreate(RK_SLEEP_QUEUE_HANDLE *const);
RK_ERR kSleepQueueDestroy(RK_SLEEP_QUEUE_HANDLE *const);

#endif
RK_ERR kSleepQueueSleep(RK_SLEEP_QUEUE* const, RK_TICK const);
RK_ERR kSleepQueueSignal(RK_SLEEP_QUEUE* const);
RK_ERR kSleepQueueReady(RK_SLEEP_QUEUE* const, RK_TASK_HANDLE);
RK_ERR kSleepQueueUnready(RK_SLEEP_QUEUE* const, RK_TASK_HANDLE);
RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const* const, ULONG* const);
RK_ERR kSleepQueueWake(RK_SLEEP_QUEUE* const, UINT, UINT*);

#ifndef kSleepQueueFlush
#define kSleepQueueFlush(o) kSleepQueueWake(o, 0, NULL)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
