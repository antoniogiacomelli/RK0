/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.5                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
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
RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);
RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const, UINT *const);
RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                  const RK_TICK timeout);
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);

#if (RK_CONF_PORTS == ON)
RK_ERR kMesgQueueSetServer(RK_MESG_QUEUE *const kobj, RK_TASK_HANDLE const owner);
RK_ERR kMesgQueueServerDone(RK_MESG_QUEUE *const);
#endif


/* Message Queue Helpers */
#if (RK_CONF_MESG_QUEUE == ON)
#ifndef RK_MESGQ_MESG_SIZE
#define RK_MESGQ_MESG_SIZE(MESG_TYPE) \
    RK_TYPE_SIZE_POW2_WORDS(MESG_TYPE)
#endif

#ifndef RK_MESGQ_BUF_SIZE
#define RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG) \
    (UINT)((RK_MESGQ_MESG_SIZE(MESG_TYPE)) * (N_MESG))
#endif
/**
 * @brief Declares the appropriate buffer to be used
 *        by a Message Queue.
 * @param BUFNAME Name of the array.
 * @param MESG_TYPE Type of the message.
 * @param N_MESG   Number of messages       
 *
 */
#ifndef RK_DECLARE_MESG_QUEUE_BUF
#define RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE, N_MESG) \
    ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif

#if (RK_CONF_PORTS == ON)
/**
 * @brief Declares the appropriate buffer to be used
 *        by a PORT.
 * 
 * @param BUFNAME.  Buffer name
 * @param MESG_TYPE  RK_PORT_MESG_2WORDS, RK_PORT_MESG_4WORDS,
 *                  RK_PORT_MESG_8WORDS, RK_PORT_MESG_COOKIE
 * @param N_MESG   Number of messages       
 *
 */

#ifndef RK_DECLARE_PORT_BUF
#define RK_DECLARE_PORT_BUF(BUFNAME, MESG_TYPE, N_MESG) \
    ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif

#ifndef RK_PORT_MSG_META_WORDS
#define RK_PORT_MSG_META_WORDS RK_PORT_META_WORDS
#endif
#endif 
#ifdef __cplusplus
}

#endif
#endif
#endif
#endif