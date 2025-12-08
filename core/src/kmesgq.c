/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.3
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************/
/**                                                                           */
/**  COMPONENT        : MESSAGE QUEUE                                         */
/**  DEPENDS ON       : LOW-LEVEL SCHEDULER, TIMER, MEMORY ALLOCATOR          */
/**  PROVIDES TO      : APPLICATION                                           */
/**  PUBLIC API       : YES                                                   */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmesgq.h>
#include <kstring.h>
#include <kapi.h>

/* Timeout Node Setup */
#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP                 \
    RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
    RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif

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
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                      const ULONG mesgSizeInWords, ULONG const nMesg)
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
    if ((mesgSizeInWords == 0) || (mesgSizeInWords > 8UL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    if ((mesgSizeInWords != 1UL) && (mesgSizeInWords != 2UL))
    {
        if (mesgSizeInWords % 4UL != 0UL)
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

    ULONG queueCapacityWords = nMesg * mesgSizeInWords;

    kobj->bufPtr = (ULONG *)bufPtr;   /* base pointer to the buffer */
    kobj->mesgSize = mesgSizeInWords; /* message size in words */
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

#if (RK_CONF_PORTS == ON)
/* this creates a port, by enabling isServer, assigning an owner.
everytime a server gets a message, it runs on the sender's priority
the sender id is the first meta-data on a RK_PORT_MESG type. */
RK_ERR kMesgQueueSetServer(RK_MESG_QUEUE *const kobj, RK_TASK_HANDLE const owner)
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

    kobj->isServer = RK_TRUE;
    kobj->ownerTask = owner;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
/* finish a client-server transaction */
RK_ERR kMesgQueueServerDone(RK_MESG_QUEUE *const kobj)
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
    if (!kobj->isServer || (kobj->ownerTask == NULL))
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }
    if (kobj->ownerTask->priority != kobj->ownerTask->prioReal)
    {
        if (kobj->ownerTask->status == RK_READY)
        {
            RK_ERR err = kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                                  &kobj->ownerTask);
            K_ASSERT(!err);
            kobj->ownerTask->priority = kobj->ownerTask->prioReal;
            err = kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority],
                           kobj->ownerTask);
            K_ASSERT(!err);
        }
        else
        {
            kobj->ownerTask->priority = kobj->ownerTask->prioReal;
        }
    }
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
    if (kobj->mesgCnt >= kobj->maxMesg)
    { /* Queue full */
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_MESGQ_FULL);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

        /*
        Priority handling on full queue, if server adopts client priority
        if non-server, boost if needed
        */
        if (kobj->ownerTask)
        {
            RK_PRIO targetPrio = kobj->ownerTask->priority; /* default: no change */
            RK_PRIO callerBasePrio = RK_gRunPtr->prioReal;
#if (RK_CONF_PORTS == ON)

            if (kobj->isServer)
            {
                targetPrio = callerBasePrio; /* adopt caller base priority */
            }
            else
            {
                if (kobj->ownerTask->priority > callerBasePrio)
                    targetPrio = callerBasePrio; /* boost only */
            }
#else

            if (kobj->ownerTask->priority > callerBasePrio)
                targetPrio = callerBasePrio; /* boost only */
#endif

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
                kTimeOut(&RK_gRunPtr->timeoutNode, timeout);
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
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        } while (kobj->mesgCnt >= kobj->maxMesg);
    }

    ULONG size = kobj->mesgSize; /* number of words to copy */
    ULONG const *srcPtr = (ULONG *)sendPtr;
    ULONG *dstPtr = kobj->writePtr;
#if (RK_CONF_PORTS == ON)

    if (kobj->isServer)
    {
        /* if server, here is the sender handle */
        *dstPtr++ = (ULONG)RK_gRunPtr;
        if (size > 1UL)
        {
            /* now  copy the remaining */
            ULONG z = size - 1UL;
            ULONG const *s = srcPtr + 1;
            K_QUEUE_CPY(dstPtr, s, z);
        }
    }
    else
    {
        K_QUEUE_CPY(dstPtr, srcPtr, size);
    }

#else

    K_QUEUE_CPY(dstPtr, srcPtr, size);

