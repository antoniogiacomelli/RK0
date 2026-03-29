/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.16.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MESSAGE PORT                                                    */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kapi.h>

#if (RK_CONF_MESG_QUEUE == ON)
#if (RK_CONF_PORTS == ON)

static RK_MAILBOX RK_gPortReplyBoxes[RK_CONF_NTASKS];

static inline RK_PORT_MSG_META *kPortMsgMeta_(ULONG *msgWordsPtr)
{
    return (RK_PORT_MSG_META *)(void *)msgWordsPtr;
}

static inline RK_PORT_MSG_META const *kPortMsgMetaConst_(ULONG const *msgWordsPtr)
{
    return (RK_PORT_MSG_META const *)(void const *)msgWordsPtr;
}

/* ensure reply route is valid for the client now every client has its
implicit replyPtr on TCB */
static RK_ERR kPortReplyBoxEnsure_(RK_TASK_HANDLE const taskHandle,
                                   RK_MAILBOX **const replyBoxPPtr)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (taskHandle->pid >= RK_CONF_NTASKS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    RK_MAILBOX *replyBoxPtr = &RK_gPortReplyBoxes[taskHandle->pid];
    if (replyBoxPtr->box.init == RK_FALSE)
    {
        RK_ERR err = kMailboxInit(replyBoxPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
    }

    replyBoxPtr->box.ownerTask = taskHandle;
    *replyBoxPPtr = replyBoxPtr;
    return (RK_ERR_SUCCESS);
}

static void kPortAdoptSenderPrio_(RK_PORT *const kobj, ULONG const *const msgWordsPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (!kobj->isServer || (kobj->ownerTask == NULL))
    {
        RK_CR_EXIT
        return;
    }

    RK_PORT_MSG_META const *meta = kPortMsgMetaConst_(msgWordsPtr);
    RK_TCB *sender = meta->senderHandle;
    RK_PRIO newPrio = sender ? sender->priority : kobj->ownerTask->priority;

    if (kobj->ownerTask->priority == newPrio)
    {
        RK_CR_EXIT
        return;
    }

    if (kobj->ownerTask->status == RK_READY)
    {
        RK_ERR err = kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                              &kobj->ownerTask);
        K_ASSERT(!err);
        kobj->ownerTask->priority = newPrio;
        err = kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority], kobj->ownerTask);
        K_ASSERT(!err);
    }
    else
    {
        kobj->ownerTask->priority = newPrio;
    }

    RK_CR_EXIT
}

RK_ERR kMesgQueueSetServer(RK_MESG_QUEUE *const kobj, RK_TASK_HANDLE const owner)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    kobj->isServer = RK_TRUE;
    kobj->ownerTask = owner;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* finish transaction, retoring server priority if needed */
RK_ERR kMesgQueueServerDone(RK_MESG_QUEUE *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    if (!kobj->isServer || (kobj->ownerTask == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if (kobj->ownerTask->priority != kobj->ownerTask->prioNominal)
    {
        if (kobj->ownerTask->status == RK_READY)
        {
            RK_ERR err = kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                                  &kobj->ownerTask);
            K_ASSERT(!err);
            kobj->ownerTask->priority = kobj->ownerTask->prioNominal;
            err = kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority],
                           kobj->ownerTask);
            K_ASSERT(!err);
            kReschedTask(kobj->ownerTask);
        }
        else
        {
            kobj->ownerTask->priority = kobj->ownerTask->prioNominal;
        }
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kPortInit(RK_PORT *const kobj,
                 VOID *const buf,
                 const ULONG msgWords,
                 const ULONG nMesg,
                 RK_TASK_HANDLE const owner)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (msgWords < RK_PORT_META_WORDS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }
#endif

    RK_ERR err = kMesgQueueInit(kobj, buf, msgWords, nMesg);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    err = kMesgQueueSetOwner(kobj, owner);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    return (kMesgQueueSetServer(kobj, owner));
}

RK_ERR kPortSetServer(RK_PORT *const kobj, RK_TASK_HANDLE owner)
{
    return (kMesgQueueSetServer(kobj, owner));
}

