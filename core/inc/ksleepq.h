/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                           */
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

RK_BEGIN_DECLS

#if (RK_CONF_SLEEP_QUEUE == ON)
RK_ERR kSleepQueueInitCore(RK_SLEEP_QUEUE* const);
#define kSleepQueueInit(kobj)                                                 \
        kSleepQueueInitCore((kobj))

RK_ERR kSleepQueueWaitCore(RK_SLEEP_QUEUE* const, RK_TICK const);
#define kSleepQueueWait(kobj, timeout)                                        \
        kSleepQueueWaitCore((kobj), (timeout))

RK_ERR kSleepQueueSignalCore(RK_SLEEP_QUEUE* const);
#define kSleepQueueSignal(kobj)                                               \
        kSleepQueueSignalCore((kobj))

RK_ERR kSleepQueueReadyCore(RK_SLEEP_QUEUE* const, RK_TASK_HANDLE);
#define kSleepQueueReady(kobj, taskHandle)                                    \
        kSleepQueueReadyCore((kobj), (taskHandle))

RK_ERR kSleepQueueQueryCore(RK_SLEEP_QUEUE const* const, ULONG* const);
#define kSleepQueueQuery(kobj, nTasksPtr)                                     \
        kSleepQueueQueryCore((kobj), (nTasksPtr))

RK_ERR kSleepQueueWakeCore(RK_SLEEP_QUEUE* const, UINT, UINT*);
#define kSleepQueueWake(kobj, nTasks, uTasksPtr)                              \
        kSleepQueueWakeCore((kobj), (nTasks), (uTasksPtr))

RK_ERR kSleepQueueBlockReadyTaskCore(RK_SLEEP_QUEUE* const, RK_TASK_HANDLE);
#define kSleepQueueBlockReadyTask(kobj, taskHandle)                           \
        kSleepQueueBlockReadyTaskCore((kobj), (taskHandle))
#endif

RK_END_DECLS

#endif
