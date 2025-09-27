/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.0
 * Architecture     :   ARMv6/7m
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
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
    runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
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

    if ((nMesg != 1UL) && (nMesg != 2UL))
    {
        if (nMesg == 0)
        {
            K_ERR_HANDLER(RK_ERR_MESGQ_INVALID_SIZE);
            RK_CR_EXIT
            return (RK_ERR_MESGQ_INVALID_SIZE);
        }
        if (nMesg % 4UL != 0UL)
        {
            K_ERR_HANDLER(RK_ERR_MESGQ_INVALID_MESG_SIZE);
            RK_CR_EXIT
            return (RK_ERR_MESGQ_INVALID_MESG_SIZE);
        }
    }

    if (kobj->init == TRUE)
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
    kobj->isServer = FALSE;
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

    if (kobj->init == FALSE)
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

    kobj->ownerTask = taskHandle;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

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
    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    kobj->isServer = TRUE;
    kobj->ownerTask = owner;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

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
    if (kobj->init == FALSE)
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
            RK_ERR err = kTCBQRem(&readyQueue[kobj->ownerTask->priority],
                                  &kobj->ownerTask);
            kassert(!err);
            kobj->ownerTask->priority = kobj->ownerTask->prioReal;
            err = kTCBQEnq(&readyQueue[kobj->ownerTask->priority],
                           kobj->ownerTask);
            kassert(!err);
        }
        else
        {
            kobj->ownerTask->priority = kobj->ownerTask->prioReal;
        }
    }
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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_IS_BLOCK_ON_ISR(timeout))
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

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);

        /*
        Priority handling on full queue, if server adopts client priority
        if non-server, boost if needed  
        */
        if (kobj->ownerTask)
        {
            RK_PRIO targetPrio = kobj->ownerTask->priority; /* default: no change */
            RK_PRIO callerBasePrio = runPtr->prioReal;

            if (kobj->isServer)
            {
                targetPrio = callerBasePrio; /* adopt caller base priority */
            }
            else
            {
                if (kobj->ownerTask->priority > callerBasePrio)
                    targetPrio = callerBasePrio; /* boost only */
            }

            if (targetPrio != kobj->ownerTask->priority)
            {
                if (kobj->ownerTask->status == RK_READY)
                {
                    RK_ERR err = kTCBQRem(&readyQueue[kobj->ownerTask->priority],
                                          &kobj->ownerTask);
                    kassert(!err);
                    kobj->ownerTask->priority = targetPrio;
                    err = kTCBQEnq(&readyQueue[kobj->ownerTask->priority],
                                   kobj->ownerTask);
                    kassert(!err);
                }
                else
                {
                    kobj->ownerTask->priority = targetPrio;
                }
            }
        }

        runPtr->status = RK_SENDING;
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
            kTimeOut(&runPtr->timeoutNode, timeout);
        }
        RK_PEND_CTXTSWTCH
        RK_CR_EXIT
        RK_CR_ENTER
        if (runPtr->timeOut)
        {
            runPtr->timeOut = FALSE;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            kRemoveTimeoutNode(&runPtr->timeoutNode);
    }

    ULONG size = kobj->mesgSize; /* number of words to copy */
    ULONG const *srcPtr = (ULONG *)sendPtr;
    ULONG *dstPtr = kobj->writePtr;
    if (kobj->isServer)
    {
        /* if server, here is the sender handle */
        *dstPtr++ = (ULONG)runPtr;
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
    kassert(kobj->mesgCnt <= kobj->maxMesg);
    /* unblock a reader, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr;
        kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_IS_BLOCK_ON_ISR(timeout))
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

    if (kobj->ownerTask && kobj->ownerTask != runPtr)
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

        runPtr->status = RK_RECEIVING;

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            kTimeOut(&runPtr->timeoutNode, timeout);
        }
        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);
        RK_PEND_CTXTSWTCH
        RK_CR_EXIT
        RK_CR_ENTER
        if (runPtr->timeOut)
        {
            runPtr->timeOut = FALSE;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            kRemoveTimeoutNode(&runPtr->timeoutNode);
    }

    ULONG size = kobj->mesgSize; /* number of words to copy */
    ULONG *dstStart = (ULONG *)recvPtr;
    ULONG *destPtr = dstStart;
    ULONG *srcPtr = kobj->readPtr;

    K_QUEUE_CPY(destPtr, srcPtr, size);

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
                    RK_ERR err = kTCBQRem(&readyQueue[kobj->ownerTask->priority],
                                          &kobj->ownerTask);
                    kassert(!err);
                    kobj->ownerTask->priority = newPrio;
                    err = kTCBQEnq(&readyQueue[kobj->ownerTask->priority],
                                   kobj->ownerTask);
                    kassert(!err);
                }
                else
                {
                    kobj->ownerTask->priority = newPrio;
                }
            }
        }
    }

    /* Check for wrap-around on read pointer */
    if (srcPtr == kobj->bufEndPtr)
    {
        srcPtr = kobj->bufPtr;
    }
    kobj->readPtr = srcPtr;
    kobj->mesgCnt--;

    /* owner keeps client priority until finishing the procedure call */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
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

    if (kobj->init == FALSE)
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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (K_IS_BLOCK_ON_ISR(timeout))
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

        runPtr->status = RK_SENDING;

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

            kTimeOut(&runPtr->timeoutNode, timeout);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);

        RK_PEND_CTXTSWTCH
        RK_CR_EXIT
        RK_CR_ENTER
        if (runPtr->timeOut)
        {
            runPtr->timeOut = FALSE;
            RK_CR_EXIT
            return (RK_ERR_TIMEOUT);
        }
        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
            kRemoveTimeoutNode(&runPtr->timeoutNode);
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
        kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
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

    if (kobj->init == FALSE)
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
        if (chosenTCBPtr == NULL && (nextTCBPtr->priority < runPtr->priority))
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

    if (kobj->init == FALSE)
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

    if (kobj->maxMesg > 1 || kobj->mesgSize > 1)
    {
        RK_CR_EXIT
        return (RK_ERR_MESGQ_NOT_A_MBOX);
    }
    BOOL wasEmpty = FALSE;

    if (kobj->mesgCnt == 0)
    {
        wasEmpty = TRUE;
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

static inline RK_PORT_MSG_META *kPortMsgMeta_(ULONG *msgWords)
{
    return (RK_PORT_MSG_META *)(void *)msgWords;
}
static inline RK_PORT_MSG_META const *kPortMsgMetaConst_(ULONG const *msgWords)
{
    return (RK_PORT_MSG_META const *)(void const *)msgWords;
}

RK_ERR kPortInit(RK_PORT *const kobj,
                 VOID *const buf,
                 const ULONG msgWords,
                 const ULONG nMesg,
                 RK_TASK_HANDLE const owner)
{
    RK_ERR err = kMesgQueueInit(kobj, buf, msgWords, nMesg);
    if (err != RK_ERR_SUCCESS)
    {
        return err;
    }

    err = kMesgQueueSetOwner(kobj, owner);
    if (err != RK_ERR_SUCCESS)
    {
        return err;
    }

    return kMesgQueueSetServer(kobj, owner);
}

RK_ERR kPortSetServer(RK_PORT *const kobj, RK_TASK_HANDLE owner)
{
    return kMesgQueueSetServer(kobj, owner);
}

RK_ERR kPortSend(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout)
{
    return kMesgQueueSend(kobj, msg, timeout);
}

RK_ERR kPortRecv(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout)
{
    return kMesgQueueRecv(kobj, msg, timeout);
}

RK_ERR kPortServerDone(RK_PORT *const kobj)
{
    return (kMesgQueueServerDone(kobj));
}

RK_ERR kPortSendRecv(RK_PORT *const kobj,
                     ULONG *const msgWords,
                     RK_MAILBOX *const replyBox,
                     UINT *const replyCodePtr,
                     const RK_TICK timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWords == NULL) || (replyBox == NULL) || (replyCodePtr == NULL))
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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (replyBox->box.init == FALSE)
    {
        RK_ERR initErr = kMailboxInit(replyBox);
        if (initErr != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return initErr;
        }
    }

    RK_PORT_MSG_META *meta = kPortMsgMeta_(msgWords);
    meta->replyBox = &replyBox->box;
    RK_ERR err = kMesgQueueSend(kobj, msgWords, timeout);
    kassert(err == RK_ERR_SUCCESS);
    err = kMailboxPend(replyBox, replyCodePtr, timeout);
    RK_CR_EXIT
    return (err);
}

