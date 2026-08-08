/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_SEMA_H
#define RK_SEMA_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

RK_BEGIN_DECLS

#if (RK_CONF_SEMAPHORE == ON)
RK_ERR kSemaphoreInitCore(RK_SEMAPHORE *const, UINT const, UINT const);
#define kSemaphoreInit(kobj, initValue, maxValue)                             \
        kSemaphoreInitCore((kobj), (initValue), (maxValue))

RK_ERR kSemaphorePendCore(RK_SEMAPHORE *const, RK_TICK const);
#define kSemaphorePend(kobj, timeout)                                         \
        kSemaphorePendCore((kobj), (timeout))

RK_ERR kSemaphorePostCore(RK_SEMAPHORE *const);
#define kSemaphorePost(kobj)                                                  \
        kSemaphorePostCore((kobj))

RK_ERR kSemaphoreQueryCore(RK_SEMAPHORE const *const, INT *const);
#define kSemaphoreQuery(kobj, countPtr)                                       \
        kSemaphoreQueryCore((kobj), (countPtr))
#endif

RK_END_DECLS

#endif
