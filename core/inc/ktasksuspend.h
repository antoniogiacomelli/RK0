/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.82.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_TASKSUSPEND_H
#define RK_TASKSUSPEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

RK_ERR kTaskSelfSuspend(VOID);
RK_ERR kTaskResume(RK_TASK_HANDLE const taskHandle);

#ifdef __cplusplus
}
#endif

#endif /* RK_TASKSUSPEND_H */
