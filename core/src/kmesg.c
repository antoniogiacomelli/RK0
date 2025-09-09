/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.6
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

/******************************************************************************
 *
 *  Module          : MESSAGE-PASSING
 *  Depends on      : LOW-LEVEL SCHEDULER, TIMER, MEMORY ALLOCATOR
 *  Provides to     : APPLICATION
 *  Public API      : YES
 *
 *****************************************************************************/

#define RK_CODE

#include <kservices.h>
#include <kstring.h>

/* Timeout Node Setup */

#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP                 \
    runPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING; \
    runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif

/******************************************************************************/
/* MAILBOXES                                                                  */
 /*****************************************************************************/
#if (RK_CONF_MBOX == ON)
/*
 * a mailbox holds an VOID *variable as a mail
 * it can initialise full (non-null) or empty (NULL)
 * */

RK_ERR kMboxInit(RK_MBOX *const kobj, VOID *const initMailPtr)
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
    
    if (kobj->init == TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    
#endif

    kobj->init = TRUE;
    kobj->objID = RK_MAILBOX_KOBJ_ID;
    kobj->mailPtr = initMailPtr;
    kobj->ownerTask = NULL;

#if (RK_CONF_MBOX_NOTIFY == ON)

    kobj->sendNotifyCbk = NULL;
    kobj->recvNotifyCbk = NULL;

#endif

    RK_ERR listerr = kListInit(&kobj->waitingQueue);
    kassert(listerr == 0);
    RK_CR_EXIT
    return (RK_SUCCESS);
}
/* Only an owner can _receive_ from a message kernel object */
RK_ERR kMboxSetOwner(RK_MBOX *const kobj, RK_TASK_HANDLE const taskHandle)
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

    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif

    kobj->ownerTask = taskHandle;
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#if (RK_CONF_MBOX_NOTIFY == ON)

RK_ERR kMboxInstallPostCbk(RK_MBOX *const kobj,
                           VOID (*cbk)(RK_MBOX *))
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

#endif

    kobj->sendNotifyCbk = cbk;
    RK_CR_EXIT
    return (RK_SUCCESS);
}
RK_ERR kMboxInstallPendCbk(RK_MBOX *const kobj,
                           VOID (*cbk)(RK_MBOX *))
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

#endif

    kobj->recvNotifyCbk = cbk;
    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif

RK_ERR kMboxPost(RK_MBOX *const kobj, VOID *sendPtr,
                 RK_TICK const timeout)
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

    /* a post issued by an ISR cannot have a timeout other than RK_NO_WAIT */
    if (RK_IS_BLOCK_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (sendPtr == NULL)
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

#endif

    /* mailbox is full  */
    if (kobj->mailPtr != NULL)
    {

        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_MBOX_FULL);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);
        /* priority boost if mailbox has a unique receiver */
        if (kobj->ownerTask && (kobj->ownerTask->priority > runPtr->priority))
        {
            kobj->ownerTask->priority = runPtr->priority;
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
    kobj->mailPtr = sendPtr;

#if (RK_CONF_MBOX_NOTIFY == ON)
    if (kobj->sendNotifyCbk != NULL)
        kobj->sendNotifyCbk(kobj);
#endif
    if (kobj->ownerTask != NULL)
        kobj->ownerTask->priority = kobj->ownerTask->prioReal;
    /*  full: unblock a reader, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeReadPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeReadPtr);
        kReadyCtxtSwtch(freeReadPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kMboxPend(RK_MBOX *const kobj, VOID **recvPPtr, RK_TICK const timeout)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if ((kobj == NULL) || (recvPPtr == NULL))
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

    if (RK_IS_BLOCK_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    /* if mailbox has an owner, only the owner an receive from it */
    if (kobj->ownerTask && kobj->ownerTask != runPtr)
    {
        K_ERR_HANDLER(RK_FAULT_PORT_OWNER);
        RK_CR_EXIT
        return (RK_ERR_PORT_OWNER);
    }

#endif
    if (kobj->mailPtr == NULL)
    {
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_MBOX_EMPTY);
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
    *recvPPtr = kobj->mailPtr;
    kobj->mailPtr = NULL;

#if (RK_CONF_MBOX_NOTIFY == ON)
    
    if (kobj->recvNotifyCbk != NULL)
        kobj->recvNotifyCbk(kobj);

#endif
        /* unblock potential writers */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeWriterPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeWriterPtr);
        kReadyCtxtSwtch(freeWriterPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#if (RK_CONF_FUNC_MBOX_QUERY == ON)
RK_ERR kMboxQuery(RK_MBOX const *const kobj, UINT *const statePtr)
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

    if (kobj->objID != RK_MAILBOX_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (statePtr == NULL)
    {
        RK_CR_EXIT
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

#endif

    *statePtr = (kobj->mailPtr) ? (1) : (0);
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#endif

#if (RK_CONF_FUNC_MBOX_PEEK == ON)
/* read from a mailbox without extracting data */
RK_ERR kMboxPeek(RK_MBOX *const kobj, VOID **const peekPPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL || peekPPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERROR);
    }

    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERROR);
    }

    if (kobj->objID != RK_MAILBOX_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

#endif

    if (kobj->mailPtr == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_MBOX_EMPTY);
    }

    *peekPPtr = kobj->mailPtr;

    /* keep mailPtr */
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#endif

