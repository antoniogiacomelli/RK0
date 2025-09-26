/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_MESGQ_H
#define RK_MESGQ_H

#include <kenv.h>
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_MESG_QUEUE == ON)
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const, VOID *const, ULONG const, ULONG const);
RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const, RK_TASK_HANDLE const);
RK_ERR kMesgQueueSetServer(RK_MESG_QUEUE *const kobj, RK_TASK_HANDLE const owner);
RK_ERR kMesgQueueServerDone(RK_MESG_QUEUE *const);
RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);
RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const, UINT *const);
RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                  const RK_TICK timeout);
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);


#ifdef __cplusplus
}

#endif
#endif
#endif
