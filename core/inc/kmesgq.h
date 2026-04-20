/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.19.1                                                           */
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
RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const, VOID *const, RK_TICK const);
RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const, VOID *const);
RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);
RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const, UINT *const);
RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                     const RK_TICK timeout);
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

RK_ERR kMesgQueueInstallSendCbk(RK_MESG_QUEUE *const kobj,
                                VOID (*cbk)(RK_MESG_QUEUE *));
#endif

/* Message Queue Helpers */
#ifndef RK_MESGQ_MESG_SIZE
#define RK_MESGQ_MESG_SIZE(MESG_TYPE)\
        RK_TYPE_SIZE_POW2_WORDS(MESG_TYPE)
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
#endif

#ifndef RK_DECLARE_PORT_BUF
#define RK_DECLARE_PORT_BUF(BUFNAME, MESG_TYPE, N_MESG)\
        RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE, N_MESG)
#endif

RK_ERR kPortInit_(RK_MESG_QUEUE *const portPtr, VOID *const bufPtr,
                  ULONG const mesgWords, ULONG const depth,
                  RK_TASK_HANDLE const ownerTask);


#ifndef kPortInit
#define kPortInit(PORT_PTR, BUF_PTR, MESG_WORDS, DEPTH, OWNER_TASK)\
        kPortInit_((PORT_PTR), (BUF_PTR), (MESG_WORDS), (DEPTH),\
                   (OWNER_TASK))
#endif

#ifndef kPortSend
#define kPortSend(OWNER_TASK, SEND_PTR, TIMEOUT)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueueSend((OWNER_TASK)->queuePortPtr, (SEND_PTR), (TIMEOUT))))
#endif

#ifndef kPortJam
#define kPortJam(OWNER_TASK, SEND_PTR, TIMEOUT)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueueJam((OWNER_TASK)->queuePortPtr, (SEND_PTR), (TIMEOUT))))
#endif

#ifndef kPortPostOvw
#define kPortPostOvw(OWNER_TASK, SEND_PTR)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueuePostOvw((OWNER_TASK)->queuePortPtr, (SEND_PTR))))
#endif

#ifndef kPortRecv
#define kPortRecv(OWNER_TASK, RECV_PTR, TIMEOUT)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueueRecv((OWNER_TASK)->queuePortPtr, (RECV_PTR), (TIMEOUT))))
#endif

#ifndef kMesgSend
#define kMesgSend kPortSend
#endif

#ifndef kMesgRecv
#define kMesgRecv(RECV_PTR, TIMEOUT)\
        ((RK_gRunPtr->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueueRecv(RK_gRunPtr->queuePortPtr, (RECV_PTR), (TIMEOUT)))
#endif

#ifndef kPortReset
#define kPortReset(OWNER_TASK)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueueReset((OWNER_TASK)->queuePortPtr)))
#endif

#ifndef kPortPeek
#define kPortPeek(OWNER_TASK, RECV_PTR)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueuePeek((OWNER_TASK)->queuePortPtr, (RECV_PTR))))
#endif

#ifndef kPortQuery
#define kPortQuery(OWNER_TASK, N_MESG_PTR)\
        (((OWNER_TASK) == NULL) ? RK_ERR_OBJ_NULL :\
        (((OWNER_TASK)->queuePortPtr == NULL) ? RK_ERR_INVALID_OBJ :\
        kMesgQueueQuery((OWNER_TASK)->queuePortPtr, (N_MESG_PTR)))
#endif
#endif /* RK_CONF_MESG_QUEUE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RK_MESGQ_H */