#endif

    /*  wrap-around */
    if (dstPtr == kobj->bufEndPtr)
    {
        dstPtr = kobj->bufPtr;
    }

    kobj->writePtr = dstPtr;

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
        do
        {
            RK_gRunPtr->status = RK_RECEIVING;

            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

                kTimeOut(&RK_gRunPtr->timeoutNode, timeout);
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
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        } while (kobj->mesgCnt == 0);
    }

    ULONG size = kobj->mesgSize; /* number of words to copy */
    ULONG *dstStart = (ULONG *)recvPtr;
    ULONG *destPtr = dstStart;
    ULONG *srcPtr = kobj->readPtr;
    K_ASSERT(kobj->mesgCnt > 0);
    K_QUEUE_CPY(destPtr, srcPtr, size);
    kobj->mesgCnt--;

#if (RK_CONF_PORTS == ON)
    /*  if server adopt sender's priority  */

    if (kobj->isServer)
    {
        RK_TCB *sender = (RK_TCB *)(dstStart[0]);
        RK_PRIO newPrio = sender ? sender->priority : kobj->ownerTask->priority;
        if (kobj->ownerTask)
        {
            if (kobj->ownerTask->priority != newPrio)
            {
                if (kobj->ownerTask->status == RK_READY)
                {
                    RK_ERR err = kTCBQRem(&RK_gReadyQueue[kobj->ownerTask->priority],
                                          &kobj->ownerTask);
                    K_ASSERT(!err);
                    kobj->ownerTask->priority = newPrio;
                    err = kTCBQEnq(&RK_gReadyQueue[kobj->ownerTask->priority],
                                   kobj->ownerTask);
                    K_ASSERT(!err);
                }
                else
                {
                    kobj->ownerTask->priority = newPrio;
                }
            }
        }
    }

