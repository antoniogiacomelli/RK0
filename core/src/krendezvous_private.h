/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_RENDEZVOUS_PRIVATE_H
#define RK_RENDEZVOUS_PRIVATE_H

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

static inline VOID kRendezvousUpdateReceiverPrio_(RK_TCB *const receiverPtr)
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

static inline VOID kRendezvousCopy_(VOID *const recvPtr,
                             VOID const *const mesgPtr,
                             ULONG const mesgBytes)
{
    RK_MEMCPY(recvPtr, mesgPtr, mesgBytes);
}

static inline RK_ERR kRendezvousDirectRecv_(RK_TCB *const receiverPtr,
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

static inline VOID kRendezvousPromoteNext_(RK_TCB *const receiverPtr)
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

static inline RK_ERR kRendezvousConsumePendingSend_(RK_TCB *const receiverPtr,
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

#endif /* RK_CONF_RENDEZVOUS */

#endif /* RK_RENDEZVOUS_PRIVATE_H */