RK_ERR kPortReply(RK_PORT *const kobj, ULONG const *const msgWords, const UINT replyCode)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (msgWords == NULL))
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

    if (kobj->init == FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    RK_PORT_MSG_META const *meta = kPortMsgMetaConst_(msgWords);
    RK_MAILBOX *replyBox = K_GET_CONTAINER_ADDR(meta->replyBox, RK_MAILBOX, box);

#if (RK_CONF_ERR_CHECK == ON)
    if (replyBox == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    UINT code = replyCode;
    RK_ERR err = kMailboxPost(replyBox, &code, RK_WAIT_FOREVER);
    RK_CR_EXIT
    return err;
}

RK_ERR kPortReplyDone(RK_PORT *const kobj,
                      ULONG const *const msgWords,
                      const UINT replyCode)
{
    RK_ERR errPost = kPortReply(kobj, msgWords, replyCode);
    RK_ERR errDemote = kPortServerDone(kobj);
    return (errPost != RK_ERR_SUCCESS) ? errPost : errDemote;
}


RK_ERR kPortSetOwner(RK_PORT *const kobj, RK_TASK_HANDLE const taskHandle)
{
    return kMesgQueueSetOwner(kobj, taskHandle);
}
#endif /* RK_CONF_MESG_QUEUE */
#endif