#if (RK_CONF_FUNC_MBOX_POSTOVW == ON)
RK_ERR kMboxPostOvw(RK_MBOX *const kobj, VOID *sendPtr)
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

    if (kobj->mailPtr == NULL)
    {
        /*  mailbox is empty, if any tasks are waiting
        they are readers, it is fair to unblock  */
        kobj->mailPtr = sendPtr;
        if (kobj->waitingQueue.size > 0)
        {
            RK_TCB *freeReadPtr = NULL;
            kTCBQDeq(&kobj->waitingQueue, &freeReadPtr);
            kReadyCtxtSwtch(freeReadPtr);
        }
    }
    else
    {
        /* otherwise any waiting tasks are writers */ 
        /* just overwrite */   

        kobj->mailPtr = sendPtr;
    
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif
#endif /* mailbox */


/******************************************************************************/
/* MAIL QUEUE                                                                 */
/******************************************************************************/
#if (RK_CONF_QUEUE == ON)

RK_ERR kQueueInit(RK_QUEUE *const kobj, VOID **bufPPtr,
                  ULONG const maxItems)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL || bufPPtr == NULL || maxItems == 0)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    
    if (kobj->init == TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    
#endif

    kobj->mailQPPtr = bufPPtr;
    kobj->bufEndPPtr = kobj->mailQPPtr + maxItems;
    kobj->headPPtr = kobj->mailQPPtr;
    kobj->tailPPtr = kobj->mailQPPtr;
    kobj->maxItems = maxItems;
    kobj->countItems = 0;
    kobj->init = TRUE;
    kobj->objID = RK_MAILQUEUE_KOBJ_ID;
    kobj->ownerTask = NULL;

#if (RK_CONF_QUEUE_NOTIFY == ON)

    kobj->sendNotifyCbk = NULL;
    kobj->recvNotifyCbk = NULL;

#endif

    RK_ERR listerr = kListInit(&kobj->waitingQueue);
    kassert(listerr == 0);

    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kQueueSetOwner(RK_QUEUE *const kobj, RK_TASK_HANDLE const taskHandle)
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
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
    return (RK_SUCCESS);
}

#if (RK_CONF_QUEUE_NOTIFY == ON)

RK_ERR kQueueInstallPostCbk(RK_QUEUE *const kobj,
                            VOID (*cbk)(RK_QUEUE *))
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
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

    kobj->sendNotifyCbk = cbk;
    RK_CR_EXIT
    return (RK_SUCCESS);
} 

RK_ERR kQueueInstallPendCbk(RK_QUEUE *const kobj,
                            VOID (*cbk)(RK_QUEUE *))
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
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

    kobj->recvNotifyCbk = cbk;
    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif


