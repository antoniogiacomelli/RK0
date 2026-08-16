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
/******************************************************************************/
/* COMPONENT: SYNCHRONOUS UNBUFFERED MESSAGE                                  */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ksynchmesg.h>
#include <kapi.h>
#include <kstring.h>
#include <ktrace.h>

#if (RK_CONF_SYNCH_MESG == ON)

static inline VOID kSynchMesgClearSender_(RK_TCB *const senderPtr)
{
    senderPtr->synchMesgPtr = NULL;
    senderPtr->synchMesgBytes = 0UL;
    senderPtr->synchMesgReceiverPtr = NULL;
}

static inline VOID kSynchMesgClearReceiverWait_(RK_TCB *const receiverPtr)
{
    receiverPtr->synchMesgRecvBufPtr = NULL;
    receiverPtr->synchMesgRecvBytesPtr = NULL;
}

static inline RK_BOOL kSynchMesgTaskOwnsMutex_(RK_TCB const *const taskPtr)
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

static inline RK_BOOL kSynchMesgBytesValid_(ULONG const mesgBytes)
{
    return (((mesgBytes != 0UL) && ((mesgBytes % RK_WORD_SIZE) == 0UL))
                ? RK_TRUE
                : RK_FALSE);
}

static inline VOID kSynchMesgDisarmTimeout_(RK_TCB *const taskPtr)
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

