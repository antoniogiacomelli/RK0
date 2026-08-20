/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: ASYNCHRONOUS DIRECT MESSAGE                                     */
/******************************************************************************/

#ifndef RK_MESG_H
#define RK_MESG_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if ((RK_CONF_ASYNCH_MESG == ON) && (RK_CONF_MESG_QUEUE == ON))
RK_ERR kMesgEndpointInit(RK_TASK_HANDLE const);
RK_ERR kMesgPoolInit(RK_MEM_PARTITION *const, VOID *const, ULONG const,
                     ULONG const, RK_PRIO const);
RK_ERR kMesgAlloc(RK_MEM_PARTITION *const, RK_MESG **const, RK_TICK const);
RK_ERR kMesgFree(RK_MESG *const);
VOID *kMesgPayload(RK_MESG *const);
VOID const *kMesgPayloadConst(RK_MESG const *const);
ULONG kMesgPayloadBytes(RK_MESG const *const);
RK_TASK_HANDLE kMesgGetSenderHandle(RK_MESG const *const);
RK_ERR kMesgGetSenderID(RK_MESG const *const, RK_TID *const);
RK_ERR kMesgSend(RK_TASK_HANDLE const, RK_MESG *const);
RK_ERR kMesgWait(RK_TASK_HANDLE const, RK_MESG **const, RK_TICK const);
#endif /* RK_CONF_ASYNCH_MESG && RK_CONF_MESG_QUEUE */

#ifdef __cplusplus
}
#endif

#endif /* RK_MESG_H */
