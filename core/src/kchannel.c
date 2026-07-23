/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.0                                                          */
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
#include <ktrace.h>

#if (RK_CONF_CHANNEL == ON)

#define RK_CHANNEL_ROUTE_WORDS ((ULONG)1UL)
#define RK_CHANNEL_ROUTE_IDX ((ULONG)0UL)

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
        K_ASSERT(!err);
        serverTaskPtr->priority = newPrio;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio, newPrio);
        err = kTCBQEnq(&RK_gReadyQueue[serverTaskPtr->priority], serverTaskPtr);
        K_ASSERT(!err);
    }
    else
    {
        RK_PRIO const oldPrio = serverTaskPtr->priority;
        serverTaskPtr->priority = newPrio;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio, newPrio);
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
        K_ASSERT(!err);
        serverTaskPtr->priority = serverTaskPtr->prioNominal;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio,
                             serverTaskPtr->prioNominal);
        err = kTCBQEnq(&RK_gReadyQueue[serverTaskPtr->priority], serverTaskPtr);
        K_ASSERT(!err);
        kReschedTask(serverTaskPtr);
    }
    else
    {
        RK_PRIO const oldPrio = serverTaskPtr->priority;
        serverTaskPtr->priority = serverTaskPtr->prioNominal;
        kTraceRecordTaskPrio(serverTaskPtr, oldPrio,
                             serverTaskPtr->prioNominal);
    }
}

/* Called with scheduler data protected by critical section. */
static RK_BOOL kChannelRequestPending_(RK_CHANNEL const *const kobj,
                                       RK_REQ_BUF const *const reqBufPtr)
{
    RK_TCB const *senderPtr = NULL;

    if ((kobj == NULL) || (reqBufPtr == NULL) ||
        (reqBufPtr->channelPtr != kobj) ||
        (reqBufPtr->state != RK_CHANNEL_REQ_QUEUED))
    {
        return (RK_FALSE);
    }

    senderPtr = reqBufPtr->sender;
    if (senderPtr == NULL)
    {
        return (RK_FALSE);
    }

    return (((senderPtr->status == RK_RECEIVING) &&
             (senderPtr->timeoutNode.waitingQueuePtr ==
              &kobj->waitingRequesters))
                ? RK_TRUE
                : RK_FALSE);
}

static RK_BOOL kChannelRemoveRoute_(RK_CHANNEL *const kobj,
                                    RK_REQ_BUF const *const reqBufPtr);

/* Called with scheduler data protected by critical section. */
static VOID kChannelClearRequest_(RK_REQ_BUF *const reqBufPtr)
{
    if (reqBufPtr == NULL)
    {
        return;
    }

    reqBufPtr->sender = NULL;
    reqBufPtr->channelPtr = NULL;
    reqBufPtr->state = RK_CHANNEL_REQ_FREE;
}

/* Called with scheduler data protected by critical section. */
VOID kChannelTimeoutRequest(RK_REQ_BUF *const reqBufPtr)
{
    RK_CHANNEL *channelPtr = NULL;

    if (reqBufPtr == NULL)
    {
        return;
    }

    channelPtr = reqBufPtr->channelPtr;
    if (channelPtr == NULL)
    {
        return;
    }

    if (reqBufPtr->state == RK_CHANNEL_REQ_QUEUED)
    {
        kChannelRemoveRoute_(channelPtr, reqBufPtr);
        kChannelClearRequest_(reqBufPtr);
        kTraceRecordObject(channelPtr, RK_TRACE_OP_TIMEOUT,
                           RK_ERR_TIMEOUT, channelPtr->ringBuf.nFull);
        return;
    }

    if ((reqBufPtr->state != RK_CHANNEL_REQ_ACTIVE) ||
        (channelPtr->activeReqPtr != reqBufPtr))
    {
        return;
    }

    reqBufPtr->sender = NULL;
    reqBufPtr->reqPtr = NULL;
    reqBufPtr->respPtr = NULL;
    reqBufPtr->state = RK_CHANNEL_REQ_ABANDONED;
    kTraceRecordObject(channelPtr, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT, 1UL);
}

/* Called with scheduler data protected by critical section. */
static RK_BOOL kChannelRemoveRoute_(RK_CHANNEL *const kobj,
                                    RK_REQ_BUF const *const reqBufPtr)
{
    RK_BOOL removed = RK_FALSE;
    ULONG const targetRouteWord = (ULONG)reqBufPtr;
    ULONG const nFull = kobj->ringBuf.nFull;

    for (ULONG i = 0UL; i < nFull; i++)
    {
        ULONG routeWord = 0UL;

        kRingBufRead(&kobj->ringBuf, &routeWord);
        if ((removed == RK_FALSE) && (routeWord == targetRouteWord))
        {
            removed = RK_TRUE;
            continue;
        }
        kRingBufWrite(&kobj->ringBuf, &routeWord);
    }

    return (removed);
}

