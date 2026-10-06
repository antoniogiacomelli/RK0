/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.90.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: EXCHANGE MAILBOX                                                */
/******************************************************************************/
#define RK_SOURCE_CODE

#include <kexchg.h>
#include <ksch.h>
#include <ktrace.h>

#if (RK_CONF_EXCHG == ON)
static RK_ERR kExchangePublicReadyErr_(RK_ERR const err)
{
    if ((err == RK_ERR_RESCHED_PENDING) ||
        (err == RK_ERR_RESCHED_NOT_NEEDED))
    {
        return (RK_ERR_SUCCESS);
    }

    return (err);
}

#if (RK_CONF_ERR_CHECK == ON)
static RK_ERR kExchangeCheckObject_(RK_EXCHANGE const *const kobj)
{
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_EXCHG_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }

    return (RK_ERR_SUCCESS);
}
#endif

static VOID kExchangeClearBlockingTimeout_(RK_TCB *const taskPtr)
{
    if (taskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
    {
        kRemoveTimeoutNode(&taskPtr->timeoutNode);
        taskPtr->timeoutNode.timeoutType = 0U;
    }
}

static VOID kExchangeDeliver_(RK_TCB *const taskPtr, VOID *const mailPtr)
{
    if (taskPtr->exchgPendPPtr != NULL)
    {
        *(taskPtr->exchgPendPPtr) = mailPtr;
        taskPtr->exchgPendPPtr = NULL;
    }
}

static RK_ERR kExchangeReadyOne_(RK_EXCHANGE *const kobj, VOID *const mailPtr)
{
    RK_TCB *recvTaskPtr = NULL;
    RK_ERR err = kWaitQDeq(&kobj->waitingReceivers, &recvTaskPtr);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    kExchangeClearBlockingTimeout_(recvTaskPtr);
    kExchangeDeliver_(recvTaskPtr, mailPtr);
    err = kExchangePublicReadyErr_(kReadySwtch(recvTaskPtr));
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, err,
                       kobj->waitingReceivers.size);

    return (err);
}

#if (RK_CONF_EXCHG_BROADCAST == ON)
static RK_ERR kExchangeReadyAll_(RK_EXCHANGE *const kobj, VOID *const mailPtr,
                                 UINT *const nRecvPtr)
{
    RK_TCB *chosenTCBPtr = NULL;
    UINT nRecv = 0U;
    RK_ERR err = RK_ERR_SUCCESS;

    while (kobj->waitingReceivers.size > 0UL)
    {
        RK_TCB *recvTaskPtr = NULL;
        err = kWaitQDeq(&kobj->waitingReceivers, &recvTaskPtr);
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }

        kExchangeClearBlockingTimeout_(recvTaskPtr);
        kExchangeDeliver_(recvTaskPtr, mailPtr);
        err = kReadyNoSwtch(recvTaskPtr);
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }

        nRecv++;
        if ((chosenTCBPtr == NULL) ||
            (recvTaskPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = recvTaskPtr;
        }
    }

    if ((err == RK_ERR_SUCCESS) && (chosenTCBPtr != NULL))
    {
        err = kExchangePublicReadyErr_(kReschedTask(chosenTCBPtr));
    }

    if (nRecvPtr != NULL)
    {
        *nRecvPtr = nRecv;
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, err, (ULONG)nRecv);

    return (err);
}
#endif

static VOID kExchangeWakeSenderIfAny_(RK_EXCHANGE *const kobj)
{
    RK_TCB *sendTaskPtr = NULL;

    if ((kobj == NULL) || (kobj->waitingSenders.size == 0UL))
    {
        return;
    }

    RK_ERR const err = kWaitQDeq(&kobj->waitingSenders, &sendTaskPtr);
    if (err != RK_ERR_SUCCESS)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, err,
                           kobj->waitingSenders.size);
        return;
    }

    kExchangeClearBlockingTimeout_(sendTaskPtr);
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingSenders.size);
    (void)kReadySwtch(sendTaskPtr);
}

