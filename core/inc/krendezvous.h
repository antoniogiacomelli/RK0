/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.60.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_RENDEZVOUS_H
#define RK_RENDEZVOUS_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>


#ifdef __cplusplus
extern "C" {
#endif
#if (RK_CONF_RENDEZVOUS == ON)
RK_ERR kRendezvousRecv(VOID *const, ULONG *const, RK_TICK const);
RK_ERR kRendezvousSend(RK_TASK_HANDLE const, VOID const *const, ULONG const,
                       RK_TICK const);
RK_ERR kRendezvousInit(RK_TASK_HANDLE const, ULONG const);
#endif
#ifdef __cplusplus
}
#endif

#endif /* RK_RENDEZVOUS_H */
