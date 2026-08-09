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
/* COMPONENT: PROCEDURE CALL CHANNEL                                          */
/******************************************************************************/

#define RK_SOURCE_CODE

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
static VOID kChannelClearCaller_(RK_TCB *const callerPtr)
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
static RK_TCB *kChannelPeekQueuedCaller_(RK_TCB *const serverPtr)
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
static VOID kChannelWakeServer_(RK_TCB *const serverPtr)
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

/* Called from timeout handling with scheduler data protected. */
VOID kChannelTimeoutRequest(RK_TCB *const callerPtr)
{
    RK_TCB *serverPtr = NULL;

    if (callerPtr == NULL)
    {
        return;
    }

    serverPtr = callerPtr->channelServerPtr;
    if (serverPtr == NULL)
    {
        return;
    }

    if (callerPtr->channelState == RK_CALL_QUEUED)
    {
        kChannelClearCaller_(callerPtr);
        return;
    }

    if ((callerPtr->channelState == RK_CALL_ACTIVE) &&
        (serverPtr->channelActiveCallerPtr == callerPtr))
    {
        callerPtr->channelReqPtr = NULL;
        callerPtr->channelRespPtr = NULL;
        callerPtr->channelReqSize = 0UL;
        callerPtr->channelState = RK_CALL_ABANDONED;
    }
}

RK_ERR kChannelCall(RK_TASK_HANDLE const serverTask,
                    VOID *const reqPtr,
                    VOID *const respPtr,
                    ULONG const size,
                    RK_TICK const timeout)
{
    RK_CR_AREA

#if (RK_CONF_ERR_CHECK == ON)
    if (serverTask == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (timeout == RK_NO_WAIT)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        return (RK_ERR_INVALID_TIMEOUT);
    }

    if ((serverTask == RK_gRunPtr) ||
        (kChannelTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE) ||
        (RK_gRunPtr->channelServerPtr != NULL) ||
        (RK_gRunPtr->channelState != RK_CALL_IDLE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        return (RK_ERR_TASK_INVALID_ST);
    }

    RK_CR_ENTER

    RK_gRunPtr->channelServerPtr = serverTask;
    RK_gRunPtr->channelReqPtr = reqPtr;
    RK_gRunPtr->channelRespPtr = respPtr;
    RK_gRunPtr->channelReqSize = size;
    RK_gRunPtr->channelState = RK_CALL_QUEUED;
    RK_gRunPtr->timeoutNode.waitingQueuePtr = &serverTask->channelCallers;
    RK_gRunPtr->timeoutNode.waitInfo = 0U;

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
        RK_BARRIER
        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            kChannelClearCaller_(RK_gRunPtr);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_RECEIVING;
    RK_ERR enqErr = kTCBQEnqByPrio(&serverTask->channelCallers, RK_gRunPtr);
    K_ASSERT(enqErr == RK_ERR_SUCCESS);
    if (enqErr != RK_ERR_SUCCESS)
    {
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
        }
        RK_gRunPtr->status = RK_RUNNING;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        kChannelClearCaller_(RK_gRunPtr);
        RK_CR_EXIT
        return (enqErr);
    }

    kChannelWakeServer_(serverTask);
    kPendCtxSwtch();
    RK_CR_EXIT

    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        RK_gRunPtr->timeoutNode.waitInfo = 0U;
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
        (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
    {
        kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        RK_gRunPtr->timeoutNode.timeoutType = 0U;
    }
    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
    RK_gRunPtr->timeoutNode.waitInfo = 0U;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelAccept(RK_CALL_DATA *const callPtr,
                      RK_TICK const timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (callPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    RK_CR_AREA
    RK_CR_ENTER

    if (kChannelTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (RK_gRunPtr->channelActiveCallerPtr != NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_CHANNEL_BUSY);
    }

    while (kChannelPeekQueuedCaller_(RK_gRunPtr) == NULL)
    {
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
        {
            RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            RK_gRunPtr->timeoutNode.waitingQueuePtr =
                &RK_gRunPtr->channelAcceptWaiters;
            RK_BARRIER
            RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                RK_CR_EXIT
                return (err);
            }
        }

        RK_gRunPtr->status = RK_RECEIVING;
        RK_ERR err = kTCBQEnq(&RK_gRunPtr->channelAcceptWaiters, RK_gRunPtr);
        K_ASSERT(err == RK_ERR_SUCCESS);
        if (err != RK_ERR_SUCCESS)
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
            }
            RK_gRunPtr->status = RK_RUNNING;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_CR_EXIT
            return (err);
        }

        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_gRunPtr->timeoutNode.waitInfo = 0U;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
        }
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
        RK_gRunPtr->timeoutNode.waitInfo = 0U;
    }

    RK_TCB *callerPtr = kChannelPeekQueuedCaller_(RK_gRunPtr);
    K_ASSERT(callerPtr != NULL);
    if (callerPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    callerPtr->channelState = RK_CALL_ACTIVE;
    RK_gRunPtr->channelActiveCallerPtr = callerPtr;

    callPtr->caller = callerPtr;
    callPtr->reqPtr = callerPtr->channelReqPtr;
    callPtr->respPtr = callerPtr->channelRespPtr;
    callPtr->size = callerPtr->channelReqSize;

    kChannelAdoptClientPrio_(RK_gRunPtr, callerPtr);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelDone(RK_CALL_DATA const *const callPtr)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((callPtr == NULL) || (callPtr->caller == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif

    RK_TCB *callerPtr = callPtr->caller;
    RK_TCB *requesterToReady = NULL;
    RK_CR_AREA
    RK_CR_ENTER

    if (kChannelTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }

    if ((RK_gRunPtr->channelActiveCallerPtr != callerPtr) ||
        (callerPtr->channelServerPtr != RK_gRunPtr) ||
        ((callerPtr->channelState != RK_CALL_ACTIVE) &&
         (callerPtr->channelState != RK_CALL_ABANDONED)))
    {
        RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_CHANNEL_NOT_ACTIVE);
#endif
        return (RK_ERR_CHANNEL_NOT_ACTIVE);
    }

    if ((callerPtr->status == RK_RECEIVING) &&
        (callerPtr->timeoutNode.waitingQueuePtr == &RK_gRunPtr->channelCallers))
    {
        if (callerPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&callerPtr->timeoutNode);
            callerPtr->timeoutNode.timeoutType = 0U;
        }
        callerPtr->timeoutNode.waitingQueuePtr = NULL;
        callerPtr->timeoutNode.waitInfo = 0U;

        requesterToReady = callerPtr;
        RK_ERR err = kTCBQRem(&RK_gRunPtr->channelCallers, &requesterToReady);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }

    RK_gRunPtr->channelActiveCallerPtr = NULL;
    kChannelClearCaller_(callerPtr);
    kChannelRestoreServerPrio_(RK_gRunPtr);

    if (requesterToReady != NULL)
    {
        kReadySwtch(requesterToReady);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_CHANNEL */
