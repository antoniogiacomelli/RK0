/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.30.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SYNCHRONOUS TASK-TO-TASK EXCHANGE                               */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kexchange.h>
#include <kapi.h>
#include <ktrace.h>

#if (RK_CONF_EXCHANGE == ON)

static inline VOID kExchangeClearSender_(RK_TCB *const senderPtr)
{
    senderPtr->exchangeMesgPtr = NULL;
    senderPtr->exchangeWaitPtr = NULL;
}

static inline VOID kExchangeDisarmTimeout_(RK_TCB *const taskPtr)
{
    if (kTimeoutNodeIsArmed(&taskPtr->timeoutNode) == RK_TRUE)
    {
        RK_ERR err = kTimeoutNodeDisarm(&taskPtr->timeoutNode);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    else
    {
        kTimeoutNodeReset(&taskPtr->timeoutNode);
    }
}

static VOID kExchangeDirectRecv_(RK_TCB *const receiverPtr,
                                 RK_EXCHANGE *const receiverExchangePtr,
                                 VOID *const mesgPtr)
{
    K_ASSERT(receiverExchangePtr->exchangeRecvStorePtr != NULL);

    *receiverExchangePtr->exchangeRecvStorePtr = mesgPtr;
    receiverExchangePtr->exchangeRecvStorePtr = NULL;

    if (receiverPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_RECV)
    {
        kExchangeDisarmTimeout_(receiverPtr);
    }
    kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_RECV,
                       RK_ERR_SUCCESS, 0UL);
    kReadySwtch(receiverPtr);
}

static VOID kExchangePromoteNext_(RK_TCB *const receiverPtr)
{
    RK_EXCHANGE *const receiverExchangePtr = receiverPtr->exchangePtr;
    if ((receiverExchangePtr == NULL) ||
        (receiverExchangePtr->inboxMesgPtr != NULL))
    {
        return;
    }

    if (receiverExchangePtr->waitingSenders.size > 0U)
    {
        RK_TCB *senderPtr = kTCBQPeek(&receiverExchangePtr->waitingSenders);
        receiverExchangePtr->inboxMesgPtr = senderPtr->exchangeMesgPtr;
        receiverExchangePtr->exchangePeerPtr = senderPtr;
    }
}

static RK_ERR kExchangeConsumeInbox_(RK_TCB *const receiverPtr,
                                     VOID **const mesgPPtr)
{
    RK_EXCHANGE *const receiverExchangePtr = receiverPtr->exchangePtr;
    K_ASSERT(receiverExchangePtr != NULL);
    K_ASSERT(receiverExchangePtr->inboxMesgPtr != NULL);

    RK_TCB *senderPtr = receiverExchangePtr->exchangePeerPtr;
    K_ASSERT(senderPtr != NULL);

    *mesgPPtr = receiverExchangePtr->inboxMesgPtr;
    receiverExchangePtr->inboxMesgPtr = NULL;
    receiverExchangePtr->exchangePeerPtr = NULL;

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverExchangePtr->waitingSenders, &remPtr);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    if (senderPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_SEND)
    {
        kExchangeDisarmTimeout_(senderPtr);
    }

    kExchangeClearSender_(senderPtr);
    err = kReadySwtch(senderPtr);
    if (err < 0)
    {
        return (err);
    }

    kExchangePromoteNext_(receiverPtr);
    kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_RECV,
                       RK_ERR_SUCCESS,
                       receiverExchangePtr->waitingSenders.size);
    return (RK_ERR_SUCCESS);
}

VOID kExchangeTimeoutSend(RK_TCB *const senderPtr)
{
    RK_EXCHANGE *const receiverExchangePtr = senderPtr->exchangeWaitPtr;
    if (receiverExchangePtr == NULL)
    {
        return;
    }

    RK_TCB *receiverPtr = receiverExchangePtr->ownerTask;
    if (receiverPtr == NULL)
    {
        kExchangeClearSender_(senderPtr);
        return;
    }

    if (receiverExchangePtr->exchangePeerPtr == senderPtr)
    {
        receiverExchangePtr->inboxMesgPtr = NULL;
        receiverExchangePtr->exchangePeerPtr = NULL;
    }

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverExchangePtr->waitingSenders, &remPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);

    kExchangeClearSender_(senderPtr);
    kExchangePromoteNext_(receiverPtr);
    kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_TIMEOUT,
                       RK_ERR_TIMEOUT,
                       receiverExchangePtr->waitingSenders.size);
}

