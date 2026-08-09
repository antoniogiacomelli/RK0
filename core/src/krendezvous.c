/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.52.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SYNCHRONOUS TASK-TO-TASK RENDEZVOUS                             */
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
    senderPtr->rendezvousReceiverPtr = NULL;
}

static inline VOID kRendezvousClearReceiverWait_(RK_TCB *const receiverPtr)
{
    receiverPtr->rendezvousRecvBufPtr = NULL;
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

static VOID kRendezvousUpdateReceiverPrio_(RK_TCB *const receiverPtr)
{
    if (receiverPtr == NULL)
    {
        return;
    }

    RK_PRIO targetPrio = receiverPtr->prioNominal;
    if (receiverPtr->rendezvousSenders.size > 0U)
    {
        RK_TCB *senderPtr = kTCBQPeek(&receiverPtr->rendezvousSenders);
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

static VOID kRendezvousCopy_(VOID *const recvPtr,
                             VOID const *const mesgPtr,
                             ULONG const mesgBytes)
{
    RK_MEMCPY(recvPtr, mesgPtr, mesgBytes);
}

static RK_ERR kRendezvousDirectRecv_(RK_TCB *const receiverPtr,
                                     VOID const *const mesgPtr)
{
    K_ASSERT(receiverPtr->rendezvousRecvBufPtr != NULL);

    kRendezvousCopy_(receiverPtr->rendezvousRecvBufPtr, mesgPtr,
                     receiverPtr->rendezvousMesgBytes);
    receiverPtr->rendezvousRecvStatus = RK_ERR_SUCCESS;

    kRendezvousClearReceiverWait_(receiverPtr);

    if (receiverPtr->timeoutNode.timeoutType == RK_TIMEOUT_SYNCH_RECV)
    {
        kRendezvousDisarmTimeout_(receiverPtr);
    }

    kReadySwtch(receiverPtr);
    return (RK_ERR_SUCCESS);
}

static VOID kRendezvousPromoteNext_(RK_TCB *const receiverPtr)
{
    if ((receiverPtr == NULL) ||
        (receiverPtr->rendezvousPendingMesgPtr != NULL))
    {
        return;
    }

    if (receiverPtr->rendezvousSenders.size > 0U)
    {
        RK_TCB *senderPtr = kTCBQPeek(&receiverPtr->rendezvousSenders);
        receiverPtr->rendezvousPendingMesgPtr = senderPtr->rendezvousMesgPtr;
        receiverPtr->rendezvousPendingSenderPtr = senderPtr;
    }
}

static RK_ERR kRendezvousConsumePendingSend_(RK_TCB *const receiverPtr,
                                             VOID *const recvPtr)
{
    K_ASSERT(receiverPtr != NULL);
    K_ASSERT(receiverPtr->rendezvousPendingMesgPtr != NULL);

    RK_TCB *senderPtr = receiverPtr->rendezvousPendingSenderPtr;
    K_ASSERT(senderPtr != NULL);

    kRendezvousCopy_(recvPtr, receiverPtr->rendezvousPendingMesgPtr,
                     receiverPtr->rendezvousMesgBytes);
    senderPtr->rendezvousStatus = RK_ERR_SUCCESS;

    receiverPtr->rendezvousPendingMesgPtr = NULL;
    receiverPtr->rendezvousPendingSenderPtr = NULL;

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverPtr->rendezvousSenders, &remPtr);
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
    kRendezvousUpdateReceiverPrio_(receiverPtr);

    err = kReadySwtch(senderPtr);
    if (err < 0)
    {
        return (err);
    }

    return (RK_ERR_SUCCESS);
}

VOID kRendezvousTimeoutSend(RK_TCB *const senderPtr)
{
    RK_TCB *const receiverPtr = senderPtr->rendezvousReceiverPtr;
    if (receiverPtr == NULL)
    {
        return;
    }

    if (receiverPtr->rendezvousPendingSenderPtr == senderPtr)
    {
        receiverPtr->rendezvousPendingMesgPtr = NULL;
        receiverPtr->rendezvousPendingSenderPtr = NULL;
    }

    RK_TCB *remPtr = senderPtr;
    RK_ERR err = kTCBQRem(&receiverPtr->rendezvousSenders, &remPtr);
    K_ASSERT(err == RK_ERR_SUCCESS);

    kRendezvousClearSender_(senderPtr);
    kRendezvousPromoteNext_(receiverPtr);
    kRendezvousUpdateReceiverPrio_(receiverPtr);
}

RK_ERR kRendezvousInit(RK_TASK_HANDLE const taskHandle,
                       ULONG const mesgBytes)
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

    if ((mesgBytes == 0UL) || ((mesgBytes % RK_WORD_SIZE) != 0UL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    if (taskHandle->rendezvousMesgBytes != 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_HAS_OWNER);
    }

    taskHandle->rendezvousMesgBytes = mesgBytes;
    taskHandle->rendezvousPendingMesgPtr = NULL;
    taskHandle->rendezvousPendingSenderPtr = NULL;
    taskHandle->rendezvousRecvBufPtr = NULL;
    taskHandle->rendezvousRecvStatus = RK_ERR_SUCCESS;
    RK_ERR err = kTCBQInit(&taskHandle->rendezvousSenders);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kRendezvousSend(RK_TASK_HANDLE const taskHandle,
                       VOID const *const mesgPtr, RK_TICK const timeout)
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

    if (taskHandle->rendezvousMesgBytes == 0UL)
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

    if (taskHandle->rendezvousMesgBytes == 0UL)
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

    if ((taskHandle->rendezvousRecvBufPtr != NULL) &&
        (taskHandle->rendezvousPendingMesgPtr == NULL))
    {
        RK_ERR const err = kRendezvousDirectRecv_(taskHandle, mesgPtr);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gRunPtr->rendezvousMesgPtr = mesgPtr;
    RK_gRunPtr->rendezvousStatus = RK_ERR_SUCCESS;
    RK_gRunPtr->rendezvousReceiverPtr = taskHandle;

    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_SEND;
        RK_gRunPtr->timeoutNode.waitingQueuePtr =
            &taskHandle->rendezvousSenders;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kRendezvousClearSender_(RK_gRunPtr);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_SENDING;
    RK_ERR enqErr = kTCBQEnqByPrio(&taskHandle->rendezvousSenders,
                                   RK_gRunPtr);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if (timeout != RK_WAIT_FOREVER)
        {
            kRendezvousDisarmTimeout_(RK_gRunPtr);
        }
        kRendezvousClearSender_(RK_gRunPtr);
        RK_CR_EXIT
        return (enqErr);
    }

    kRendezvousPromoteNext_(taskHandle);
    kRendezvousUpdateReceiverPrio_(taskHandle);
    kPendCtxSwtch();

    RK_CR_EXIT
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    RK_ERR const err = RK_gRunPtr->rendezvousStatus;
    RK_gRunPtr->rendezvousStatus = RK_ERR_SUCCESS;
    RK_CR_EXIT
    return (err);
}

RK_ERR kRendezvousRecv(VOID *const recvPtr, RK_TICK const timeout)
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

    if (RK_gRunPtr->rendezvousMesgBytes == 0UL)
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

    if (RK_gRunPtr->rendezvousMesgBytes == 0UL)
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

    if (RK_gRunPtr->rendezvousPendingMesgPtr != NULL)
    {
        RK_ERR err = kRendezvousConsumePendingSend_(RK_gRunPtr, recvPtr);
        RK_CR_EXIT
        return (err);
    }

    if (timeout == RK_NO_WAIT)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_gRunPtr->rendezvousRecvBufPtr = recvPtr;
    RK_gRunPtr->rendezvousRecvStatus = RK_ERR_SUCCESS;
    if (timeout != RK_WAIT_FOREVER)
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_SYNCH_RECV;
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            kRendezvousClearReceiverWait_(RK_gRunPtr);
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

    if (RK_gRunPtr->rendezvousRecvBufPtr == NULL)
    {
        RK_ERR const err = RK_gRunPtr->rendezvousRecvStatus;
        RK_gRunPtr->rendezvousRecvStatus = RK_ERR_SUCCESS;
        RK_CR_EXIT
        return (err);
    }

    RK_ERR err = kRendezvousConsumePendingSend_(RK_gRunPtr, recvPtr);
    kRendezvousClearReceiverWait_(RK_gRunPtr);
    RK_CR_EXIT
    return (err);
}

#endif /* RK_CONF_RENDEZVOUS */
