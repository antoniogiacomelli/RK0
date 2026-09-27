/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.84.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_EXCHG_H
#define RK_EXCHG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#if (RK_CONF_EXCHG == ON)
RK_ERR kExchangeInit(RK_EXCHANGE *const, VOID *const);
RK_ERR kExchangePend(RK_EXCHANGE *const, VOID **const, RK_TICK const);
RK_ERR kExchangePost(RK_EXCHANGE *const, VOID *const, RK_TICK const);
RK_ERR kExchangePeek(RK_EXCHANGE const *const, VOID **const);
RK_ERR kExchangeOverwrite(RK_EXCHANGE *const, VOID *const);
RK_ERR kExchangeQuery(RK_EXCHANGE const *const, VOID **const, UINT *const);
#if (RK_CONF_EXCHG_BROADCAST == ON)
RK_ERR kExchangeBroadcast(RK_EXCHANGE *const, VOID *const, UINT *const);
#endif
#ifndef kExchangeAccept
#define kExchangeAccept(KOBJ, MESG_PPTR)                                  \
    kExchangePend((KOBJ), (MESG_PPTR), RK_NO_WAIT)
#endif
#ifndef kExchangePostOvw
#define kExchangePostOvw kExchangeOverwrite
#endif
#ifndef RK_DECLARE_EXCHG
#define RK_DECLARE_EXCHG(EXCHG_NAME) RK_EXCHANGE EXCHG_NAME;
#endif
#endif /* RK_CONF_EXCHG */

#ifdef __cplusplus
}
#endif

#endif
