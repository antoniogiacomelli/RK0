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
#ifndef RK_TASKFLAGS_H
#define RK_TASKFLAGS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
RK_ERR kTaskEventGet(ULONG const, UINT const, ULONG *const, RK_TICK const);
RK_ERR kTaskEventSet(RK_TASK_HANDLE const, ULONG const);
RK_ERR kTaskEventClear(RK_TASK_HANDLE, ULONG const);
RK_ERR kTaskEventQuery(RK_TASK_HANDLE const, ULONG *const);

#ifdef __cplusplus
}
#endif

#endif