static VOID kSynchMesgUpdateReceiverPrio_(RK_TCB *const receiverPtr)
{
    if (receiverPtr == NULL)
    {
        return;
    }

    RK_PRIO targetPrio = receiverPtr->prioNominal;
    if (receiverPtr->synchMesgSenders.size > 0U)
    {
        RK_TCB *senderPtr = kTCBQPeek(&receiverPtr->synchMesgSenders);
        K_ASSERT(senderPtr != NULL);
        if ((senderPtr != NULL) && (senderPtr->priority < targetPrio))
        {
            targetPrio = senderPtr->priority;
        }
    }

    if (targetPrio == receiverPtr->priority)
    {
        return;
    }

    if (receiverPtr->status == RK_READY)
    {
        RK_PRIO const oldPrio = receiverPtr->priority;
        RK_TCB *remPtr = receiverPtr;
        RK_ERR err = kTCBQRem(&RK_gReadyQueue[receiverPtr->priority],
                              &remPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        receiverPtr->priority = targetPrio;
        kTraceRecordTaskPrio(receiverPtr, oldPrio, targetPrio);
        err = kTCBQEnq(&RK_gReadyQueue[receiverPtr->priority], receiverPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    else
    {
        RK_PRIO const oldPrio = receiverPtr->priority;
        receiverPtr->priority = targetPrio;
        kTraceRecordTaskPrio(receiverPtr, oldPrio, targetPrio);
    }
}

static VOID kSynchMesgCopy_(VOID *const recvPtr,
                             VOID const *const mesgPtr,
                             ULONG const mesgBytes)
{
    RK_MEMCPY(recvPtr, mesgPtr, mesgBytes);
}

static inline VOID kSynchMesgRecvBytesSet_(ULONG *const mesgBytesPtr,
                                                ULONG const mesgBytes)
{
    if (mesgBytesPtr != NULL)
    {
        *mesgBytesPtr = mesgBytes;
    }
}

static inline ULONG kSynchMesgSenderBytes_(
    RK_TCB const *const receiverPtr,
    RK_TCB const *const senderPtr)
{
#if defined(RK_QEMU_UNIT_TEST)
    if (senderPtr->synchMesgBytes != 0UL)
    {
        return (senderPtr->synchMesgBytes);
    }
    return (receiverPtr->synchMesgMaxBytes);
#else
    K_UNUSE(receiverPtr);
    return (senderPtr->synchMesgBytes);
#endif
}

static RK_ERR kSynchMesgDirectRecv_(RK_TCB *const receiverPtr,
                                     VOID const *const mesgPtr,
                                     ULONG const mesgBytes)
{
    K_ASSERT(receiverPtr->synchMesgRecvBufPtr != NULL);

    kSynchMesgCopy_(receiverPtr->synchMesgRecvBufPtr, mesgPtr, mesgBytes);
    kSynchMesgRecvBytesSet_(receiverPtr->synchMesgRecvBytesPtr,
                                 mesgBytes);
    receiverPtr->synchMesgRecvStatus = RK_ERR_SUCCESS;

    kSynchMesgClearReceiverWait_(receiverPtr);

    if (receiverPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_RECV)
    {
        kSynchMesgDisarmTimeout_(receiverPtr);
    }

    kReadySwtch(receiverPtr);
    return (RK_ERR_SUCCESS);
}

static VOID kSynchMesgPromoteNext_(RK_TCB *const receiverPtr)
{
    if ((receiverPtr == NULL) ||
        (receiverPtr->synchMesgPendingPtr != NULL))
    {
        return;
    }

    if (receiverPtr->synchMesgSenders.size > 0U)
    {
        RK_TCB *senderPtr = kTCBQPeek(&receiverPtr->synchMesgSenders);
        receiverPtr->synchMesgPendingPtr = senderPtr->synchMesgPtr;
        receiverPtr->synchMesgPendingSenderPtr = senderPtr;
    }
}

static RK_ERR kSynchMesgConsumePendingSend_(RK_TCB *const receiverPtr,
                                             VOID *const recvPtr,
                                             ULONG *const mesgBytesPtr)
{
    K_ASSERT(receiverPtr != NULL);
    K_ASSERT(receiverPtr->synchMesgPendingPtr != NULL);

    RK_TCB *senderPtr = receiverPtr->synchMesgPendingSenderPtr;
    K_ASSERT(senderPtr != NULL);
    ULONG const mesgBytes =
        kSynchMesgSenderBytes_(receiverPtr, senderPtr);

    kSynchMesgCopy_(recvPtr, receiverPtr->synchMesgPendingPtr,
                     mesgBytes);
    kSynchMesgRecvBytesSet_(mesgBytesPtr, mesgBytes);
    senderPtr->synchMesgStatus = RK_ERR_SUCCESS;

    receiverPtr->synchMesgPendingPtr = NULL;
    receiverPtr->synchMesgPendingSenderPtr = NULL;

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverPtr->synchMesgSenders, &remPtr);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    if (senderPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_SEND)
    {
        kSynchMesgDisarmTimeout_(senderPtr);
    }

    kSynchMesgClearSender_(senderPtr);

    kSynchMesgPromoteNext_(receiverPtr);
    kSynchMesgUpdateReceiverPrio_(receiverPtr);

    err = kReadySwtch(senderPtr);
    if (err < 0)
    {
        return (err);
    }

    return (RK_ERR_SUCCESS);
}

VOID kSynchMesgTimeoutSend(RK_TCB *const senderPtr)
{
    RK_TCB *const receiverPtr = senderPtr->synchMesgReceiverPtr;
    if (receiverPtr == NULL)
    {
        return;
    }

    if (receiverPtr->synchMesgPendingSenderPtr == senderPtr)
    {
        receiverPtr->synchMesgPendingPtr = NULL;
        receiverPtr->synchMesgPendingSenderPtr = NULL;
    }

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverPtr->synchMesgSenders, &remPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);

    kSynchMesgClearSender_(senderPtr);
    kSynchMesgPromoteNext_(receiverPtr);
    kSynchMesgUpdateReceiverPrio_(receiverPtr);
}

RK_ERR kSynchMesgInit(RK_TASK_HANDLE const taskHandle,
                      ULONG const maxMesgBytes)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
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

    if (kSynchMesgBytesValid_(maxMesgBytes) == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    if (kSynchMesgBytesValid_(maxMesgBytes) == RK_FALSE)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (taskHandle->synchMesgMaxBytes != 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }

    taskHandle->synchMesgMaxBytes = maxMesgBytes;
#if defined(RK_QEMU_UNIT_TEST)
    taskHandle->synchMesgBytes = maxMesgBytes;
#endif
    taskHandle->synchMesgPendingPtr = NULL;
    taskHandle->synchMesgPendingSenderPtr = NULL;
    taskHandle->synchMesgRecvBufPtr = NULL;
    taskHandle->synchMesgRecvBytesPtr = NULL;
    taskHandle->synchMesgRecvStatus = RK_ERR_SUCCESS;
    RK_ERR err = kTCBQInit(&taskHandle->synchMesgSenders);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSynchSendWait(RK_TASK_HANDLE const taskHandle,
                      VOID const *const mesgPtr,
                      ULONG const mesgBytes,
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

    if (taskHandle == RK_gRunPtr)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (taskHandle->synchMesgMaxBytes == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((kSynchMesgBytesValid_(mesgBytes) == RK_FALSE) ||
        (mesgBytes > taskHandle->synchMesgMaxBytes))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (taskHandle->synchMesgMaxBytes == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((kSynchMesgBytesValid_(mesgBytes) == RK_FALSE) ||
        (mesgBytes > taskHandle->synchMesgMaxBytes))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    if (kSynchMesgTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if ((taskHandle->synchMesgRecvBufPtr != NULL) &&
        (taskHandle->synchMesgPendingPtr == NULL))
    {
        RK_ERR const err =
            kSynchMesgDirectRecv_(taskHandle, mesgPtr, mesgBytes);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gRunPtr->synchMesgPtr = mesgPtr;
    RK_gRunPtr->synchMesgBytes = mesgBytes;
    RK_gRunPtr->synchMesgStatus = RK_ERR_SUCCESS;
    RK_gRunPtr->synchMesgReceiverPtr = taskHandle;

    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_SEND;
        RK_gRunPtr->timeoutNode.waitingQueuePtr =
            &taskHandle->synchMesgSenders;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kSynchMesgClearSender_(RK_gRunPtr);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_SENDING;
    RK_ERR enqErr = kTCBQEnqByPrio(&taskHandle->synchMesgSenders,
                                   RK_gRunPtr);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if (timeout != RK_WAIT_FOREVER)
        {
            kSynchMesgDisarmTimeout_(RK_gRunPtr);
        }
        kSynchMesgClearSender_(RK_gRunPtr);
        RK_CR_EXIT
        return (enqErr);
    }

    kSynchMesgPromoteNext_(taskHandle);
    kSynchMesgUpdateReceiverPrio_(taskHandle);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    RK_ERR const err = RK_gRunPtr->synchMesgStatus;
    RK_gRunPtr->synchMesgStatus = RK_ERR_SUCCESS;
    RK_CR_EXIT
    return (err);
}

RK_ERR kSyncRecv(VOID *const recvPtr,
                 ULONG *const mesgBytesPtr,
                 RK_TICK const timeout)
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

    if (RK_gRunPtr->synchMesgMaxBytes == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
        RK_CR_EXIT
        return (RK_ERR_INVALID_TIMEOUT);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (RK_gRunPtr->synchMesgMaxBytes == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kSynchMesgTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (RK_gRunPtr->synchMesgPendingPtr != NULL)
    {
        RK_ERR err = kSynchMesgConsumePendingSend_(RK_gRunPtr, recvPtr,
                                                    mesgBytesPtr);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_gRunPtr->synchMesgRecvBufPtr = recvPtr;
    RK_gRunPtr->synchMesgRecvBytesPtr = mesgBytesPtr;
    RK_gRunPtr->synchMesgRecvStatus = RK_ERR_SUCCESS;
    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_RECV;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kSynchMesgClearReceiverWait_(RK_gRunPtr);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_RECEIVING;
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if (RK_gRunPtr->synchMesgRecvBufPtr == NULL)
    {
        RK_ERR const err = RK_gRunPtr->synchMesgRecvStatus;
        RK_gRunPtr->synchMesgRecvStatus = RK_ERR_SUCCESS;
        RK_CR_EXIT
        return (err);
    }

    RK_ERR err = kSynchMesgConsumePendingSend_(RK_gRunPtr, recvPtr,
                                                mesgBytesPtr);
    kSynchMesgClearReceiverWait_(RK_gRunPtr);
    RK_CR_EXIT
    return (err);
}

#endif /* RK_CONF_SYNCH_MESG */
