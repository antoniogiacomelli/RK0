/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
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
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
RK_ERR kTaskFlagsGet(ULONG const, UINT const, ULONG *const, RK_TICK const);
RK_ERR kTaskFlagsSet(RK_TASK_HANDLE const, ULONG const);
RK_ERR kTaskFlagsClear(RK_TASK_HANDLE, ULONG const);
RK_ERR kTaskFlagsQuery(RK_TASK_HANDLE const, ULONG *const);

#ifdef __cplusplus
}
#endif

#endif
