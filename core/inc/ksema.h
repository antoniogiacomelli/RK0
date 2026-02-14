/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.16                                              */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_SEMA_H
#define RK_SEMA_H


#ifdef __cplusplus
extern "C" {
#endif

#include <kenv.h>
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#if (RK_CONF_SEMAPHORE == ON)
RK_ERR kSemaphoreInit(RK_SEMAPHORE *const, UINT const, UINT const);
RK_ERR kSemaphorePend(RK_SEMAPHORE *const, RK_TICK const);
RK_ERR kSemaphorePost(RK_SEMAPHORE *const);
RK_ERR kSemaphoreFlush(RK_SEMAPHORE *const);
RK_ERR kSemaphoreQuery(RK_SEMAPHORE const *const, INT *const);
#endif


#ifdef __cplusplus
}
#endif

#endif