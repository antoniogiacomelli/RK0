/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.60.1                                                           */
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
RK_ERR kCondQueueInit(RK_COND_QUEUE* const);

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kCondQueueCreate(RK_COND_QUEUE_HANDLE *const);
RK_ERR kCondQueueDestroy(RK_COND_QUEUE_HANDLE *const);

#endif
RK_ERR kCondQueueSleep(RK_COND_QUEUE* const, RK_TICK const);
RK_ERR kCondQueueSignal(RK_COND_QUEUE* const);
RK_ERR kCondQueueReady(RK_COND_QUEUE* const, RK_TASK_HANDLE);
RK_ERR kCondQueueQuery(RK_COND_QUEUE const* const, ULONG* const);
RK_ERR kCondQueueWake(RK_COND_QUEUE* const, UINT, UINT*);
RK_ERR kCondQueueBlockReadyTask(RK_COND_QUEUE* const, RK_TASK_HANDLE);

#ifndef kCondQueueFlush
#define kCondQueueFlush(o) kCondQueueWake(o, 0, NULL)
#endif

#ifndef kSleepQueueInit
#define kSleepQueueInit kCondQueueInit
#endif
#ifndef kSleepQueueCreate
#define kSleepQueueCreate kCondQueueCreate
#endif
#ifndef kSleepQueueDestroy
#define kSleepQueueDestroy kCondQueueDestroy
#endif
#ifndef kSleepQueueWait
#define kSleepQueueWait kCondQueueSleep
#endif
#ifndef kSleepQueueSleep
#define kSleepQueueSleep kSleepQueueWait
#endif
#ifndef kSleepQueueSignal
#define kSleepQueueSignal kCondQueueSignal
#endif
#ifndef kSleepQueueReady
#define kSleepQueueReady kCondQueueReady
#endif
#ifndef kSleepQueueQuery
#define kSleepQueueQuery kCondQueueQuery
#endif
#ifndef kSleepQueueWake
#define kSleepQueueWake kCondQueueWake
#endif
#ifndef kSleepQueueFlush
#define kSleepQueueFlush(o) kCondQueueFlush(o)
#endif
#ifndef kSleepQueueBlockReadyTask
#define kSleepQueueBlockReadyTask kCondQueueBlockReadyTask
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
