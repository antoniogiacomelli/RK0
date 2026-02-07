/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.11                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_MUTEX_H
#define RK_MUTEX_H

#include <kenv.h>
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_MUTEX == ON)
RK_ERR kMutexInit(RK_MUTEX *const, UINT);
RK_ERR kMutexLock(RK_MUTEX *const, RK_TICK const);
RK_ERR kMutexUnlock(RK_MUTEX *const);
RK_ERR kMutexQuery(RK_MUTEX const *const, UINT *const);
#endif

#ifdef __cplusplus
}
#endif

#endif
