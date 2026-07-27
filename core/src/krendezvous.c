/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SYNCHRONOUS TASK-TO-TASK RENDEZVOUS                               */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <krendezvous.h>
#include <kapi.h>
#include <kstring.h>
#include <ktrace.h>

#if (RK_CONF_RENDEZVOUS == ON)

static inline VOID kRendezvousClearSender_(RK_TCB *const senderPtr)
{
    senderPtr->rendezvousMesgPtr = NULL;
    senderPtr->rendezvousMesgBytes = 0UL;
    senderPtr->rendezvousWaitPtr = NULL;
}

static inline VOID kRendezvousClearReceiver_(RK_RENDEZVOUS *const kobj)
{
    kobj->rendezvousRecvBufPtr = NULL;
    kobj->rendezvousRecvBufBytes = 0UL;
    kobj->rendezvousRecvBytesPtr = NULL;
}

static inline RK_BOOL kRendezvousTaskOwnsMutex_(RK_TCB const *const taskPtr)
{
#if (RK_CONF_MUTEX == ON)
    return (((taskPtr != NULL) && (taskPtr->ownedMutexList.size > 0UL))
                ? RK_TRUE
                : RK_FALSE);
#else
    (VOID)taskPtr;
    return (RK_FALSE);
#endif
}

static inline VOID kRendezvousDisarmTimeout_(RK_TCB *const taskPtr)
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

