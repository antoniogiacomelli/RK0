/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.18.1 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MESSAGE QUEUE                                                   */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmesgq.h>
#include <kringbuf.h>
#include <kstring.h>
#include <kapi.h>
#include <ksystasks.h>

#if (RK_CONF_MESG_QUEUE == ON)

RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                      const ULONG mesgWords, ULONG const nMesg)
{
    RK_CR_AREA

    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if ((kobj == NULL) || (bufPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    /* message size needs to be 1, 2, 4, or 8 words */
    if ((mesgWords == 0) || (mesgWords > 8UL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_MSG_SIZE);
    }

    if ((mesgWords != 1UL) && (mesgWords != 2UL))
    {
        if (mesgWords % 4UL != 0UL)
        {
            K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
            RK_CR_EXIT
            return (RK_ERR_INVALID_MSG_SIZE);
        }
    }

    if (nMesg == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_DEPTH);
    }

    if (kobj->init == 1)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#endif

    RK_ERR err = kRingBufInit(&kobj->ringBuf, bufPtr, mesgWords, nMesg);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kListInit(&kobj->waitingReceivers);
    if (err != 0)
    {
        RK_CR_EXIT
        return (err);
    }
    err = kListInit(&kobj->waitingSenders);
    if (err != 0)
    {
        RK_CR_EXIT
        return (err);
    }

    kobj->init = 1;
    kobj->objID = RK_MESGQQUEUE_KOBJ_ID;

    kobj->ownerTask = NULL;

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    kobj->sendNotifyCbk = NULL;

#endif

    RK_CR_EXIT

    return (err);
}

RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const kobj,
                          RK_TASK_HANDLE const ownerTask)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (ownerTask == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
    if (ownerTask->pid >= RK_CONF_NTASKS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    if ((ownerTask->queuePortPtr != NULL) &&
        (ownerTask->queuePortPtr != kobj))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
#endif
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    if ((kobj->ownerTask != NULL) && (kobj->ownerTask != ownerTask))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
#endif
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    kobj->ownerTask = ownerTask;
    ownerTask->queuePortPtr = kobj;

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

RK_ERR kMesgQueueInstallSendCbk(RK_MESG_QUEUE *const kobj,
                                VOID (*cbk)(RK_MESG_QUEUE *))

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

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    kobj->sendNotifyCbk = cbk;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
#endif

RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                      const RK_TICK timeout)
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

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    { /* Queue full */
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_FULL);
        }

        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingSenders;
                RK_BARRIER
                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_SENDING;
            kTCBQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);

            if (kobj->ownerTask)
            {
                RK_PRIO callerBasePrio = RK_gRunPtr->prioNominal;
                RK_PRIO targetPrio = kobj->ownerTask->priority;

                if (targetPrio > callerBasePrio)
                {
                    targetPrio = callerBasePrio;
                }

                if (targetPrio != kobj->ownerTask->priority)
                {
                    if (kobj->ownerTask->status == RK_READY)
                    {
                        RK_ERR err =
                            kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                                     &kobj->ownerTask);
                        K_ASSERT(!err);
                        kobj->ownerTask->priority = targetPrio;
                        err =
                            kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority],
                                     kobj->ownerTask);
                        K_ASSERT(!err);
                    }
                    else
                    {
                        kobj->ownerTask->priority = targetPrio;
                    }
                }
            }

            RK_PEND_CTXTSWTCH
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
        } while (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf);
    }

    kRingBufWrite(&kobj->ringBuf, (ULONG const *)sendPtr);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)
    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);
