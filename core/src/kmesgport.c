/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.19                                                           */
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

static inline RK_PORT_MSG_META *kPortMsgMeta_(ULONG *msgWordsPtr)
{
    return (RK_PORT_MSG_META *)(void *)msgWordsPtr;
}

static inline RK_PORT_MSG_META const *kPortMsgMetaConst_(ULONG const *msgWordsPtr)
{
    return (RK_PORT_MSG_META const *)(void const *)msgWordsPtr;
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
    meta->replyBox = NULL;

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
                     RK_MAILBOX *const replyBoxPtr,
                     UINT *const replyCodePtr,
                     const RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWordsPtr == NULL) || (replyBoxPtr == NULL) ||
        (replyCodePtr == NULL))
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

    if (replyBoxPtr->box.init == RK_FALSE)
    {
        RK_ERR err = kMailboxInit(replyBoxPtr);
        if (err != RK_ERR_SUCCESS)
        {
            return (err);
        }
    }
    if ((replyBoxPtr->box.ownerTask != NULL) && (replyBoxPtr->box.ownerTask != RK_gRunPtr))
    {
        return (RK_ERR_MESGQ_HAS_OWNER);
    }

    /* consume stale mailbox content from previous timed-out transactions. */
    if (replyBoxPtr->box.mesgCnt > 0U)
    {
        UINT staleReplyCode = 0U;
        kMailboxPend(replyBoxPtr, &staleReplyCode, RK_NO_WAIT);
    }

    RK_PORT_MSG_META *meta = kPortMsgMeta_(msgWordsPtr);
    meta->senderHandle = RK_gRunPtr;
    meta->replyBox = &replyBoxPtr->box;

    RK_ERR err = kMesgQueueSend(kobj, msgWordsPtr, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    err = kMailboxPend(replyBoxPtr, replyCodePtr, timeout);
    if (err == RK_ERR_TIMEOUT)
    {
        /*
         * Reserve reply mailbox with a stale marker so late server replies
         * fail fast instead of blocking. Next call clears stale content.
         */
        UINT timeoutMarker = 0U;
        kMailboxPost(replyBoxPtr, &timeoutMarker, RK_NO_WAIT);
    }

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
    RK_MESG_QUEUE *replyBoxQueue = meta->replyBox;

#if (RK_CONF_ERR_CHECK == ON)
    if (replyBoxQueue == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif

    RK_MAILBOX *replyBox = K_GET_CONTAINER_ADDR(replyBoxQueue, RK_MAILBOX, box);
    UINT code = replyCode;
    /* must not block the server indefinitely     */
    return (kMailboxPost(replyBox, &code, RK_NO_WAIT));
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