static VOID kRendezvousUpdateOwnerPrio_(RK_RENDEZVOUS *const receiverRendezvousPtr)
{
    RK_TCB *ownerPtr = receiverRendezvousPtr->ownerTask;
    if (ownerPtr == NULL)
    {
        return;
    }

    RK_PRIO targetPrio = ownerPtr->prioNominal;
    if (receiverRendezvousPtr->waitingSenders.size > 0U)
    {
        RK_TCB *senderPtr =
            kTCBQPeek(&receiverRendezvousPtr->waitingSenders);
        K_ASSERT(senderPtr != NULL);
        if ((senderPtr != NULL) && (senderPtr->priority < targetPrio))
        {
            targetPrio = senderPtr->priority;
        }
    }

    if (targetPrio == ownerPtr->priority)
    {
        return;
    }

    if (ownerPtr->status == RK_READY)
    {
        RK_PRIO const oldPrio = ownerPtr->priority;
        RK_TCB *remPtr = ownerPtr;
        RK_ERR err = kTCBQRem(&RK_gReadyQueue[ownerPtr->priority], &remPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        ownerPtr->priority = targetPrio;
        kTraceRecordTaskPrio(ownerPtr, oldPrio, targetPrio);
        err = kTCBQEnq(&RK_gReadyQueue[ownerPtr->priority], ownerPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    else
    {
        RK_PRIO const oldPrio = ownerPtr->priority;
        ownerPtr->priority = targetPrio;
        kTraceRecordTaskPrio(ownerPtr, oldPrio, targetPrio);
    }
}

static RK_ERR kRendezvousCopy_(VOID *const recvPtr,
                             ULONG const recvBytes,
                             ULONG *const mesgBytesPtr,
                             VOID const *const mesgPtr,
                             ULONG const mesgBytes)
{
    if (mesgBytesPtr != NULL)
    {
        *mesgBytesPtr = mesgBytes;
    }
    if (recvBytes < mesgBytes)
    {
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    RK_MEMCPY(recvPtr, mesgPtr, (size_t)mesgBytes);
    return (RK_ERR_SUCCESS);
}

static RK_ERR kRendezvousDirectRecv_(RK_TCB *const receiverPtr,
                                   RK_RENDEZVOUS *const receiverRendezvousPtr,
                                   VOID const *const mesgPtr,
                                   ULONG const mesgBytes)
{
    K_ASSERT(receiverRendezvousPtr->rendezvousRecvBufPtr != NULL);

    RK_ERR const err =
        kRendezvousCopy_(receiverRendezvousPtr->rendezvousRecvBufPtr,
                         receiverRendezvousPtr->rendezvousRecvBufBytes,
                         receiverRendezvousPtr->rendezvousRecvBytesPtr,
                         mesgPtr, mesgBytes);
    receiverRendezvousPtr->rendezvousRecvStatus = err;

    kRendezvousClearReceiver_(receiverRendezvousPtr);

    if (receiverPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_RECV)
    {
        kRendezvousDisarmTimeout_(receiverPtr);
    }
    kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_RECV,
                       err, 0UL);
    kReadySwtch(receiverPtr);
    return (err);
}

static VOID kRendezvousPromoteNext_(RK_TCB *const receiverPtr)
{
    RK_RENDEZVOUS *const receiverRendezvousPtr = receiverPtr->rendezvousPtr;
    if ((receiverRendezvousPtr == NULL) ||
        (receiverRendezvousPtr->inboxMesgPtr != NULL))
    {
        return;
    }

    if (receiverRendezvousPtr->waitingSenders.size > 0U)
    {
        RK_TCB *senderPtr = kTCBQPeek(&receiverRendezvousPtr->waitingSenders);
        receiverRendezvousPtr->inboxMesgPtr = senderPtr->rendezvousMesgPtr;
        receiverRendezvousPtr->inboxMesgBytes = senderPtr->rendezvousMesgBytes;
        receiverRendezvousPtr->rendezvousPeerPtr = senderPtr;
    }
}

static RK_ERR kRendezvousConsumeInbox_(RK_TCB *const receiverPtr,
                                     VOID *const recvPtr,
                                     ULONG const recvBytes,
                                     ULONG *const mesgBytesPtr)
{
    RK_RENDEZVOUS *const receiverRendezvousPtr = receiverPtr->rendezvousPtr;
    K_ASSERT(receiverRendezvousPtr != NULL);
    K_ASSERT(receiverRendezvousPtr->inboxMesgPtr != NULL);

    RK_TCB *senderPtr = receiverRendezvousPtr->rendezvousPeerPtr;
    K_ASSERT(senderPtr != NULL);

    RK_ERR err = kRendezvousCopy_(recvPtr, recvBytes, mesgBytesPtr,
                                  receiverRendezvousPtr->inboxMesgPtr,
                                  receiverRendezvousPtr->inboxMesgBytes);
    RK_ERR const copyStatus = err;
    senderPtr->rendezvousStatus = err;

    receiverRendezvousPtr->inboxMesgPtr = NULL;
    receiverRendezvousPtr->inboxMesgBytes = 0UL;
    receiverRendezvousPtr->rendezvousPeerPtr = NULL;

    RK_TCB *remPtr = senderPtr;
    err = kTCBQRem(&receiverRendezvousPtr->waitingSenders, &remPtr);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    if (senderPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_SEND)
    {
        kRendezvousDisarmTimeout_(senderPtr);
    }

    kRendezvousClearSender_(senderPtr);

    kRendezvousPromoteNext_(receiverPtr);
    kRendezvousUpdateOwnerPrio_(receiverRendezvousPtr);

    err = kReadySwtch(senderPtr);
    if (err < 0)
    {
        return (err);
    }

    kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_RECV,
                       copyStatus, receiverRendezvousPtr->waitingSenders.size);
    return (copyStatus);
}

VOID kRendezvousTimeoutSend(RK_TCB *const senderPtr)
{
    RK_RENDEZVOUS *const receiverRendezvousPtr = senderPtr->rendezvousWaitPtr;
    if (receiverRendezvousPtr == NULL)
    {
        return;
    }

    RK_TCB *receiverPtr = receiverRendezvousPtr->ownerTask;
    if (receiverPtr == NULL)
    {
        kRendezvousClearSender_(senderPtr);
        return;
    }

    if (receiverRendezvousPtr->rendezvousPeerPtr == senderPtr)
    {
        receiverRendezvousPtr->inboxMesgPtr = NULL;
        receiverRendezvousPtr->inboxMesgBytes = 0UL;
        receiverRendezvousPtr->rendezvousPeerPtr = NULL;
    }

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverRendezvousPtr->waitingSenders, &remPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);

    kRendezvousClearSender_(senderPtr);
    kRendezvousPromoteNext_(receiverPtr);
    kRendezvousUpdateOwnerPrio_(receiverRendezvousPtr);
    kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_TIMEOUT,
                       RK_ERR_TIMEOUT,
                       receiverRendezvousPtr->waitingSenders.size);
}

