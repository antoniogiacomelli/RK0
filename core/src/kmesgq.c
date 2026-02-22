/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.0                                                           */
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
#include <kstring.h>
#include <kapi.h>
#include <ksystasks.h>

#if (RK_CONF_MESG_QUEUE == ON)
#ifndef K_QUEUE_CPY
#define K_QUEUE_CPY(d, s, z) \
    do                       \
    {                        \
        while (--(z))        \
        {                    \
            *(d)++ = *(s)++; \
        }                    \
        *(d)++ = *(s)++;     \
    } while (0)
#endif

#ifndef K_MESGQ_IS_FAST_MBOX
#define K_MESGQ_IS_FAST_MBOX(kobj) \
    (((kobj)->maxMesg == 1UL) && ((kobj)->mesgSize == 1UL))
#endif

#ifndef K_MESGQ_CAN_USE_MBOX_FASTPATH
#define K_MESGQ_CAN_USE_MBOX_FASTPATH(kobj) K_MESGQ_IS_FAST_MBOX(kobj)
#endif

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
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    if ((mesgWords != 1UL) && (mesgWords != 2UL))
    {
        if (mesgWords % 4UL != 0UL)
        {
            K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
            RK_CR_EXIT
            return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
        }
    }

    if (nMesg == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_MESGQ_INVALID_SIZE);
    }

    if (kobj->init == 1)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#endif

    ULONG queueCapacityWords = nMesg * mesgWords;

    kobj->bufPtr = (ULONG *)bufPtr;   /* base pointer to the buffer */
    kobj->mesgSize = mesgWords; /* message size in words */
    kobj->maxMesg = nMesg;            /* maximum number of messages */
    kobj->mesgCnt = 0;
#if (RK_CONF_PORTS == ON)
    kobj->isServer = RK_FALSE;
#endif
    kobj->writePtr = kobj->bufPtr; /* start write pointer */
    kobj->readPtr = kobj->bufPtr;  /* start read pointer (same as wrt) */
    kobj->ownerTask = NULL;
    /* end of the buffer in word units */
    kobj->bufEndPtr = kobj->bufPtr + queueCapacityWords;

    RK_ERR err = kListInit(&kobj->waitingQueue);
    if (err != 0)
    {
        RK_CR_EXIT
        return (err);
    }

    kobj->init = 1;
    kobj->objID = RK_MESGQQUEUE_KOBJ_ID;

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

    kobj->sendNotifyCbk = NULL;

#endif

    RK_CR_EXIT

    return (err);
}
#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

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

RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const kobj,
                          RK_TASK_HANDLE const taskHandle)
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

    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    if (kobj->ownerTask)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_HAS_OWNER);
    }
    kobj->ownerTask = taskHandle;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
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
    if (kobj->mesgCnt >= kobj->maxMesg)
    { /* Queue full */
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_MESGQ_FULL);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
        {
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
            RK_CR_EXIT
            return (RK_ERR_INVALID_TIMEOUT);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

        /* Priority inheritance: boost owner to caller base priority if needed. */
        if (kobj->ownerTask)
        {
            RK_PRIO callerBasePrio = RK_gRunPtr->prioNominal;
            RK_PRIO targetPrio = kobj->ownerTask->priority; /* default: no change */

            if (targetPrio > callerBasePrio)
            {
                targetPrio = callerBasePrio;
            }

            if (targetPrio != kobj->ownerTask->priority)
            {
                if (kobj->ownerTask->status == RK_READY)
                {
                    RK_ERR err = kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                                          &kobj->ownerTask);
                    K_ASSERT(!err);
                    kobj->ownerTask->priority = targetPrio;
                    err = kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority],
                                   kobj->ownerTask);
                    K_ASSERT(!err);
                }
                else
                {
                    kobj->ownerTask->priority = targetPrio;
                }
            }
        }
        do
        {

            RK_gRunPtr->status = RK_SENDING;
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_TCB *selfPtr = RK_gRunPtr;
                    if ((selfPtr->tcbNode.nextPtr != NULL) &&
                        (selfPtr->tcbNode.prevPtr != NULL))
                    {
                        kTCBQRem(&kobj->waitingQueue, &selfPtr);
                    }
                    RK_gRunPtr->status = RK_RUNNING;
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
#if (RK_CONF_ERR_CHECK == ON)
                    if (err == RK_ERR_INVALID_PARAM)
                    {
                        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
                    }
#endif
                    if (err == RK_ERR_INVALID_PARAM)
                    {
                        err = RK_ERR_INVALID_TIMEOUT;
                    }
                    RK_CR_EXIT
                    return (err);
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
        } while (kobj->mesgCnt >= kobj->maxMesg);
    }

    if (K_MESGQ_CAN_USE_MBOX_FASTPATH(kobj))
    {
        kobj->bufPtr[0] = ((ULONG const *)sendPtr)[0];
        kobj->writePtr = kobj->bufPtr;
    }
    else
    {
        ULONG size = kobj->mesgSize; /* number of words to copy */
        ULONG const *srcPtr = (ULONG *)sendPtr;
        ULONG *dstPtr = kobj->writePtr;
        K_QUEUE_CPY(dstPtr, srcPtr, size);

        /*  wrap-around */
        if (dstPtr == kobj->bufEndPtr)
        {
            dstPtr = kobj->bufPtr;
        }

        kobj->writePtr = dstPtr;
    }

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)
    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);