RK_ERR kQueuePost(RK_QUEUE *const kobj, VOID *const sendPtr,
                  RK_TICK const timeout)
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
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

    if (RK_IS_BLOCK_ON_ISR(timeout))
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
    /*   if queue is full */
    if (kobj->countItems == kobj->maxItems)
    {
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_QUEUE_FULL);
        }

        if (kobj->ownerTask && (kobj->ownerTask->priority > runPtr->priority))
        {
            kobj->ownerTask->priority = runPtr->priority;
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
            kTimeOut(&runPtr->timeoutNode, timeout);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);
        runPtr->status = RK_SENDING;
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
        {
            kRemoveTimeoutNode(&runPtr->timeoutNode);
        }
    }
    /* this effectively store sendPtr in the current tail _position_ */
    *kobj->tailPPtr = sendPtr;
    if (kobj->ownerTask != NULL)
        kobj->ownerTask->priority = kobj->ownerTask->prioReal;

    kobj->tailPPtr++;
    if (kobj->tailPPtr >= kobj->bufEndPPtr)
    {
        kobj->tailPPtr = kobj->mailQPPtr;
    }

    kobj->countItems++;

#if (RK_CONF_QUEUE_NOTIFY == ON)
    if (kobj->sendNotifyCbk != NULL)
        kobj->sendNotifyCbk(kobj);
#endif
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeReadPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeReadPtr);
        kReadyCtxtSwtch(freeReadPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kQueuePend(RK_QUEUE *const kobj, VOID **const recvPPtr, RK_TICK const timeout)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL || recvPPtr == NULL)
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (RK_IS_BLOCK_ON_ISR(timeout))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif
    /*   if queue is empty */
    if (kobj->countItems == 0)
    {
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_QUEUE_EMPTY);
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
            kTimeOut(&runPtr->timeoutNode, timeout);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);

        runPtr->status = RK_RECEIVING;
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
        {
            kRemoveTimeoutNode(&runPtr->timeoutNode);
        }
    }

    /* get the message from the head position */
    *recvPPtr = *kobj->headPPtr;

    kobj->headPPtr++;
    if (kobj->headPPtr >= kobj->bufEndPPtr)
    {
        kobj->headPPtr = kobj->mailQPPtr;
    }

    kobj->countItems--;

#if (RK_CONF_QUEUE_NOTIFY == ON)

    if (kobj->recvNotifyCbk != NULL)
        kobj->recvNotifyCbk(kobj);
#endif
    
    
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeSendPtr = NULL;

        kTCBQDeq(&kobj->waitingQueue, &freeSendPtr);
        kReadyCtxtSwtch(freeSendPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#if (RK_CONF_FUNC_QUEUE_JAM == ON)

RK_ERR kQueueJam(RK_QUEUE *const kobj, VOID *const sendPtr, RK_TICK const timeout)
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (RK_IS_BLOCK_ON_ISR(timeout))
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
    /*   if queue is full */
    if (kobj->countItems == kobj->maxItems)
    {
        if (timeout == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_QUEUE_FULL);
        }

        if (kobj->ownerTask && (kobj->ownerTask->priority > runPtr->priority))
        {
            runPtr->priority = kobj->ownerTask->priority;
        }

        if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
        {
            RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
            kTimeOut(&runPtr->timeoutNode, timeout);
        }

        kTCBQEnqByPrio(&kobj->waitingQueue, runPtr);
        runPtr->status = RK_SENDING;
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
        {
            kRemoveTimeoutNode(&runPtr->timeoutNode);
        }
    }

    /*  jam position (one before head) */
    VOID **jamPtr;
    if (kobj->headPPtr == kobj->mailQPPtr)
    {
        /* head is at the start, get back 1 sizeof(VOID **) */
        jamPtr = kobj->bufEndPPtr - 1;
    }
    else
    {
        jamPtr = kobj->headPPtr - 1;
    }

    /* store the message at the jam position */
    *jamPtr = sendPtr;
    /*  head pointer <- jam position */
    kobj->headPPtr = jamPtr;
    kobj->countItems++;