RK_ERR kPortSend(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL || msg == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (kobj->mesgSize < RK_PORT_META_WORDS)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    RK_PORT_MSG_META *meta = kPortMsgMeta_((ULONG *)msg);
    meta->senderHandle = RK_gRunPtr;
    meta->replySeq = 0U;

    return (kMesgQueueSend(kobj, msg, timeout));
}

RK_ERR kPortRecv(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL || msg == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (kobj->mesgSize < RK_PORT_META_WORDS)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    RK_ERR err = kMesgQueueRecv(kobj, msg, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    kPortAdoptSenderPrio_(kobj, (ULONG const *)msg);
    return (RK_ERR_SUCCESS);
}

RK_ERR kPortServerDone(RK_PORT *const kobj)
{
    return (kMesgQueueServerDone(kobj));
}

RK_ERR kPortSendRecv(RK_PORT *const kobj,
                     ULONG *const msgWordsPtr,
                     UINT *const replyCodePtr,
                     const RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWordsPtr == NULL) || (replyCodePtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (kobj->mesgSize < RK_PORT_META_WORDS)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    if (timeout == RK_NO_WAIT)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        return (RK_ERR_INVALID_TIMEOUT);
    }

    if (RK_gRunPtr->replyPtr != NULL)
    {
        return (RK_ERR_MESGQ_FULL);
    }

    RK_MAILBOX *replyBoxPtr = NULL;
    RK_ERR err = kPortReplyBoxEnsure_(RK_gRunPtr, &replyBoxPtr);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    if (replyBoxPtr->box.mesgCnt > 0U)
    {
        UINT staleReplyCode = 0U;
        kMailboxPend(replyBoxPtr, &staleReplyCode, RK_NO_WAIT);
    }

    RK_gRunPtr->replySeq += 1U;
    if (RK_gRunPtr->replySeq == 0U)
    {
        RK_gRunPtr->replySeq = 1U;
    }
    RK_gRunPtr->replyPtr = replyBoxPtr;

    RK_PORT_MSG_META *meta = kPortMsgMeta_(msgWordsPtr);
    meta->senderHandle = RK_gRunPtr;
    meta->replySeq = RK_gRunPtr->replySeq;

    err = kMesgQueueSend(kobj, msgWordsPtr, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        RK_gRunPtr->replyPtr = NULL;
        return (err);
    }

    err = kMailboxPend(replyBoxPtr, replyCodePtr, timeout);
    if (err == RK_ERR_TIMEOUT)
    {
        kMailboxReset(replyBoxPtr);
        replyBoxPtr->box.ownerTask = RK_gRunPtr;
    }
    RK_gRunPtr->replyPtr = NULL;
    return (err);
}

RK_ERR kPortReply(RK_PORT *const kobj, ULONG const *const msgWordsPtr, const UINT replyCode)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWordsPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (kobj->mesgSize < RK_PORT_META_WORDS)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    RK_PORT_MSG_META const *meta = kPortMsgMetaConst_(msgWordsPtr);
    RK_TCB *sender = meta->senderHandle;

#if (RK_CONF_ERR_CHECK == ON)
    if ((sender == NULL) || (meta->replySeq == 0U))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (sender->pid >= RK_CONF_NTASKS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    if ((sender->replyPtr == NULL) || (sender->replySeq != meta->replySeq))
    {
        return (RK_ERR_MESGQ_FULL);
    }

    UINT code = replyCode;
    /* must not block the server indefinitely     */
    return (kMailboxPost(sender->replyPtr, &code, RK_NO_WAIT));
}

RK_ERR kPortReplyDone(RK_PORT *const kobj,
                      ULONG const *const msgWordsPtr,
                      const UINT replyCode)
{
    RK_ERR errPost = kPortReply(kobj, msgWordsPtr, replyCode);
    RK_ERR errDemote = kPortServerDone(kobj);
    return (errPost != RK_ERR_SUCCESS) ? (errPost) : (errDemote);
}

RK_ERR kPortSetOwner(RK_PORT *const kobj, RK_TASK_HANDLE const taskHandle)
{
    return (kMesgQueueSetOwner(kobj, taskHandle));
}

#endif /* RK_CONF_PORTS */
#endif /* RK_CONF_MESG_QUEUE */
