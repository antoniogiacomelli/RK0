/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_MESGQ_H
#define RK_MESGQ_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_MESG_QUEUE == ON)
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const, VOID *const, ULONG const,
                      ULONG const);
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kMesgQueueCreate(RK_MESG_QUEUE_HANDLE *const, VOID *const, ULONG const,
                        ULONG const);
RK_ERR kMesgQueueDestroy(RK_MESG_QUEUE_HANDLE *const);
#ifndef kMboxCreate
#define kMboxCreate kMesgQueueCreate
#endif
#ifndef kMboxDestroy
#define kMboxDestroy kMesgQueueDestroy
#endif
#endif
RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const, VOID *const);
RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);
RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const, UINT *const,
                       UINT *const, UINT *const);
#ifndef kMesgQueueQueryMessageCount
#define kMesgQueueQueryMessageCount(KOBJ, N_MESG_PTR)\
        kMesgQueueQuery((KOBJ), (N_MESG_PTR), (NULL), (NULL))
#endif
#ifndef kMesgQueueQueryWaitingReceivers
#define kMesgQueueQueryWaitingReceivers(KOBJ, N_WAIT_R_PTR)\
        kMesgQueueQuery((KOBJ), (NULL), (N_WAIT_R_PTR), (NULL))
#endif
#ifndef kMesgQueueQueryWaitingSenders
#define kMesgQueueQueryWaitingSenders(KOBJ, N_WAIT_S_PTR)\
        kMesgQueueQuery((KOBJ), (NULL), (NULL), (N_WAIT_S_PTR))
#endif
#ifndef kMboxQuery
#define kMboxQuery kMesgQueueQuery
#endif
#ifndef kMboxQueryMessageCount
#define kMboxQueryMessageCount(KOBJ, N_MESG_PTR)\
        kMesgQueueQueryMessageCount((KOBJ), (N_MESG_PTR))
#endif
#ifndef kMboxQueryWaitingReceivers
#define kMboxQueryWaitingReceivers(KOBJ, N_WAIT_R_PTR)\
        kMesgQueueQueryWaitingReceivers((KOBJ), (N_WAIT_R_PTR))
#endif
#ifndef kMboxQueryWaitingSenders
#define kMboxQueryWaitingSenders(KOBJ, N_WAIT_S_PTR)\
        kMesgQueueQueryWaitingSenders((KOBJ), (N_WAIT_S_PTR))
#endif
RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                     const RK_TICK timeout);
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);
RK_ERR kMesgQueueBroadcast(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                           UINT *const nRecvPtr);
RK_ERR kMesgQueueBroadcastWake(RK_MESG_QUEUE *const kobj, UINT const nTasks);
RK_ERR kMesgQueueBroadcastRecv(RK_MESG_QUEUE *const kobj,
                               VOID *const recvPtr,
                               const RK_TICK timeout);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

RK_ERR kMesgQueueInstallSendCbk(RK_MESG_QUEUE *const kobj,
                                VOID (*cbk)(RK_MESG_QUEUE *));
#endif

/* Message Queue Helpers */
#ifndef RK_MESGQ_MESG_SIZE
#define RK_MESGQ_MESG_SIZE(MESG_TYPE)\
        RK_TYPE_SIZE_POW2_WORDS(MESG_TYPE)
#ifndef RK_MBOX_MESG_SIZE
#define RK_MBOX_MESG_SIZE(MESG_TYPE) RK_MESGQ_MESG_SIZE(MESG_TYPE)
#endif
#endif

#ifndef RK_MESGQ_BUF_SIZE
#define RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)\
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
#define RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE, N_MESG)\
        ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);

#ifndef RK_DECLARE_MBOX_BUF
#define RK_DECLARE_MBOX_BUF(BUFNAME, MESG_TYPE)\
        RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE, 1U)
#endif
#endif

#ifndef RK_DECLARE_MESG_QUEUE
#define RK_DECLARE_MESG_QUEUE(QUEUE_NAME, BUFNAME, MESG_TYPE, N_MESG)\
    RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE,N_MESG)\
    RK_MESG_QUEUE QUEUE_NAME;
#endif

#ifndef RK_DECLARE_MBOX
#define RK_DECLARE_MBOX(MBOX_NAME, BUFNAME, MESG_TYPE)\
    RK_DECLARE_MBOX_BUF(BUFNAME, MESG_TYPE)\
    RK_MBOX MBOX_NAME;
#endif

#endif /* RK_CONF_MESG_QUEUE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RK_MESGQ_H */