#endif
    kobj->mesgCnt++;
    K_ASSERT(kobj->mesgCnt <= kobj->maxMesg);
    /* unblock a reader, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingQueue);
        if (freeTaskPtr->status == RK_RECEIVING)
        {
            kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
            if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
            {
                kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
                freeTaskPtr->timeoutNode.timeoutType = 0;
                freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
            }
            kReadySwtch(freeTaskPtr);
        }
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

    if (kobj->ownerTask && kobj->ownerTask != RK_gRunPtr)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_OWNER);
    }

    if (kobj->mesgCnt == 0)
    {
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_MESGQ_EMPTY);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
        {
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
            RK_CR_EXIT
            return (RK_ERR_INVALID_TIMEOUT);
        }
        do
        {
            RK_gRunPtr->status = RK_RECEIVING;

            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->status = RK_RUNNING;
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
#if (RK_CONF_ERR_CHECK == ON)
                    if (err == RK_ERR_INVALID_PARAM)
                    {
                        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
                    }
#endif
                    if (err == RK_ERR_INVALID_PARAM)
                    {
                        err = RK_ERR_INVALID_TIMEOUT;
                    }
                    RK_CR_EXIT
                    return (err);
                }
            }
            kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

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
        } while (kobj->mesgCnt == 0);
    }

    ULONG *dstStart = (ULONG *)recvPtr;
    ULONG *srcPtr = kobj->readPtr;
    UINT fastMailbox = K_MESGQ_CAN_USE_MBOX_FASTPATH(kobj);

    if (fastMailbox)
    {
        dstStart[0] = kobj->bufPtr[0];
    }
    else
    {
        ULONG size = kobj->mesgSize; /* number of words to copy */
        ULONG *destPtr = dstStart;
        K_QUEUE_CPY(destPtr, srcPtr, size);
    }
    kobj->mesgCnt--;

    if (fastMailbox)
    {
        kobj->readPtr = kobj->bufPtr;
    }
    else
    {
        /* Check for wrap-around on read pointer */
        if (srcPtr == kobj->bufEndPtr)
        {
            srcPtr = kobj->bufPtr;
        }
        kobj->readPtr = srcPtr;
    }
    /* owner keeps client priority until finishing the procedure call */
    /* unlock a writer, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingQueue);
        if (freeTaskPtr->status == RK_SENDING)
        {
            kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
            if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
            {
                kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
                freeTaskPtr->timeoutNode.timeoutType = 0;
                freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
            }
            kReadySwtch(freeTaskPtr);
        }
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

    if (kobj->mesgCnt == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_EMPTY);
    }

    if (K_MESGQ_CAN_USE_MBOX_FASTPATH(kobj))
    {
        ((ULONG *)recvPtr)[0] = kobj->bufPtr[0];
    }
    else
    {
        ULONG size = kobj->mesgSize;              /* number of words to copy */
        ULONG const *readPtrTemp = kobj->readPtr; /* make a local copy */
        ULONG *dstPtr = (ULONG *)recvPtr;
        K_QUEUE_CPY(dstPtr, readPtrTemp, size);
    }

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

    if (kobj->mesgCnt >= kobj->maxMesg)
    { /* Queue full */
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_MESGQ_FULL);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
        {
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
#endif
            RK_CR_EXIT
            return (RK_ERR_INVALID_TIMEOUT);
        }

        do
        {
            RK_gRunPtr->status = RK_SENDING;

            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

                RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
                if (err != RK_ERR_SUCCESS)
                {
                    RK_gRunPtr->status = RK_RUNNING;
                    RK_gRunPtr->timeoutNode.timeoutType = 0;
                    RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
#if (RK_CONF_ERR_CHECK == ON)
                    if (err == RK_ERR_INVALID_PARAM)
                    {
                        K_ERR_HANDLER(RK_FAULT_INVALID_TIMEOUT);
                    }
#endif
                    if (err == RK_ERR_INVALID_PARAM)
                    {
                        err = RK_ERR_INVALID_TIMEOUT;
                    }
                    RK_CR_EXIT
                    return (err);
                }
            }

            kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

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
        } while (kobj->mesgCnt >= kobj->maxMesg);
    }

    if (K_MESGQ_CAN_USE_MBOX_FASTPATH(kobj))
    {
        kobj->bufPtr[0] = ((ULONG const *)sendPtr)[0];
        kobj->readPtr = kobj->bufPtr;
        kobj->writePtr = kobj->bufPtr;
    }
    else
    {
        ULONG size = kobj->mesgSize; /* number of words to copy */
        ULONG *newReadPtr;
        /* decrement the read pointer by one message, */
        /* if at the beginning, wrap to the _end_ minus the message size */
        if (kobj->readPtr == kobj->bufPtr)
        {
            newReadPtr = kobj->bufEndPtr - size;
        }
        else
        {
            newReadPtr = kobj->readPtr - size;
        }
        /* copy message from sendPtr to the new head (newReadPtr) */
        {
            ULONG const *srcPtr = (ULONG *)sendPtr;
            K_QUEUE_CPY(newReadPtr, srcPtr, size);
        }
        /* update read pointer */
        kobj->readPtr = newReadPtr;
    }

    kobj->mesgCnt++;

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);

