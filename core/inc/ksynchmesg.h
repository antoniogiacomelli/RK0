/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.70.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_SYNCH_MESG_H
#define RK_SYNCH_MESG_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>


#ifdef __cplusplus
extern "C" {
#endif
#if (RK_CONF_SYNCH_MESG == ON)
RK_ERR kSyncRecv(VOID *const, ULONG *const, RK_TICK const);
RK_ERR kSynchSendWait(RK_TASK_HANDLE const, VOID const *const, ULONG const,
                      RK_TICK const);
RK_ERR kSynchMesgInit(RK_TASK_HANDLE const, ULONG const);
#endif
#ifdef __cplusplus
}
#endif

#endif /* RK_SYNCH_MESG_H */