RK_ERR kExchangeInit(RK_EXCHANGE *const kobj,
                     RK_TASK_HANDLE const taskHandle)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (taskHandle == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (taskHandle->exchangePtr != NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }

    if ((kobj->init == RK_TRUE) || (kobj->ownerTask != NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    kobj->objID = RK_EXCHANGE_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->init = RK_TRUE;
    kobj->inboxMesgPtr = NULL;
    kobj->exchangePeerPtr = NULL;
    kobj->ownerTask = taskHandle;
    kobj->exchangeRecvStorePtr = NULL;
    RK_ERR err = kTCBQInit(&kobj->waitingSenders);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    taskHandle->exchangePtr = kobj;
    kTraceRegisterObject(kobj, RK_EXCHANGE_KOBJ_ID);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kExchangeSend(RK_TASK_HANDLE const taskHandle, VOID *const mesgPtr,
                     RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((taskHandle == NULL) || (mesgPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (taskHandle->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (taskHandle == RK_gRunPtr)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_EXCHANGE *const receiverExchangePtr = taskHandle->exchangePtr;
    if ((receiverExchangePtr == NULL) ||
        (receiverExchangePtr->init != RK_TRUE) ||
        (receiverExchangePtr->ownerTask != taskHandle))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((receiverExchangePtr->exchangeRecvStorePtr != NULL) &&
        (receiverExchangePtr->inboxMesgPtr == NULL))
    {
        kExchangeDirectRecv_(taskHandle, receiverExchangePtr, mesgPtr);
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_SEND,
                           RK_ERR_SUCCESS, 0UL);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if (timeout == RK_NO_WAIT)
    {
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK,
                           RK_ERR_NOWAIT,
                           receiverExchangePtr->waitingSenders.size);
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gRunPtr->exchangeMesgPtr = mesgPtr;
    RK_gRunPtr->exchangeWaitPtr = receiverExchangePtr;

    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_SEND;
        RK_gRunPtr->timeoutNode.waitingQueuePtr =
            &receiverExchangePtr->waitingSenders;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kExchangeClearSender_(RK_gRunPtr);
            kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK, err,
                               receiverExchangePtr->waitingSenders.size);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_SENDING;
    RK_ERR enqErr =
        kTCBQEnqByPrio(&receiverExchangePtr->waitingSenders, RK_gRunPtr);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if (timeout != RK_WAIT_FOREVER)
        {
            kExchangeDisarmTimeout_(RK_gRunPtr);
        }
        kExchangeClearSender_(RK_gRunPtr);
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK, enqErr,
                           receiverExchangePtr->waitingSenders.size);
        RK_CR_EXIT
        return (enqErr);
    }
    kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK,
                       RK_ERR_SUCCESS,
                       receiverExchangePtr->waitingSenders.size);
    kExchangePromoteNext_(taskHandle);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_TIMEOUT,
                           RK_ERR_TIMEOUT,
                           receiverExchangePtr->waitingSenders.size);
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_SEND, RK_ERR_SUCCESS,
                       receiverExchangePtr->waitingSenders.size);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kExchangeRecv(VOID **const mesgPPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (mesgPPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    RK_EXCHANGE *const receiverExchangePtr = RK_gRunPtr->exchangePtr;
    if ((receiverExchangePtr == NULL) ||
        (receiverExchangePtr->init != RK_TRUE) ||
        (receiverExchangePtr->ownerTask != RK_gRunPtr))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    *mesgPPtr = NULL;

    if (receiverExchangePtr->inboxMesgPtr != NULL)
    {
        RK_ERR err = kExchangeConsumeInbox_(RK_gRunPtr, mesgPPtr);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK,
                           RK_ERR_BUFFER_EMPTY, 0UL);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    receiverExchangePtr->exchangeRecvStorePtr = mesgPPtr;
    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_RECV;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            receiverExchangePtr->exchangeRecvStorePtr = NULL;
            kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK, err,
                               0UL);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_RECEIVING;
    kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_BLOCK,
                       RK_ERR_SUCCESS, 1UL);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_TIMEOUT,
                           RK_ERR_TIMEOUT, 0UL);
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if (receiverExchangePtr->exchangeRecvStorePtr == NULL)
    {
        kTraceRecordObject(receiverExchangePtr, RK_TRACE_OP_RECV,
                           RK_ERR_SUCCESS, 0UL);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    RK_ERR err = kExchangeConsumeInbox_(RK_gRunPtr, mesgPPtr);
    receiverExchangePtr->exchangeRecvStorePtr = NULL;
    RK_CR_EXIT
    return (err);
}

#endif /* RK_CONF_EXCHANGE */
