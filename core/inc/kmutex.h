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

/******************************************************************************/
#ifndef RK_MUTEX_H
#define RK_MUTEX_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

RK_BEGIN_DECLS

#if (RK_CONF_MUTEX == ON)
RK_ERR kMutexInitCore(RK_MUTEX *const, UINT);
#define kMutexInit(kobj, protocol)                                            \
        kMutexInitCore((kobj), (protocol))

RK_ERR kMutexLockCore(RK_MUTEX *const, RK_TICK const);
#define kMutexLock(kobj, timeout)                                             \
        kMutexLockCore((kobj), (timeout))

RK_ERR kMutexUnlockCore(RK_MUTEX *const);
#define kMutexUnlock(kobj)                                                    \
        kMutexUnlockCore((kobj))

RK_ERR kMutexQueryCore(RK_MUTEX const *const, UINT *const);
#define kMutexQuery(kobj, statePtr)                                           \
        kMutexQueryCore((kobj), (statePtr))
#endif

RK_END_DECLS

#endif