#if (RK_CONF_QUEUE_NOTIFY == ON)
    if (kobj->sendNotifyCbk != NULL)
#endif

#if (RK_CONF_QUEUE_NOTIFY == ON)
    kobj->sendNotifyCbk(kobj);
#endif
    
    if (kobj->ownerTask != NULL)
        kobj->ownerTask->priority = kobj->ownerTask->prioReal;

    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeReadPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeReadPtr);
        kReadyCtxtSwtch(freeReadPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_QUEUE_PEEK == ON)


RK_ERR kQueuePeek(RK_QUEUE *const kobj, VOID **const peekPPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL || peekPPtr == NULL)
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

#endif
    /*   if queue is empty */
    if (kobj->countItems == 0)
    {
        RK_CR_EXIT
        return (RK_ERR_QUEUE_EMPTY);
    }
    *peekPPtr = *kobj->headPPtr;

    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_QUEUE_QUERY == ON)

RK_ERR kQueueQuery(RK_QUEUE const *const kobj, UINT *const nMailPtr)
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

    if (kobj->objID != RK_MAILQUEUE_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (nMailPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    *nMailPtr = (UINT)kobj->countItems;
    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif

#endif /* RK_CONF_QUEUE */

/******************************************************************************/
/* STREAM QUEUE                                                               */
/******************************************************************************/
#if (RK_CONF_STREAM == ON)
#ifndef RK_CPYQ
#define RK_CPYQ(s, d, z)     \
    do                       \
    {                        \
        while (--(z))        \
        {                    \
            *(d)++ = *(s)++; \
        }                    \
        *(d)++ = *(s)++;     \
    } while (0)
#endif
RK_ERR kStreamInit(RK_STREAM *const kobj, VOID *const bufPtr,
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
    if (mesgSizeInWords == 0)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_MESG_SIZE);
    }

    if ((mesgSizeInWords != 1UL) && (mesgSizeInWords != 2UL))
    {
        /* allowed sizes, 1, 2, 4, 8... 2^N */
        if (mesgSizeInWords % 4UL != 0UL)
        {
            K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
            RK_CR_EXIT
            return (RK_ERR_INVALID_MESG_SIZE);
        }
    }

    if ((nMesg != 1UL) && (nMesg != 2UL))
    {
        if (nMesg == 0)
        {
            RK_CR_EXIT
            return (RK_ERR_INVALID_QUEUE_SIZE);
        }

        /* allowed sizes, 1, 2, 4, 8... 2^N */
        if (nMesg % 4UL != 0UL)
        {
            RK_CR_EXIT
            return (RK_ERR_INVALID_QUEUE_SIZE);
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
    kobj->objID = RK_STREAMQUEUE_KOBJ_ID;
   
#if (RK_CONF_STREAM_NOTIFY == ON)

    kobj->recvNotifyCbk = NULL;
    kobj->sendNotifyCbk = NULL;

#endif
    
    RK_CR_EXIT

    return (err);
}

RK_ERR kStreamSetOwner(RK_STREAM *const kobj, RK_TASK_HANDLE const taskHandle)
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

    if (kobj->objID != RK_STREAMQUEUE_KOBJ_ID)
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
    return (RK_SUCCESS);
}

RK_ERR kStreamSend(RK_STREAM *const kobj, VOID *const sendPtr,
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

    if (kobj->objID != RK_STREAMQUEUE_KOBJ_ID)
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

    if (RK_IS_BLOCK_ON_ISR(timeout))
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
    { /* Stream full */
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_STREAM_FULL);
        }
        runPtr->status = RK_SENDING;

        if (kobj->ownerTask && (kobj->ownerTask->priority > runPtr->priority))
        {
            kobj->ownerTask->priority = runPtr->priority;
        }
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
    ULONG const *srcPtr = (ULONG *)sendPtr;
    ULONG *dstPtr = kobj->writePtr;
    RK_CPYQ(srcPtr, dstPtr, size);
    /*  wrap-around */
    if (dstPtr == kobj->bufEndPtr)
    {
        dstPtr = kobj->bufPtr;
    }

    kobj->writePtr = dstPtr;

#if (RK_CONF_STREAM_NOTIFY == ON)
    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);
