/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_CHANNEL_PRIVATE_H
#define RK_CHANNEL_PRIVATE_H

#include <kapi.h>
#include <ktrace.h>

#if (RK_CONF_CHANNEL == ON)

static inline RK_BOOL kChannelTaskOwnsMutex_(RK_TCB const *const taskPtr)
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

/* Called with scheduler data protected by critical section. */
static inline VOID kChannelAdoptClientPrio_(RK_TCB *const serverTaskPtr,
                                            RK_TCB *const clientTaskPtr)
{
    RK_PRIO newPrio = 0U;

    if ((serverTaskPtr == NULL) || (clientTaskPtr == NULL))
    {
        return;
    }

    newPrio = clientTaskPtr->priority;
    if (serverTaskPtr->priority == newPrio)
    {
        return;
    }

    if (serverTaskPtr->status == RK_READY)
    {
        RK_TCB *serverPtr = serverTaskPtr;
        RK_PRIO const oldPrio = serverTaskPtr->priority;
        RK_ERR err =
            kTCBQRem(&RK_gReadyQueue[serverTaskPtr->priority], &serverPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        serverTaskPtr->priority = newPrio;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio, newPrio);
        err = kTCBQEnq(&RK_gReadyQueue[serverTaskPtr->priority], serverTaskPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
    else
    {
        RK_PRIO const oldPrio = serverTaskPtr->priority;
        serverTaskPtr->priority = newPrio;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio, newPrio);
        if (serverTaskPtr->status == RK_RUNNING)
        {
            kReschedRunning();
        }
    }
}

/* Called with scheduler data protected by critical section. */
static inline VOID kChannelRestoreServerPrio_(RK_TCB *const serverTaskPtr)
{
    if (serverTaskPtr == NULL)
    {
        return;
    }

    if (serverTaskPtr->priority == serverTaskPtr->prioNominal)
    {
        return;
    }

    if (serverTaskPtr->status == RK_READY)
    {
        RK_TCB *serverPtr = serverTaskPtr;
        RK_PRIO const oldPrio = serverTaskPtr->priority;
        RK_ERR err =
            kTCBQRem(&RK_gReadyQueue[serverTaskPtr->priority], &serverPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        serverTaskPtr->priority = serverTaskPtr->prioNominal;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio,
                             serverTaskPtr->prioNominal);
        err = kTCBQEnq(&RK_gReadyQueue[serverTaskPtr->priority], serverTaskPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        kReschedTask(serverTaskPtr);
    }
    else
    {
        RK_PRIO const oldPrio = serverTaskPtr->priority;
        serverTaskPtr->priority = serverTaskPtr->prioNominal;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio,
                             serverTaskPtr->prioNominal);
        if (serverTaskPtr->status == RK_RUNNING)
        {
            kReschedRunning();
        }
    }
}

/* Called with scheduler data protected by critical section. */
static inline VOID kChannelClearCaller_(RK_TCB *const callerPtr)
{
    if (callerPtr == NULL)
    {
        return;
    }

    callerPtr->channelServerPtr = NULL;
    callerPtr->channelReqPtr = NULL;
    callerPtr->channelRespPtr = NULL;
    callerPtr->channelReqSize = 0UL;
    callerPtr->channelState = RK_CALL_IDLE;
}

/* Called with scheduler data protected by critical section. */
static inline RK_TCB *kChannelPeekQueuedCaller_(RK_TCB *const serverPtr)
{
    RK_NODE *nodePtr = NULL;

    if (serverPtr == NULL)
    {
        return (NULL);
    }

    nodePtr = serverPtr->channelCallers.listDummy.nextPtr;
    while (nodePtr != &serverPtr->channelCallers.listDummy)
    {
        RK_TCB *callerPtr = K_GET_TCB_ADDR(nodePtr);

        if ((callerPtr != NULL) &&
            (callerPtr->channelServerPtr == serverPtr) &&
            (callerPtr->channelState == RK_CALL_QUEUED))
        {
            return (callerPtr);
        }

        nodePtr = nodePtr->nextPtr;
        RK_BARRIER
    }

    return (NULL);
}

/* Called with scheduler data protected by critical section. */
static inline VOID kChannelWakeServer_(RK_TCB *const serverPtr)
{
    RK_TCB *serverWaitPtr = NULL;

    if ((serverPtr == NULL) || (serverPtr->channelAcceptWaiters.size == 0UL))
    {
        return;
    }

    serverWaitPtr = kTCBQPeek(&serverPtr->channelAcceptWaiters);
    kTCBQDeq(&serverPtr->channelAcceptWaiters, &serverWaitPtr);
    if (serverWaitPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        kRemoveTimeoutNode(&serverWaitPtr->timeoutNode);
        serverWaitPtr->timeoutNode.timeoutType = 0U;
    }
    serverWaitPtr->timeoutNode.waitingQueuePtr = NULL;
    serverWaitPtr->timeoutNode.waitInfo = 0U;
    kReadySwtch(serverWaitPtr);
}

#endif /* RK_CONF_CHANNEL */

#endif /* RK_CHANNEL_PRIVATE_H */
