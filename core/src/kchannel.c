/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.19.0                                                          */
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
#include <kringbuf.h>

#if (RK_CONF_CHANNEL == ON)

#define RK_CHANNEL_ROUTE_WORDS ((ULONG)1UL)
#define RK_CHANNEL_ROUTE_IDX ((ULONG)0UL)

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
        RK_ERR err =
            kTCBQRem(&RK_gReadyQueue[serverTaskPtr->priority], &serverPtr);
        K_ASSERT(!err);
        serverTaskPtr->priority = newPrio;
        err = kTCBQEnq(&RK_gReadyQueue[serverTaskPtr->priority], serverTaskPtr);
        K_ASSERT(!err);
    }
    else
    {
        serverTaskPtr->priority = newPrio;
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
        RK_ERR err =
            kTCBQRem(&RK_gReadyQueue[serverTaskPtr->priority], &serverPtr);
        K_ASSERT(!err);
        serverTaskPtr->priority = serverTaskPtr->prioNominal;
        err = kTCBQEnq(&RK_gReadyQueue[serverTaskPtr->priority], serverTaskPtr);
        K_ASSERT(!err);
        kReschedTask(serverTaskPtr);
    }
    else
    {
        serverTaskPtr->priority = serverTaskPtr->prioNominal;
    }
}