#endif

    if (kobj->ownerTask != NULL)
        kobj->ownerTask->priority = kobj->ownerTask->prioReal;

    kobj->mesgCnt++;
    
    /* unblock a reader, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr;
        kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
        kReadyCtxtSwtch(freeTaskPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_ERR kStreamRecv(RK_STREAM *const kobj, VOID *const recvPtr,
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

    if (kobj->objID != RK_STREAMQUEUE_KOBJ_ID)
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

    if (RK_IS_BLOCK_ON_ISR(timeout))
    {

        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if (kobj->ownerTask && kobj->ownerTask != runPtr)
    {
        K_ERR_HANDLER(RK_FAULT_PORT_OWNER);
        RK_CR_EXIT
        return (RK_ERR_PORT_OWNER);
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
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_STREAM_EMPTY);
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
    ULONG *destPtr = (ULONG *)recvPtr;
    ULONG *srcPtr = kobj->readPtr;

    RK_CPYQ(srcPtr, destPtr, size);

    /* Check for wrap-around on read pointer */
    if (srcPtr == kobj->bufEndPtr)
    {
        srcPtr = kobj->bufPtr;
    }
    kobj->readPtr = srcPtr;
    kobj->mesgCnt--;

#if (RK_CONF_STREAM_NOTIFY == ON)
    if (kobj->recvNotifyCbk)
        kobj->recvNotifyCbk(kobj);
#endif

    /* Unblock a waiting sender if needed */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
        kReadyCtxtSwtch(freeTaskPtr);
    }
    RK_CR_EXIT
    return (RK_SUCCESS);
}

#if (RK_CONF_FUNC_STREAM_PEEK == ON)
RK_ERR kStreamPeek(RK_STREAM  *const kobj, VOID *const recvPtr)
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

    if (kobj->objID != RK_STREAMQUEUE_KOBJ_ID)
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

    if (kobj->ownerTask && kobj->ownerTask != runPtr)
    {
        K_ERR_HANDLER(RK_FAULT_PORT_OWNER);
        RK_CR_EXIT
        return (RK_ERR_PORT_OWNER);
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
        return (RK_ERR_STREAM_EMPTY);
    }

    ULONG size = kobj->mesgSize;              /* number of words to copy */
    ULONG const *readPtrTemp = kobj->readPtr; /* make a local copy */
    ULONG *dstPtr = (ULONG *)recvPtr;
    RK_CPYQ(readPtrTemp, dstPtr, size);

    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_STREAM_JAM == ON)

RK_ERR kStreamJam(RK_STREAM *const kobj, VOID *const sendPtr,
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

    if (kobj->objID != RK_STREAMQUEUE_KOBJ_ID)
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

    if (RK_IS_BLOCK_ON_ISR(timeout))
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
    { /* Stream full */
        if (timeout == RK_NO_WAIT)
        {
            RK_CR_EXIT
            return (RK_ERR_STREAM_FULL);
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
        RK_CPYQ(srcPtr, newReadPtr, size);
    }
    /* update read pointer */
    kobj->readPtr = newReadPtr;

    kobj->mesgCnt++;

#if (RK_CONF_STREAM_NOTIFY == ON)

    if (kobj->sendNotifyCbk)
        kobj->sendNotifyCbk(kobj);

#endif

    /* unblock a reader, if any */
    if (kobj->waitingQueue.size > 0)
    {
        RK_TCB *freeTaskPtr = NULL;
        kTCBQDeq(&kobj->waitingQueue, &freeTaskPtr);
        kReadyCtxtSwtch(freeTaskPtr);
    }

    RK_CR_EXIT
    return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_STREAM_QUERY == ON)
RK_ERR kStreamQuery(RK_STREAM const *const kobj, UINT *const nMesgPtr)
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

    if (kobj->objID != RK_STREAMQUEUE_KOBJ_ID)
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
    return (RK_SUCCESS);
}
#endif