#endif

    /* Check for wrap-around on read pointer */
    if (srcPtr == kobj->bufEndPtr)
    {
        srcPtr = kobj->bufPtr;
    }
    kobj->readPtr = srcPtr;

    /* owner keeps client priority until finishing the procedure call */
    /* unlock a writer, if any */
    if (kobj->waitingQueue.size > 0)
    {

        RK_TCB *freeTaskPtr = NULL;
        freeTaskPtr = kTCBQPeek(&kobj->waitingQueue);
        if (freeTaskPtr->status == RK_SENDING)
        {
            kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
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

    ULONG size = kobj->mesgSize;              /* number of words to copy */
    ULONG const *readPtrTemp = kobj->readPtr; /* make a local copy */
    ULONG *dstPtr = (ULONG *)recvPtr;
    K_QUEUE_CPY(dstPtr, readPtrTemp, size);

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

        do
        {
            RK_gRunPtr->status = RK_SENDING;

            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            {
                RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

                kTimeOut(&RK_gRunPtr->timeoutNode, timeout);
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
            if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
                kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        } while (kobj->mesgCnt >= kobj->maxMesg);
    }

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

    kobj->mesgCnt = 0;
    kobj->writePtr = kobj->bufPtr; /* start write pointer */
    kobj->readPtr = kobj->bufPtr;  /* start read pointer (same as wrt) */
    kobj->ownerTask = NULL;

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

    kobj->sendNotifyCbk = NULL;
#endif

    UINT toWake = kobj->waitingQueue.size;
    if (toWake == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }
    RK_TCB *chosenTCBPtr = NULL;
    for (UINT i = 0; i < toWake; i++)
    {
        RK_TCB *nextTCBPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        kReadyNoSwtch(nextTCBPtr);
        if (chosenTCBPtr == NULL && (nextTCBPtr->priority < RK_gRunPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
        else
        {
            if (nextTCBPtr->priority < chosenTCBPtr->priority)
                chosenTCBPtr = nextTCBPtr;
        }
    }
    kSchedTask(chosenTCBPtr);
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

    ULONG size = kobj->mesgSize; /* number of words to copy */
    ULONG const *srcPtr = (ULONG *)sendPtr;
    ULONG *dstPtr = kobj->writePtr;
    K_QUEUE_CPY(dstPtr, srcPtr, size);
    /* size 1 */
    kobj->writePtr = kobj->bufPtr;
    kobj->readPtr = kobj->bufPtr;
    kobj->mesgCnt = 1;

    if (wasEmpty && kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeReadPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeReadPtr);
        kReadySwtch(freeReadPtr);
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#if (RK_CONF_PORTS == ON)

static inline RK_PORT_MSG_META *kPortMsgMeta_(ULONG *msgWordsPtr)
{
    return (RK_PORT_MSG_META *)(void *)msgWordsPtr;
}
static inline RK_PORT_MSG_META const *kPortMsgMetaConst_(ULONG const *msgWordsPtr)
{
    return (RK_PORT_MSG_META const *)(void const *)msgWordsPtr;
}

RK_ERR kPortInit(RK_PORT *const kobj,
                 VOID *const buf,
                 const ULONG msgWords,
                 const ULONG nMesg,
                 RK_TASK_HANDLE const owner)
{
#if (RK_CONF_ERR_CHECK == ON)

    if (msgWords < RK_PORT_META_WORDS)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }
#endif

    RK_ERR err = kMesgQueueInit(kobj, buf, msgWords, nMesg);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    err = kMesgQueueSetOwner(kobj, owner);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    return (kMesgQueueSetServer(kobj, owner));
}

RK_ERR kPortSetServer(RK_PORT *const kobj, RK_TASK_HANDLE owner)
{
    return (kMesgQueueSetServer(kobj, owner));
}

RK_ERR kPortSend(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout)
{
    return (kMesgQueueSend(kobj, msg, timeout));
}

RK_ERR kPortRecv(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout)
{
    return (kMesgQueueRecv(kobj, msg, timeout));
}

RK_ERR kPortServerDone(RK_PORT *const kobj)
{
    return (kMesgQueueServerDone(kobj));
}

RK_ERR kPortSendRecv(RK_PORT *const kobj,
                     ULONG *const msgWordsPtr,
                     RK_MAILBOX *const replyBox,
                     UINT *const replyCodePtr,
                     const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWordsPtr == NULL) || (replyBox == NULL) || (replyCodePtr == NULL))
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
#endif

    if (kobj->mesgSize < RK_PORT_META_WORDS)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    if (replyBox->box.init == RK_FALSE)
    {
        RK_ERR err = kMailboxInit(replyBox);
        K_ASSERT(!err);
    }

    RK_PORT_MSG_META *meta = kPortMsgMeta_(msgWordsPtr);
    meta->replyBox = &replyBox->box;
    RK_ERR err = kMesgQueueSend(kobj, msgWordsPtr, timeout);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }
    RK_CR_EXIT
    return (kMailboxPend(replyBox, replyCodePtr, timeout));
}

RK_ERR kPortReply(RK_PORT *const kobj, ULONG const *const msgWordsPtr, const UINT replyCode)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWordsPtr == NULL))
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
#endif

    if (kobj->mesgSize < RK_PORT_META_WORDS)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
    }

    RK_PORT_MSG_META const *meta = kPortMsgMetaConst_(msgWordsPtr);
    RK_MESG_QUEUE *replyBoxQueue = meta->replyBox;

#if (RK_CONF_ERR_CHECK == ON)
    if (replyBoxQueue == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    RK_MAILBOX *replyBox = K_GET_CONTAINER_ADDR(replyBoxQueue, RK_MAILBOX, box);

    UINT code = replyCode;
    RK_ERR err = kMailboxPost(replyBox, &code, RK_WAIT_FOREVER);
    RK_CR_EXIT
    return (err);
}

RK_ERR kPortReplyDone(RK_PORT *const kobj,
                      ULONG const *const msgWordsPtr,
                      const UINT replyCode)
{
    RK_ERR errPost = kPortReply(kobj, msgWordsPtr, replyCode);
    RK_ERR errDemote = kPortServerDone(kobj);
    return (errPost != RK_ERR_SUCCESS) ? (errPost) : (errDemote);
}

RK_ERR kPortSetOwner(RK_PORT *const kobj, RK_TASK_HANDLE const taskHandle)
{
    return (kMesgQueueSetOwner(kobj, taskHandle));
}
#endif
#endif /* RK_CONF_MESG_QUEUE */
