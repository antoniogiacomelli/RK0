/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.19.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_TASKFLAGS_H
#define RK_TASKFLAGS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
RK_ERR kEventGet(RK_EVENT_FLAG const, RK_OPTION const, RK_EVENT_FLAG* const, RK_TICK const);
RK_ERR kEventSet(RK_TASK_HANDLE const, RK_EVENT_FLAG const);
RK_ERR kEventClear(RK_TASK_HANDLE, RK_EVENT_FLAG const);
RK_ERR kEventQuery(RK_TASK_HANDLE const, RK_EVENT_FLAG* const);

#ifdef __cplusplus
}
#endif

#endif