#endif /*RK_CONF_STREAM*/

#if (RK_CONF_MRM == ON)
/******************************************************************************/
/* MRM Buffers                                                                */
/******************************************************************************/
RK_ERR kMRMInit(RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
                VOID *mesgPoolPtr, ULONG const nBufs, ULONG const dataSizeWords)
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

    if (kobj->init == TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    

#endif
    RK_ERR err = RK_ERROR;

    err = kMemInit(&kobj->mrmMem, mrmPoolPtr, sizeof(RK_MRM_BUF), nBufs);
    if (!err)
        err = kMemInit(&kobj->mrmDataMem, mesgPoolPtr, dataSizeWords * 4,
                       nBufs);
    if (!err)
    {
        /* nobody is using anything yet */
        kobj->currBufPtr = NULL;
        kobj->init = TRUE;
        kobj->size = dataSizeWords;
        kobj->objID = RK_MRM_KOBJ_ID;
    }

    RK_CR_EXIT
    return (err);
}

RK_MRM_BUF *kMRMReserve(RK_MRM *const kobj)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (NULL);
    }
    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (NULL);
    }

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (NULL);
    }

#endif

    RK_MRM_BUF *allocPtr = NULL;
    if ((kobj->currBufPtr != NULL))
    {
        if ((kobj->currBufPtr->nUsers == 0))
        {
            allocPtr = kobj->currBufPtr;
            RK_MEMSET(kobj->currBufPtr->mrmData, 0, kobj->size);
        }
        else
        {
            allocPtr = kMemAlloc(&kobj->mrmMem);
            if (allocPtr != NULL)
            {
                allocPtr->mrmData = (ULONG *)kMemAlloc(&kobj->mrmDataMem);
            }
        }
    }
    else
    {
        allocPtr = kMemAlloc(&kobj->mrmMem);
        if (allocPtr != NULL)
        {
            allocPtr->mrmData = (ULONG *)kMemAlloc(&kobj->mrmDataMem);
        }
    }
    RK_CR_EXIT
    return (allocPtr);
}

RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
                   VOID const *pubMesgPtr)
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

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (bufPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (pubMesgPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    ULONG *mrmMesgPtr_ = (ULONG *)bufPtr->mrmData;
    const ULONG *pubMesgPtr_ = (const ULONG *)pubMesgPtr;
    for (UINT i = 0; i < kobj->size; ++i)
    {
        mrmMesgPtr_[i] = pubMesgPtr_[i];
    }
    kobj->currBufPtr = bufPtr;
    RK_CR_EXIT
    return (RK_SUCCESS);
}

RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (NULL);
    }

    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (NULL);
    }

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (NULL);
    }

    if (getMesgPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (NULL);
    }

#endif

    kobj->currBufPtr->nUsers++;
    ULONG *getMesgPtr_ = (ULONG *)getMesgPtr;
    ULONG const *mrmMesgPtr_ = (ULONG const *)kobj->currBufPtr->mrmData;
    for (ULONG i = 0; i < kobj->size; ++i)
    {
        getMesgPtr_[i] = mrmMesgPtr_[i];
    }
    RK_CR_EXIT
    return (kobj->currBufPtr);
}

RK_ERR kMRMUnget(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr)
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

    if (bufPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

#endif

    RK_ERR err = 0;
    if (bufPtr->nUsers > 0)
        bufPtr->nUsers--;
    /* deallocate if not used and not the most recent buffer */
    if ((bufPtr->nUsers == 0) && (kobj->currBufPtr != bufPtr))
    {
        ULONG *mrmDataPtr = bufPtr->mrmData;
        kMemFree(&kobj->mrmDataMem, (VOID *)mrmDataPtr);
        kMemFree(&kobj->mrmMem, (VOID *)bufPtr);
    }

    RK_CR_EXIT
    return (err);
}
#endif