#endif
    K_ASSERT(kobj->ringBuf.nFull <= kobj->ringBuf.maxBuf);
    /* unblock a reader, if any */
    if (kobj->waitingReceivers.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingReceivers);
        kTCBQDeq(&kobj->waitingReceivers, &freeTaskPtr);
        if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
            freeTaskPtr->timeoutNode.timeoutType = 0;
            freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadySwtch(freeTaskPtr);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const kobj, VOID *const recvPtr,
                      const RK_TICK timeout)
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

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (recvPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if ((kobj->ownerTask != NULL) && (kobj->ownerTask != RK_gRunPtr))
    {
        RK_CR_EXIT
        return (RK_ERR_NOT_OWNER);
    }

    if (kobj->ringBuf.nFull == 0)
    {
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_EMPTY);
        }
        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr =
                    &kobj->waitingReceivers;
                RK_BARRIER

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
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
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
        } while (kobj->ringBuf.nFull == 0);
    }

    kRingBufRead(&kobj->ringBuf, (ULONG *)recvPtr);
    /* unlock a writer, if any */
    if (kobj->waitingSenders.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingSenders);
        kTCBQDeq(&kobj->waitingSenders, &freeTaskPtr);
        if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
            freeTaskPtr->timeoutNode.timeoutType = 0;
            freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadySwtch(freeTaskPtr);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const kobj, VOID *const recvPtr)
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

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (recvPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->ringBuf.nFull == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_BUFFER_EMPTY);
    }

    kRingBufPeek(&kobj->ringBuf, (ULONG *)recvPtr);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                     const RK_TICK timeout)
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

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_BLOCKING_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf)
    { /* Queue full */
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_BUFFER_FULL);
        }

        do
        {
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingSenders;
                RK_BARRIER

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
                    RK_CR_EXIT
                    return (err);
                }
            }
            RK_gRunPtr->status = RK_SENDING;

            kTCBQEnqByPrio(&kobj->waitingSenders, RK_gRunPtr);

            if (kobj->ownerTask)
            {
                RK_PRIO callerBasePrio = RK_gRunPtr->prioNominal;
                RK_PRIO targetPrio = kobj->ownerTask->priority;

                if (targetPrio > callerBasePrio)
                {
                    targetPrio = callerBasePrio;
                }

                if (targetPrio != kobj->ownerTask->priority)
                {
                    if (kobj->ownerTask->status == RK_READY)
                    {
                        RK_ERR err =
                            kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                                     &kobj->ownerTask);
                        K_ASSERT(!err);
                        kobj->ownerTask->priority = targetPrio;
                        err =
                            kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority],
                                     kobj->ownerTask);
                        K_ASSERT(!err);
                    }
                    else
                    {
                        kobj->ownerTask->priority = targetPrio;
                    }
                }
            }

            RK_PEND_CTXTSWTCH
            RK_CR_EXIT
            RK_CR_ENTER
            if (RK_gRunPtr->timeOut)
            {
                RK_gRunPtr->timeOut = RK_FALSE;
                RK_CR_EXIT
                return (RK_ERR_TIMEOUT);
            }
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
                (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
            {
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
                RK_gRunPtr->timeoutNode.timeoutType = 0;
                RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            }
        } while (kobj->ringBuf.nFull >= kobj->ringBuf.maxBuf);
    }

    kRingBufJam(&kobj->ringBuf, (ULONG const *)sendPtr);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);

#endif

    /* unblock a reader, if any */
    if (kobj->waitingReceivers.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingReceivers);
        kTCBQDeq(&kobj->waitingReceivers, &freeTaskPtr);
        if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
            freeTaskPtr->timeoutNode.timeoutType = 0;
            freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadySwtch(freeTaskPtr);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr)
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
    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (nMesgPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    *nMesgPtr = (UINT)kobj->ringBuf.nFull;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj)
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

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

#endif

    UINT toWakeR = kobj->waitingReceivers.size;
    UINT toWakeS = kobj->waitingSenders.size;
    UINT toWake = toWakeR + toWakeS;
    /* Defer only when running in ISR context; handle multi-wake inline
       otherwise to avoid re-enqueue loops when the PostProc worker invokes this
       helper. */
    if ((toWake > 0U) && kIsISR())
    {
        RK_CR_EXIT
        return (
            kPostProcJobEnq(RK_POSTPROC_JOB_MESGQ_RESET, (VOID *)kobj, toWake));
    }

    kRingBufReset(&kobj->ringBuf);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

    kobj->sendNotifyCbk = NULL;
#endif

    if (toWake == 0U)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    RK_TCB *chosenTCBPtr = NULL;
    for (UINT i = 0U; i < toWakeR; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingReceivers, &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadyNoSwtch(nextTCBPtr);
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
    }
    for (UINT i = 0U; i < toWakeS; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingSenders, &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadyNoSwtch(nextTCBPtr);
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
    }
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

/* this works only for N=1 */
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr)
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

    if (kobj->objID != RK_MESGQQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (sendPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    if (kobj->ringBuf.maxBuf > 1)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }
    RK_BOOL wasEmpty = RK_FALSE;

    if (kobj->ringBuf.nFull == 0)
    {
        wasEmpty = RK_TRUE;
    }

    kRingBufOverwrite(&kobj->ringBuf, (ULONG const *)sendPtr);

    if (wasEmpty && kobj->waitingReceivers.size > 0)
    {
        RK_TCB *freeReadPtr = NULL;
        kTCBQDeq(&kobj->waitingReceivers, &freeReadPtr);
        if (freeReadPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&freeReadPtr->timeoutNode);
            freeReadPtr->timeoutNode.timeoutType = 0;
            freeReadPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadySwtch(freeReadPtr);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif /* RK_CONF_MESG_QUEUE */
