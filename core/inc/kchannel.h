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

#ifndef RK_CHANNEL_H
#define RK_CHANNEL_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

RK_BEGIN_DECLS

#if (RK_CONF_CHANNEL == ON)
RK_ERR kChannelCallCore(RK_TASK_HANDLE const, VOID *const, VOID *const,
                        ULONG const, RK_TICK const);
#define kChannelCall(serverTask, reqPtr, respPtr, size, timeout)              \
        kChannelCallCore((serverTask), (reqPtr), (respPtr), (size), (timeout))

RK_ERR kChannelAcceptCore(RK_CALL_DATA *const, RK_TICK const);
#define kChannelAccept(callPtr, timeout)                                      \
        kChannelAcceptCore((callPtr), (timeout))

RK_ERR kChannelDoneCore(RK_CALL_DATA const *const);
#define kChannelDone(callPtr)                                                 \
        kChannelDoneCore((callPtr))
#endif

RK_END_DECLS

#endif /* RK_CHANNEL_H */