RK_ERR kExchangeInit(RK_EXCHANGE *const kobj, VOID *const mailPtr)
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

    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
#endif

    kTCBQInit(&kobj->waitingReceivers);
    kTCBQInit(&kobj->waitingSenders);
    kobj->init = RK_TRUE;
    kobj->objID = RK_EXCHG_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->mailPtr = mailPtr;
    kTraceRegisterObject(kobj, RK_EXCHG_KOBJ_ID);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kExchangePend(RK_EXCHANGE *const kobj, VOID **const mailPPtr,
                     RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    RK_ERR err = kExchangeCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (mailPPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#else
    RK_ERR err = RK_ERR_SUCCESS;
#endif

    *mailPPtr = NULL;

    if (kobj->mailPtr != NULL)
    {
        *mailPPtr = kobj->mailPtr;
        kobj->mailPtr = NULL;
        kExchangeWakeSenderIfAny_(kobj);
        kTraceRecordObject(kobj, RK_TRACE_OP_PEND, RK_ERR_SUCCESS, 0UL);
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if (timeout == RK_NO_WAIT)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_PEND, RK_ERR_BUFFER_EMPTY,
                           kobj->waitingReceivers.size);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

        err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
            kTraceRecordObject(kobj, RK_TRACE_OP_PEND, err,
                               kobj->waitingReceivers.size);
            RK_CR_EXIT
            return (err);
        }
    }

    RK_gRunPtr->status = RK_RECEIVING;
    RK_gRunPtr->exchgPendPPtr = mailPPtr;
    kTraceRecordObject(kobj, RK_TRACE_OP_PEND_BLOCK, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size + 1UL);
    err = kWaitQEnqByPrio(&kobj->waitingReceivers, RK_gRunPtr);
    if (err != RK_ERR_SUCCESS)
    {
        kExchangeClearBlockingTimeout_(RK_gRunPtr);
        RK_gRunPtr->exchgPendPPtr = NULL;
        RK_gRunPtr->status = RK_RUNNING;
        RK_CR_EXIT
        return (err);
    }

    kPendCtxSwtch();
    RK_CR_EXIT
    RK_CR_ENTER

    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        RK_gRunPtr->exchgPendPPtr = NULL;
        kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                           kobj->waitingReceivers.size);
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
        (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
    {
        kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        RK_gRunPtr->timeoutNode.timeoutType = 0U;
    }

    RK_gRunPtr->exchgPendPPtr = NULL;
    kTraceRecordObject(kobj, RK_TRACE_OP_PEND, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kExchangePost(RK_EXCHANGE *const kobj, VOID *const mailPtr,
                     RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    RK_ERR err = kExchangeCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (mailPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#else
    RK_ERR err = RK_ERR_SUCCESS;
#endif

    while (kobj->mailPtr != NULL)
    {
        if (timeout == RK_NO_WAIT)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_POST, RK_ERR_BUFFER_FULL,
                               kobj->waitingSenders.size);
            RK_CR_EXIT
            return (RK_ERR_BUFFER_FULL);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                RK_gRunPtr->timeoutNode.timeoutType = 0U;
                kTraceRecordObject(kobj, RK_TRACE_OP_POST, err,
                                   kobj->waitingSenders.size);
                RK_CR_EXIT
                return (err);
            }
        }

        RK_gRunPtr->status = RK_SENDING;
        kTraceRecordObject(kobj, RK_TRACE_OP_SEND_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingSenders.size + 1UL);
        err = kWaitQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);
        if (err != RK_ERR_SUCCESS)
        {
            kExchangeClearBlockingTimeout_(RK_gRunPtr);
            RK_gRunPtr->status = RK_RUNNING;
            RK_CR_EXIT
            return (err);
            
        }
        kPendCtxSwtch();
        RK_CR_EXIT
        RK_CR_ENTER

        if (RK_gRunPtr->timeOut)
        {
            RK_gRunPtr->timeOut = RK_FALSE;
            kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                               kobj->waitingSenders.size);
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
            (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
        {
            RK_CR_ENTER
            kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
            RK_gRunPtr->timeoutNode.timeoutType = 0U;
            RK_CR_EXIT
        }
    }

    if (kobj->waitingReceivers.size > 0UL)
    {
        err = kExchangeReadyOne_(kobj, mailPtr);
        if (err == RK_ERR_SUCCESS)
        {
            kExchangeWakeSenderIfAny_(kobj);
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_POST, err,
                           kobj->waitingReceivers.size);
        RK_CR_EXIT
        return (err);
    }

    kobj->mailPtr = mailPtr;
    kTraceRecordObject(kobj, RK_TRACE_OP_POST, RK_ERR_SUCCESS, 1UL);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kExchangePeek(RK_EXCHANGE const *const kobj, VOID **const mailPPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    RK_ERR const err = kExchangeCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (mailPPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    *mailPPtr = kobj->mailPtr;
    if (*mailPPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kExchangeOverwrite(RK_EXCHANGE *const kobj, VOID *const mailPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    RK_ERR err = kExchangeCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (mailPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#else
    RK_ERR err = RK_ERR_SUCCESS;
#endif

    if (kobj->waitingReceivers.size > 0UL)
    {
        err = kExchangeReadyOne_(kobj, mailPtr);
        if (err == RK_ERR_SUCCESS)
        {
            kExchangeWakeSenderIfAny_(kobj);
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_POST, err,
                           kobj->waitingReceivers.size);
        RK_CR_EXIT
        return (err);
    }

    kobj->mailPtr = mailPtr;
    kTraceRecordObject(kobj, RK_TRACE_OP_POST, RK_ERR_SUCCESS, 1UL);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#if (RK_CONF_EXCHG_BROADCAST == ON)
RK_ERR kExchangeBroadcast(RK_EXCHANGE *const kobj, VOID *const mailPtr,
                          UINT *const nRecvPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if (nRecvPtr != NULL)
    {
        *nRecvPtr = 0U;
    }

#if (RK_CONF_ERR_CHECK == ON)
    RK_ERR err = kExchangeCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    if (mailPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#else
    RK_ERR err = RK_ERR_SUCCESS;
#endif

    if (kobj->waitingReceivers.size > 0UL)
    {
        err = kExchangeReadyAll_(kobj, mailPtr, nRecvPtr);
        if (err == RK_ERR_SUCCESS)
        {
            kExchangeWakeSenderIfAny_(kobj);
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_POST, err,
                           kobj->waitingReceivers.size);
        RK_CR_EXIT
        return (err);
    }

    if (kobj->mailPtr != NULL)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_POST, RK_ERR_BUFFER_FULL, 1UL);
        RK_CR_EXIT
        return (RK_ERR_BUFFER_FULL);
    }

    kobj->mailPtr = mailPtr;
    kTraceRecordObject(kobj, RK_TRACE_OP_POST, RK_ERR_SUCCESS, 1UL);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
#endif

RK_ERR kExchangeQuery(RK_EXCHANGE const *const kobj, VOID **const mailPPtr,
                      UINT *const nPendPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    RK_ERR err = kExchangeCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
#endif

    if ((mailPPtr == NULL) && (nPendPtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (mailPPtr != NULL)
    {
        *mailPPtr = kobj->mailPtr;
    }
    if (nPendPtr != NULL)
    {
        *nPendPtr = (UINT)kobj->waitingReceivers.size;
    }

    kTraceRecordObject((VOID *)kobj, RK_TRACE_OP_QUERY, RK_ERR_SUCCESS,
                       kobj->waitingReceivers.size);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
#endif /* RK_CONF_EXCHG */