RK_ERR kRendezvousInit(RK_RENDEZVOUS *const kobj,
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

    if (taskHandle->rendezvousPtr != NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }

    if ((kobj->init == RK_TRUE) || (kobj->ownerTask != NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    kobj->objID = RK_RENDEZVOUS_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->init = RK_TRUE;
    kobj->inboxMesgPtr = NULL;
    kobj->inboxMesgBytes = 0UL;
    kobj->rendezvousPeerPtr = NULL;
    kobj->ownerTask = taskHandle;
    kobj->rendezvousRecvBufPtr = NULL;
    kobj->rendezvousRecvBufBytes = 0UL;
    kobj->rendezvousRecvBytesPtr = NULL;
    kobj->rendezvousRecvStatus = RK_ERR_SUCCESS;
    RK_ERR err = kTCBQInit(&kobj->waitingSenders);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    taskHandle->rendezvousPtr = kobj;
    kTraceRegisterObject(kobj, RK_RENDEZVOUS_KOBJ_ID);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kRendezvousSend(RK_TASK_HANDLE const taskHandle,
                     VOID const *const mesgPtr, ULONG const mesgBytes,
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

    if (mesgBytes == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
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

    RK_RENDEZVOUS *const receiverRendezvousPtr = taskHandle->rendezvousPtr;
    if ((receiverRendezvousPtr == NULL) ||
        (receiverRendezvousPtr->init != RK_TRUE) ||
        (receiverRendezvousPtr->ownerTask != taskHandle))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kRendezvousTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if ((receiverRendezvousPtr->rendezvousRecvBufPtr != NULL) &&
        (receiverRendezvousPtr->inboxMesgPtr == NULL))
    {
        RK_ERR const err =
            kRendezvousDirectRecv_(taskHandle, receiverRendezvousPtr,
                                   mesgPtr, mesgBytes);
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_SEND,
                           err, 0UL);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK,
                           RK_ERR_NOWAIT,
                           receiverRendezvousPtr->waitingSenders.size);
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gRunPtr->rendezvousMesgPtr = mesgPtr;
    RK_gRunPtr->rendezvousMesgBytes = mesgBytes;
    RK_gRunPtr->rendezvousStatus = RK_ERR_SUCCESS;
    RK_gRunPtr->rendezvousWaitPtr = receiverRendezvousPtr;

    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_SEND;
        RK_gRunPtr->timeoutNode.waitingQueuePtr =
            &receiverRendezvousPtr->waitingSenders;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kRendezvousClearSender_(RK_gRunPtr);
            kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK, err,
                               receiverRendezvousPtr->waitingSenders.size);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_SENDING;
    RK_ERR enqErr =
        kTCBQEnqByPrio(&receiverRendezvousPtr->waitingSenders, RK_gRunPtr);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if (timeout != RK_WAIT_FOREVER)
        {
            kRendezvousDisarmTimeout_(RK_gRunPtr);
        }
        kRendezvousClearSender_(RK_gRunPtr);
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK, enqErr,
                           receiverRendezvousPtr->waitingSenders.size);
        RK_CR_EXIT
        return (enqErr);
    }
    kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK,
                       RK_ERR_SUCCESS,
                       receiverRendezvousPtr->waitingSenders.size);
    kRendezvousPromoteNext_(taskHandle);
    kRendezvousUpdateOwnerPrio_(receiverRendezvousPtr);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_TIMEOUT,
                           RK_ERR_TIMEOUT,
                           receiverRendezvousPtr->waitingSenders.size);
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    RK_ERR const err = RK_gRunPtr->rendezvousStatus;
    RK_gRunPtr->rendezvousStatus = RK_ERR_SUCCESS;
    kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_SEND, err,
                       receiverRendezvousPtr->waitingSenders.size);
    RK_CR_EXIT
    return (err);
}

RK_ERR kRendezvousRecv(VOID *const recvPtr, ULONG const recvBytes,
                     ULONG *const mesgBytesPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (recvPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (recvBytes == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    RK_RENDEZVOUS *const receiverRendezvousPtr = RK_gRunPtr->rendezvousPtr;
    if ((receiverRendezvousPtr == NULL) ||
        (receiverRendezvousPtr->init != RK_TRUE) ||
        (receiverRendezvousPtr->ownerTask != RK_gRunPtr))
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kRendezvousTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (mesgBytesPtr != NULL)
    {
        *mesgBytesPtr = 0UL;
    }

    if (receiverRendezvousPtr->inboxMesgPtr != NULL)
    {
        RK_ERR err = kRendezvousConsumeInbox_(RK_gRunPtr, recvPtr, recvBytes,
                                              mesgBytesPtr);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK,
                           RK_ERR_BUFFER_EMPTY, 0UL);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    receiverRendezvousPtr->rendezvousRecvBufPtr = recvPtr;
    receiverRendezvousPtr->rendezvousRecvBufBytes = recvBytes;
    receiverRendezvousPtr->rendezvousRecvBytesPtr = mesgBytesPtr;
    receiverRendezvousPtr->rendezvousRecvStatus = RK_ERR_SUCCESS;
    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_RECV;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kRendezvousClearReceiver_(receiverRendezvousPtr);
            kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK, err,
                               0UL);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_RECEIVING;
    kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_BLOCK,
                       RK_ERR_SUCCESS, 1UL);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_TIMEOUT,
                           RK_ERR_TIMEOUT, 0UL);
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if (receiverRendezvousPtr->rendezvousRecvBufPtr == NULL)
    {
        RK_ERR const err = receiverRendezvousPtr->rendezvousRecvStatus;
        receiverRendezvousPtr->rendezvousRecvStatus = RK_ERR_SUCCESS;
        kTraceRecordObject(receiverRendezvousPtr, RK_TRACE_OP_RECV,
                           err, 0UL);
        RK_CR_EXIT
        return (err);
    }

    RK_ERR err = kRendezvousConsumeInbox_(RK_gRunPtr, recvPtr, recvBytes,
                                          mesgBytesPtr);
    kRendezvousClearReceiver_(receiverRendezvousPtr);
    RK_CR_EXIT
    return (err);
}

#endif /* RK_CONF_RENDEZVOUS */