#endif

    /* unblock a reader, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingQueue);
        if (freeTaskPtr->status == RK_RECEIVING)
        {
            kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
            if (freeTaskPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
            {
                kRemoveTimeoutNode(&freeTaskPtr->timeoutNode);
                freeTaskPtr->timeoutNode.timeoutType = 0;
                freeTaskPtr->timeoutNode.waitingQueuePtr = NULL;
            }
            kReadySwtch(freeTaskPtr);
        }
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

    *nMesgPtr = (UINT)kobj->mesgCnt;
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

    UINT toWake = kobj->waitingQueue.size;
    if ((toWake > 0U) && kIsISR())
    {
        RK_CR_EXIT
        return (kPostProcJobEnq(RK_POSTPROC_JOB_MESGQ_RESET, (VOID *)kobj, toWake));
    }

    if (toWake > 1)
    {
        RK_CR_EXIT
        return (kPostProcJobEnq(RK_POSTPROC_JOB_MESGQ_RESET, (VOID *)kobj, toWake));
    }

    kobj->mesgCnt = 0;
    kobj->writePtr = kobj->bufPtr; /* start write pointer */
    kobj->readPtr = kobj->bufPtr;  /* start read pointer (same as wrt) */
    kobj->ownerTask = NULL;

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

    kobj->sendNotifyCbk = NULL;
#endif

    if (toWake == 0U)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    RK_TCB *chosenTCBPtr = NULL;
    for (UINT i = 0U; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        kReadyNoSwtch(nextTCBPtr);
        if (chosenTCBPtr == NULL)
        {
            chosenTCBPtr = nextTCBPtr;
        }
        else if (nextTCBPtr->priority < chosenTCBPtr->priority)
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

/* this only works for mailboxes. if on the first overwrite (mailbox is empty) there are waiting tasks, they can only be readers so they are unblocked. */
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

    if (kobj->objID != RK_MAILBOX_KOBJ_ID)
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

    if (kobj->maxMesg > 1)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }
    UINT wasEmpty = RK_FALSE;

    if (kobj->mesgCnt == 0)
    {
        wasEmpty = RK_TRUE;
    }

    if (K_MESGQ_IS_FAST_MBOX(kobj))
    {
        kobj->bufPtr[0] = ((ULONG const *)sendPtr)[0];
    }
    else
    {
        ULONG size = kobj->mesgSize; /* number of words to copy */
        ULONG const *srcPtr = (ULONG *)sendPtr;
        ULONG *dstPtr = kobj->writePtr;
        K_QUEUE_CPY(dstPtr, srcPtr, size);
    }
    /* size 1 */
    kobj->writePtr = kobj->bufPtr;
    kobj->readPtr = kobj->bufPtr;
    kobj->mesgCnt = 1;

    if (wasEmpty && kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeReadPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeReadPtr);
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