static RK_ERR kChannelSendCore_(RK_CHANNEL *const kobj, VOID *const sendPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, RK_ERR_BUFFER_FULL,
                           kobj->ringBuf.nFull);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_FULL);
    }

    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_SEND, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);
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
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                           kobj->waitingReceivers.size);
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
            kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK,
                               RK_ERR_BUFFER_EMPTY, 0UL);
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
                    kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, err,
                                       kobj->waitingReceivers.size);
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_RECEIVING;
            kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, RK_ERR_SUCCESS,
                               kobj->waitingReceivers.size + 1UL);
            kTCBQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);

            kPendCtxSwtch();
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                                   kobj->waitingReceivers.size);
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
    kTraceRecordObject(kobj, RK_TRACE_OP_RECV, RK_ERR_SUCCESS,
                       kobj->ringBuf.nFull);
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
    kobj->objName[0] = '\0';
    kobj->init = RK_TRUE;
    kobj->serverTask = serverTask;
    kobj->activeReqPtr = NULL;
    kobj->reqPartPtr = reqPartPtr;
    serverTask->serverChannelPtr = kobj;
    kTraceRegisterObject(kobj, RK_CHANNEL_KOBJ_ID);

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
    if (kChannelTaskOwnsMutex_(RK_gRunPtr) == RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
        return (RK_ERR_TASK_INVALID_ST);
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
    reqBufPtr->state = RK_CHANNEL_REQ_QUEUED;
    RK_gRunPtr->timeoutNode.waitInfo = (UINT)reqBufPtr;

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
            RK_gRunPtr->timeoutNode.waitInfo = 0U;
            kChannelClearRequest_(reqBufPtr);
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
        RK_gRunPtr->timeoutNode.waitInfo = 0U;
        kChannelClearRequest_(reqBufPtr);
        kTraceRecordObject(kobj, RK_TRACE_OP_CALL, enqErr,
                           kobj->waitingRequesters.size);
        RK_CR_EXIT
        return (enqErr);
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_CALL, RK_ERR_SUCCESS,
                       kobj->waitingRequesters.size);

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
        RK_gRunPtr->timeoutNode.waitInfo = 0U;
        kChannelClearRequest_(reqBufPtr);
        kTraceRecordObject(kobj, RK_TRACE_OP_CALL, err,
                           kobj->waitingRequesters.size);
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
        kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                           kobj->waitingRequesters.size);
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
    if (kobj->activeReqPtr != NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_CHANNEL_BUSY);
    }
    RK_CR_EXIT

    do
    {
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

        RK_CR_ENTER
        if (kChannelRequestPending_(kobj, *reqBufPPtr) == RK_TRUE)
        {
            if (kobj->activeReqPtr != NULL)
            {
                RK_CR_EXIT
                return (RK_ERR_CHANNEL_BUSY);
            }
            (*reqBufPPtr)->state = RK_CHANNEL_REQ_ACTIVE;
            kobj->activeReqPtr = *reqBufPPtr;
            kChannelAdoptClientPrio_(RK_gRunPtr, (*reqBufPPtr)->sender);
            kTraceRecordObject(kobj, RK_TRACE_OP_ACCEPT, RK_ERR_SUCCESS,
                               kobj->waitingRequesters.size);
            RK_CR_EXIT
            return (RK_ERR_SUCCESS);
        }
        if ((*reqBufPPtr)->channelPtr != kobj)
        {
            RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
            return (RK_ERR_INVALID_OBJ);
        }
        if (((*reqBufPPtr)->state == RK_CHANNEL_REQ_ACTIVE) ||
            ((*reqBufPPtr)->state == RK_CHANNEL_REQ_ABANDONED))
        {
            RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
            return (RK_ERR_INVALID_OBJ);
        }
        kChannelClearRequest_(*reqBufPPtr);
        RK_CR_EXIT
        *reqBufPPtr = NULL;
    } while (1);
}

RK_ERR kChannelDone(RK_REQ_BUF *const reqBufPtr)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (reqBufPtr == NULL)
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

    if (channelPtr->serverTask != RK_gRunPtr)
    {
        return (RK_ERR_NOT_OWNER);
    }

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
    if ((channelPtr->activeReqPtr != reqBufPtr) ||
        ((reqBufPtr->state != RK_CHANNEL_REQ_ACTIVE) &&
         (reqBufPtr->state != RK_CHANNEL_REQ_ABANDONED)))
    {
        RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_CHANNEL_NOT_ACTIVE);
#endif
        return (RK_ERR_CHANNEL_NOT_ACTIVE);
    }

    if ((requester != NULL) && (requester->status == RK_RECEIVING) &&
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
    channelPtr->activeReqPtr = NULL;
    kChannelClearRequest_(reqBufPtr);
    kChannelRestoreServerPrio_(RK_gRunPtr);
    RK_CR_EXIT

    RK_ERR errFree = kMemPartitionFree(channelPtr->reqPartPtr, reqBufPtr);
    kTraceRecordObject(channelPtr, RK_TRACE_OP_DONE, errFree,
                       channelPtr->waitingRequesters.size);
    return (errFree);
}

#endif /* RK_CONF_CHANNEL */
