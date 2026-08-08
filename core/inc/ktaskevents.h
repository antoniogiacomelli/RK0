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

#ifndef RK_TASKFLAGS_H
#define RK_TASKFLAGS_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

RK_BEGIN_DECLS

RK_ERR kEventGetCore(RK_EVENT_FLAG const, RK_OPTION const,
                     RK_EVENT_FLAG* const, RK_TICK const);
#define kEventGet(required, options, gotFlagsPtr, timeout)                    \
        kEventGetCore((required), (options), (gotFlagsPtr), (timeout))

RK_ERR kEventSetCore(RK_TASK_HANDLE const, RK_EVENT_FLAG const);
#define kEventSet(taskHandle, mask)                                           \
        kEventSetCore((taskHandle), (mask))

RK_ERR kEventClearCore(RK_TASK_HANDLE, RK_EVENT_FLAG const);
#define kEventClear(taskHandle, flagsToClear)                                 \
        kEventClearCore((taskHandle), (flagsToClear))

RK_ERR kEventQueryCore(RK_TASK_HANDLE const, RK_EVENT_FLAG* const);
#define kEventQuery(taskHandle, gotFlagsPtr)                                  \
        kEventQueryCore((taskHandle), (gotFlagsPtr))

RK_END_DECLS

#endif