static RK_ERR kChannelSendCore_(RK_CHANNEL *const kobj, VOID *const sendPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_FULL);
    }

    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);
    K_ASSERT(kobj->ringBuf.nFull <= kobj->ringBuf.maxBuf);

    if (kobj->waitingReceivers.size > 0UL)
    {
        RK_TCB *freeTaskPtr = kTCBQPeek(&kobj->waitingReceivers);
        kTCBQDeq(&kobj->waitingReceivers, &freeTaskPtr);
        if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
            freeTaskPtr->timeoutNode.timeoutType = 0U;
            freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadySwtch(freeTaskPtr);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

static RK_ERR kChannelRecvCore_(RK_CHANNEL *const kobj, VOID *const recvPtr,
                                const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (kobj->ringBuf.nFull == 0UL)
    {
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }

        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr =
                    &kobj->waitingReceivers;
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
            kTCBQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);

            RK_PEND_CTXTSWTCH
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
        } while (kobj->ringBuf.nFull == 0UL);
    }

    kRingBufRead(&kobj->ringBuf, (ULONG *)recvPtr);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelInit(RK_CHANNEL *const kobj, VOID *const buf, const ULONG depth,
                    RK_TASK_HANDLE const serverTask,
                    RK_MEM_PARTITION *const reqPartPtr)
{
    RK_ERR err = RK_ERR_SUCCESS;
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (buf == NULL) || (serverTask == NULL) ||
        (reqPartPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (depth == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_DEPTH);
    }
    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    if ((serverTask->serverChannelPtr != NULL) &&
        (serverTask->serverChannelPtr != kobj))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    if ((reqPartPtr->objID != RK_MEMALLOC_KOBJ_ID) ||
        (reqPartPtr->init == RK_FALSE))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (reqPartPtr->blkSize < sizeof(RK_REQ_BUF))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    err = kRingBufInit(&kobj->ringBuf, buf, RK_CHANNEL_ROUTE_WORDS, depth);
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    err = kListInit(&kobj->waitingReceivers);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    err = kListInit(&kobj->waitingRequesters);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    kobj->objID = RK_CHANNEL_KOBJ_ID;
    kobj->init = RK_TRUE;
    kobj->serverTask = serverTask;
    kobj->reqPartPtr = reqPartPtr;
    serverTask->serverChannelPtr = kobj;

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelCall(RK_TASK_HANDLE const serverTask,
                    RK_REQ_BUF *const reqBufPtr, const RK_TICK timeout)
{
    RK_CHANNEL *kobj = NULL;
    RK_CR_AREA

#if (RK_CONF_ERR_CHECK == ON)
    if ((serverTask == NULL) || (reqBufPtr == NULL))
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

    kobj = serverTask->serverChannelPtr;
    if (kobj == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        return (RK_ERR_INVALID_OBJ);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (serverTask->pid >= RK_CONF_NTASKS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->objID != RK_CHANNEL_KOBJ_ID)
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
    if (kobj->serverTask != serverTask)
    {
        return (RK_ERR_INVALID_OBJ);
    }

    if (timeout == RK_NO_WAIT)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
        return (RK_ERR_INVALID_TIMEOUT);
    }
    if (kobj->serverTask == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->reqPartPtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->ringBuf.dataSize != RK_CHANNEL_ROUTE_WORDS)
    {
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    reqBufPtr->sender = RK_gRunPtr;
    reqBufPtr->channelPtr = kobj;

    ULONG routeWord = (ULONG)reqBufPtr;
    RK_CR_ENTER

    RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingRequesters;

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0UL))
    {
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
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

    RK_ERR enqErr = kTCBQEnqByPrio(&kobj->waitingRequesters, RK_gRunPtr);
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
        RK_CR_EXIT
        return (enqErr);
    }

    RK_ERR err = kChannelSendCore_(kobj, &routeWord);
    if (err != RK_ERR_SUCCESS)
    {
        RK_TCB *selfPtr = RK_gRunPtr;
        kTCBQRem(&kobj->waitingRequesters, &selfPtr);
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

    RK_PEND_CTXTSWTCH
    RK_CR_EXIT

    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
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
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelAccept(RK_CHANNEL *const kobj, RK_REQ_BUF **const reqBufPPtr,
                      const RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (reqBufPPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_CHANNEL_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    if (kobj->serverTask != RK_gRunPtr)
    {
        return (RK_ERR_NOT_OWNER);
    }
    if (kobj->ringBuf.dataSize != RK_CHANNEL_ROUTE_WORDS)
    {
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    ULONG routeWord = 0UL;
    RK_ERR err = kChannelRecvCore_(kobj, &routeWord, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
    *reqBufPPtr = (RK_REQ_BUF *)routeWord;

    if (*reqBufPPtr == NULL)
    {
        return (RK_ERR_INVALID_OBJ);
    }
#if (RK_CONF_ERR_CHECK == ON)
    if ((*reqBufPPtr)->sender == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
    if ((*reqBufPPtr)->channelPtr != kobj)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    RK_CR_AREA
    RK_CR_ENTER
    kChannelAdoptClientPrio_(RK_gRunPtr, (*reqBufPPtr)->sender);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kChannelDone(RK_REQ_BUF *const reqBufPtr)
{
#if (RK_CONF_ERR_CHECK == ON)
    if ((reqBufPtr == NULL) || (reqBufPtr->sender == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
    if ((reqBufPtr->channelPtr == NULL) ||
        (reqBufPtr->channelPtr->reqPartPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    RK_TCB *requester = reqBufPtr->sender;
    RK_CHANNEL *channelPtr = reqBufPtr->channelPtr;

    RK_CR_AREA
    RK_CR_ENTER
    if ((requester->status == RK_RECEIVING) &&
        (requester->timeoutNode.waitingQueuePtr ==
         &channelPtr->waitingRequesters))
    {
        if (requester->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&requester->timeoutNode);
            requester->timeoutNode.timeoutType = 0U;
        }
        requester->timeoutNode.waitingQueuePtr = NULL;

        RK_TCB *requesterPtr = requester;
        kTCBQRem(&channelPtr->waitingRequesters, &requesterPtr);
        kReadySwtch(requesterPtr);
    }
    kChannelRestoreServerPrio_(RK_gRunPtr);
    RK_CR_EXIT

    RK_ERR errFree = kMemPartitionFree(channelPtr->reqPartPtr, reqBufPtr);
    return (errFree);
}

#endif /* RK_CONF_CHANNEL */
