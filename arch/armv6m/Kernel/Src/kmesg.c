/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv6/7-M
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Module          : MESSAGE-PASSING
 *  Depends on      : LOW-LEVEL SCHEDULER, TIMER
 *  Provides to     : EXECUTIVE, APPLICATION
 *  Public API      : YES
 *
 *****************************************************************************/

#define RK_CODE

#include <kexecutive.h>

/* Timeout Node Setup */

#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP \
    runPtr->timeoutNode.timeoutType = RK_BLOCKING_TIMEOUT; \
    runPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;
#endif

/*******************************************************************************
 * MAILBOXES
 ******************************************************************************/
#if (RK_CONF_MBOX==ON)
/*
 * a mailbox holds an VOID * variable as a mail
 * it can initialise full (non-null) or empty (NULL)
 * */

RK_ERR kMboxInit( RK_MBOX *const kobj, VOID * initMailPtr)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}
	kobj->mailPtr = initMailPtr;
	RK_ERR err;
	err = kListInit( &kobj->waitingQueue, "mailq");
	if (err != RK_SUCCESS)
	{
		RK_CR_EXIT
		return (err);
	}
	kobj->init = TRUE;
	kobj->objID = RK_MAILBOX_KOBJ_ID;
	RK_CR_EXIT
	return (RK_SUCCESS);
}
/* Only an owner can _receive_ from a message kernel object */
RK_ERR kMboxSetOwner( RK_MBOX *const kobj, RK_TASK_HANDLE const taskHandle)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj && taskHandle)
	{
		kobj->ownerTask = taskHandle;
		RK_CR_EXIT
		return (RK_SUCCESS);
	}
	RK_CR_EXIT
	return (RK_ERR_OBJ_NULL);
}

RK_ERR kMboxPost( RK_MBOX *const kobj, VOID * sendPtr,
		RK_TICK const timeout)
{

	RK_CR_AREA
	RK_CR_ENTER
	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
	}
	if ((kobj == NULL) || (sendPtr == NULL))
	{
		KERR( RK_FAULT_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
	}

	/* mailbox is full  */

	if (kobj->mailPtr != NULL)
	{

		if (timeout == 0)
		{
			RK_CR_EXIT
			return (RK_ERR_MBOX_FULL);
		}

		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);

		if (kobj->ownerTask)
		{
			/* priority boost */
			if (kobj->ownerTask->priority > runPtr->priority)
			{
				kobj->ownerTask->priority = runPtr->priority;
			}
		}

		runPtr->status = RK_SENDING;

		if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);

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
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
			kRemoveTimeoutNode( &runPtr->timeoutNode);

	}

	kobj->mailPtr = sendPtr;
	if (kobj->ownerTask != NULL)
		kobj->ownerTask->priority = kobj->ownerTask->realPrio;

	/*  full: unblock a reader, if any */
	if (kobj->waitingQueue.size > 0)
	{
		RK_TCB *freeReadPtr;
		freeReadPtr = kTCBQPeek( &kobj->waitingQueue);
		{
			kTCBQDeq( &kobj->waitingQueue, &freeReadPtr);
			kassert( freeReadPtr != NULL);
			kTCBQEnq( &readyQueue[freeReadPtr->priority], freeReadPtr);
			freeReadPtr->status = RK_READY;
			if (freeReadPtr->priority < runPtr->priority)
				RK_PEND_CTXTSWTCH
		}
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

#if (RK_CONF_FUNC_MBOX_POSTOVW==(ON))
RK_ERR kMboxPostOvw( RK_MBOX *const kobj, VOID * sendPtr)
{
	RK_CR_AREA
	RK_CR_ENTER

	if ((kobj == NULL) || (sendPtr == NULL))
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERR_OBJ_NOT_INIT);
	}
	if (kobj == NULL)
	{
		kobj->mailPtr = sendPtr;
		/*  full: unblock a reader, if any */
		if (kobj->waitingQueue.size > 0)
		{
			RK_TCB *freeReadPtr;
			freeReadPtr = kTCBQPeek( &kobj->waitingQueue);

			kTCBQDeq( &kobj->waitingQueue, &freeReadPtr);
			kassert( freeReadPtr != NULL);
			kTCBQEnq( &readyQueue[freeReadPtr->priority], freeReadPtr);
			freeReadPtr->status = RK_READY;
			if (freeReadPtr->priority < runPtr->priority)
				RK_PEND_CTXTSWTCH

		}
	}
	else
	{
		kobj->mailPtr = sendPtr;
	}

	RK_CR_EXIT
	return (RK_SUCCESS);
}
#endif

RK_ERR kMboxPend( RK_MBOX *const kobj, VOID * *recvPPtr, RK_TICK const timeout)
{

	RK_CR_AREA
	RK_CR_ENTER
	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
	}
	if ((kobj == NULL) || (recvPPtr == NULL))
	{
		KERR( RK_FAULT_OBJ_NULL);
	}
	if (kobj->init == FALSE)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
	}
	if (kobj->ownerTask && kobj->ownerTask != runPtr)
	{
		RK_CR_EXIT
		return (RK_ERR_PORT_OWNER);
	}

	if (kobj->mailPtr == NULL)
	{
		if (timeout == 0)
		{
			RK_CR_EXIT
			return (RK_ERR_MBOX_EMPTY);
		}
		runPtr->status = RK_RECEIVING;
		if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);
		}

		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);

		RK_PEND_CTXTSWTCH
		RK_CR_EXIT
		RK_CR_ENTER
		if (runPtr->timeOut)
		{
			runPtr->timeOut = FALSE;
			RK_CR_EXIT
			return (RK_ERR_TIMEOUT);
		}
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
			kRemoveTimeoutNode( &runPtr->timeoutNode);
	}

	*recvPPtr = kobj->mailPtr;
	kobj->mailPtr = NULL;

	/* it only makes sense do deq a writer */
	if (kobj->waitingQueue.size > 0)
	{
		RK_TCB *freeWriterPtr;
		freeWriterPtr = kTCBQPeek( &kobj->waitingQueue);
		if (freeWriterPtr->status == RK_SENDING)
		{
			kTCBQDeq( &kobj->waitingQueue, &freeWriterPtr);
			kTCBQEnq( &readyQueue[freeWriterPtr->priority], freeWriterPtr);
			freeWriterPtr->status = RK_READY;
			if ((freeWriterPtr->priority < runPtr->priority))
			{
				RK_PEND_CTXTSWTCH
			}
		}

	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}
#if (RK_CONF_FUNC_MBOX_QUERY==ON)
ULONG kMboxQuery( RK_MBOX *const kobj)
{
	return ((kobj->mailPtr == NULL) ? 0UL : 1UL);
}
#endif

#if (RK_CONF_FUNC_MBOX_PEEK==ON)
RK_ERR kMboxPeek( RK_MBOX *const kobj, VOID * *peekPPtr)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj == NULL || peekPPtr == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}
	if (!kobj->init)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERROR);
	}
	if (kobj->mailPtr == NULL)
	{
		RK_CR_EXIT
		return (RK_ERR_MBOX_EMPTY);
	}
	*peekPPtr = kobj->mailPtr;
	RK_CR_EXIT
	return (RK_SUCCESS);
}
#endif

#endif /* mailbox */

/*******************************************************************************
 * MAIL QUEUE
 ******************************************************************************/
#if (RK_CONF_QUEUE==ON)

RK_ERR kQueueInit( RK_QUEUE *const kobj, VOID * memPtr,
		ULONG const maxItems)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL || memPtr == NULL || maxItems == 0)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	kobj->mailQPtr = (VOID **) memPtr;
	kobj->bufEndPtr = kobj->mailQPtr + maxItems;
	kobj->headPtr = kobj->mailQPtr;
	kobj->tailPtr = kobj->mailQPtr;
	kobj->maxItems = maxItems;
	kobj->countItems = 0;
	kobj->init = TRUE;
	kobj->objID = RK_MAILQUEUE_KOBJ_ID;
	kobj->ownerTask = NULL;

	RK_ERR listerr = kListInit( &kobj->waitingQueue, "qq");
	kassert( listerr == 0);

	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kQueueSetOwner( RK_QUEUE *const kobj, RK_TASK_HANDLE const taskHandle)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL || taskHandle == NULL)
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}

	kobj->ownerTask = taskHandle;

	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kQueuePost( RK_QUEUE *const kobj, VOID * sendPtr,
		RK_TICK const timeout)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL || sendPtr == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (!kobj->init)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	/*   if queue is full */
	if (kobj->countItems == kobj->maxItems)
	{
		if (timeout == 0)
		{
			RK_CR_EXIT
			return (RK_ERR_QUEUE_FULL);
		}

		if (kobj->ownerTask)
		{
			if (kobj->ownerTask->priority > runPtr->priority)
			{
				kobj->ownerTask->priority = runPtr->priority;
			}
		}

		if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
			kTimeOut( &runPtr->timeoutNode, timeout);
		}

		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
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

		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{
			kRemoveTimeoutNode( &runPtr->timeoutNode);
		}
	}

	*(VOID ***) (kobj->tailPtr) = sendPtr;

	if (kobj->ownerTask != NULL)
		kobj->ownerTask->priority = kobj->ownerTask->realPrio;

	kobj->tailPtr++;
	if (kobj->tailPtr >= kobj->bufEndPtr)
	{
		kobj->tailPtr = kobj->mailQPtr;
	}

	kobj->countItems++;

	if (kobj->waitingQueue.size > 0)
	{
		RK_TCB *freeReadPtr = kTCBQPeek( &kobj->waitingQueue);
		if (freeReadPtr->status == RK_RECEIVING)
		{
			kTCBQDeq( &kobj->waitingQueue, &freeReadPtr);
			kTCBQEnq( &readyQueue[freeReadPtr->priority], freeReadPtr);
			freeReadPtr->status = RK_READY;

			if (freeReadPtr->priority < runPtr->priority)
			{
				RK_PEND_CTXTSWTCH
			}
		}
	}

	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kQueuePend( RK_QUEUE *const kobj, VOID * *recvPPtr, RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL || recvPPtr == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (!kobj->init)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	/*   if queue is empty */
	if (kobj->countItems == 0)
	{
		if (timeout == 0)
		{
			RK_CR_EXIT
			return (RK_ERR_QUEUE_EMPTY);
		}

		if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
			kTimeOut( &runPtr->timeoutNode, timeout);
		}

		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
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

		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{
			kRemoveTimeoutNode( &runPtr->timeoutNode);
		}
	}

	/* get the message from the head position */
	*recvPPtr = *(VOID ***) (kobj->headPtr);

	kobj->headPtr++;
	if (kobj->headPtr >= kobj->bufEndPtr)
	{
		kobj->headPtr = kobj->mailQPtr;
	}

	kobj->countItems--;

	if (kobj->waitingQueue.size > 0)
	{
		RK_TCB *freeSendPtr = kTCBQPeek( &kobj->waitingQueue);
		if (freeSendPtr->status == RK_SENDING)
		{
			kTCBQDeq( &kobj->waitingQueue, &freeSendPtr);
			kTCBQEnq( &readyQueue[freeSendPtr->priority], freeSendPtr);
			freeSendPtr->status = RK_READY;

			if (freeSendPtr->priority < runPtr->priority)
			{
				RK_PEND_CTXTSWTCH
			}
		}
	}

	RK_CR_EXIT
	return (RK_SUCCESS);
}

#if (RK_CONF_FUNC_QUEUE_JAM==ON)

RK_ERR kQueueJam( RK_QUEUE *const kobj, VOID * sendPtr, RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL || sendPtr == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (!kobj->init)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	/*   if queue is full */
	if (kobj->countItems == kobj->maxItems)
	{
		if (timeout == 0)
		{
			RK_CR_EXIT
			return (RK_ERR_QUEUE_FULL);
		}

		if (kobj->ownerTask)
		{
			if (kobj->ownerTask->priority > runPtr->priority)
			{
				runPtr->priority = kobj->ownerTask->priority;
			}
		}

		if ((timeout > 0) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
			kTimeOut( &runPtr->timeoutNode, timeout);
		}

		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
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

		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{
			kRemoveTimeoutNode( &runPtr->timeoutNode);
		}
	}

	/*  jam position (one before head) */
	VOID * *jamPtr;
	if (kobj->headPtr == kobj->mailQPtr)
	{
		/* head is at the start, get back 1 sizeof(VOID **) */
		jamPtr = kobj->bufEndPtr - 1;
	}
	else
	{
		jamPtr = kobj->headPtr - 1;
	}

	/* store the message at the jam position */
	*(VOID ***) jamPtr = sendPtr;

	/*  head pointer <- jam position */
	kobj->headPtr = jamPtr;

	kobj->countItems++;

	if (kobj->ownerTask != NULL)
		kobj->ownerTask->priority = kobj->ownerTask->realPrio;

	if (kobj->waitingQueue.size > 0)
	{
		RK_TCB *freeReadPtr = kTCBQPeek( &kobj->waitingQueue);
		if (freeReadPtr->status == RK_RECEIVING)
		{
			kTCBQDeq( &kobj->waitingQueue, &freeReadPtr);
			kTCBQEnq( &readyQueue[freeReadPtr->priority], freeReadPtr);
			freeReadPtr->status = RK_READY;

			if (freeReadPtr->priority < runPtr->priority)
			{
				RK_PEND_CTXTSWTCH
			}
		}
	}

	RK_CR_EXIT
	return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_QUEUE_PEEK==ON)

RK_ERR kQueuePeek( RK_QUEUE *const kobj, VOID * *peekPPtr)
{
	RK_CR_AREA
	RK_CR_ENTER

	if (kobj == NULL || peekPPtr == NULL)
	{
		KERR( RK_FAULT_OBJ_NULL);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	if (!kobj->init)
	{
		KERR( RK_FAULT_OBJ_NOT_INIT);
		RK_CR_EXIT
		return (RK_ERROR);
	}

	/*   if queue is empty */
	if (kobj->countItems == 0)
	{
		RK_CR_EXIT
		return (RK_ERR_QUEUE_EMPTY);
	}

	/* get the message at the head without removing it */
	*peekPPtr = *(VOID ***) (kobj->headPtr);

	RK_CR_EXIT
	return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_QUEUE_QUERY==ON)

ULONG kQueueQuery( RK_QUEUE *const kobj)
{
	if (kobj == NULL || !kobj->init)
	{
		return 0;
	}

	return kobj->countItems;
}
#endif

#endif /* RK_CONF_QUEUE */

/******************************************************************************/
/* STREAM QUEUE                                                               */
/******************************************************************************/
#if (RK_CONF_STREAM==ON)
#ifndef RK_CPYQ
#define RK_CPYQ(s,d,z)                     \
     do {                               \
        while (--(z))                   \
        {                               \
             *(d)++ = *(s)++;           \
        }                               \
        *(d)++ = *(s)++;                \
        _RK_DMB                             \
 } while(0)
#endif
RK_ERR kStreamInit( RK_STREAM *const kobj, VOID * buf,
		const ULONG mesgSizeInWords, ULONG const nMesg)
{
	RK_CR_AREA

	RK_CR_ENTER

	if ((kobj == NULL) || (buf == NULL))
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (mesgSizeInWords == 0)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_MESG_SIZE);
	}
	if ((mesgSizeInWords != 1UL) && (mesgSizeInWords != 2UL))
	{
		/* allowed sizes, 1, 2, 4, 8... 2^N */
		if (mesgSizeInWords % 4UL != 0UL)
		{
			RK_CR_EXIT
			return (RK_ERR_INVALID_MESG_SIZE);

		}
	}

	if (nMesg == 0)
	{
		RK_CR_EXIT
		return (RK_ERR_INVALID_QUEUE_SIZE);
	}
	ULONG queueCapacityWords = nMesg * mesgSizeInWords;

	kobj->bufPtr = (ULONG*) buf;/* base pointer to the buffer */
	kobj->mesgSize = mesgSizeInWords;/* message size in words */
	kobj->maxMesg = nMesg;/* maximum number of messages */
	kobj->mesgCnt = 0;
	kobj->writePtr = kobj->bufPtr;/* start write pointer */
	kobj->readPtr = kobj->bufPtr;/* start read pointer (same as wrt) */
	kobj->ownerTask = NULL;
	/* end of the buffer in word units */
	kobj->bufEndPtr = kobj->bufPtr + queueCapacityWords;

	RK_ERR err = kListInit( &kobj->waitingQueue, "waitingQueue");
	if (err != 0)
	{
		RK_CR_EXIT
		return RK_ERROR;
	}

	kobj->init = 1;
	kobj->objID = RK_STREAMQUEUE_KOBJ_ID;
	RK_CR_EXIT

	return RK_SUCCESS;
}

RK_ERR kStreamSetOwner( RK_STREAM *const kobj, RK_TASK_HANDLE const taskHandle)
{
	RK_CR_AREA
	RK_CR_ENTER
	if (kobj && taskHandle)
	{
		kobj->ownerTask = taskHandle;
		RK_CR_EXIT
		return (RK_SUCCESS);
	}
	RK_CR_EXIT
	return (RK_ERR_OBJ_NULL);
}

RK_ERR kStreamSend( RK_STREAM *const kobj, VOID * sendPtr,
		const RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	if ((kobj == NULL) || (sendPtr == NULL) || (kobj->init == 0))
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		RK_CR_EXIT
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
	}
	if (kobj->mesgCnt >= kobj->maxMesg)
	{/* Stream full */
		if (timeout == RK_NO_WAIT)
		{
			RK_CR_EXIT
			return (RK_ERR_STREAM_FULL);
		}
		runPtr->status = RK_SENDING;

		if (kobj->ownerTask->priority > runPtr->priority)
		{
			kobj->ownerTask->priority = runPtr->priority;
		}
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);

		}
		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
		RK_PEND_CTXTSWTCH
		RK_CR_EXIT
		RK_CR_ENTER
		if (runPtr->timeOut)
		{
			runPtr->timeOut = FALSE;
			RK_CR_EXIT
			return (RK_ERR_TIMEOUT);
		}
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
			kRemoveTimeoutNode( &runPtr->timeoutNode);
	}

	ULONG size = kobj->mesgSize;/* number of words to copy */
	ULONG *srcPtr = (ULONG*) sendPtr;
	ULONG *dstPtr = kobj->writePtr;
	RK_CPYQ( srcPtr, dstPtr, size);
	/*  wrap-around */
	if (dstPtr == kobj->bufEndPtr)
	{
		dstPtr = kobj->bufPtr;
	}

	kobj->writePtr = dstPtr;

	if (kobj->ownerTask != NULL)
		kobj->ownerTask->priority = kobj->ownerTask->realPrio;

	kobj->mesgCnt++;

	/* unblock a reader, if any */
	if ((kobj->waitingQueue.size > 0) && (kobj->mesgCnt == 1))
	{
		RK_TCB *freeTaskPtr;
		kTCBQDeq( &kobj->waitingQueue, &freeTaskPtr);
		kTCBQEnq( &readyQueue[freeTaskPtr->priority], freeTaskPtr);

		freeTaskPtr->status = RK_READY;
		if (freeTaskPtr->priority < runPtr->priority)
			RK_PEND_CTXTSWTCH
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_ERR kStreamRecv( RK_STREAM *const kobj, VOID * recvPtr,
		const RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	if ((kobj == NULL) || (recvPtr == NULL) || (kobj->init == 0))
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		RK_CR_EXIT
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
	}
	if (kobj->ownerTask && kobj->ownerTask != runPtr)
	{
		RK_CR_EXIT
		return (RK_ERR_PORT_OWNER);
	}
	if (kobj->mesgCnt == 0)
	{
		if (timeout == RK_NO_WAIT)
		{
			RK_CR_EXIT
			return (RK_ERR_STREAM_EMPTY);
		}

		runPtr->status = RK_RECEIVING;

		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);
		}
		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);
		RK_PEND_CTXTSWTCH
		RK_CR_EXIT
		RK_CR_ENTER
		if (runPtr->timeOut)
		{
			runPtr->timeOut = FALSE;
			RK_CR_EXIT
			return (RK_ERR_TIMEOUT);
		}
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
			kRemoveTimeoutNode( &runPtr->timeoutNode);
	}
	ULONG size = kobj->mesgSize;/* number of words to copy */
	ULONG *destPtr = (ULONG*) recvPtr;
	ULONG *srcPtr = kobj->readPtr;
	RK_CPYQ( srcPtr, destPtr, size);
	/* Check for wrap-around on read pointer */
	if (srcPtr == kobj->bufEndPtr)
	{
		srcPtr = kobj->bufPtr;
	}
	kobj->readPtr = srcPtr;
	kobj->mesgCnt--;

	/* Unblock a waiting sender if needed */
	if ((kobj->waitingQueue.size > 0) && (kobj->mesgCnt == (kobj->maxMesg - 1)))
	{
		RK_TCB *freeTaskPtr;
		kTCBQDeq( &kobj->waitingQueue, &freeTaskPtr);
		kTCBQEnq( &readyQueue[freeTaskPtr->priority], freeTaskPtr);
		freeTaskPtr->status = RK_READY;
		if (freeTaskPtr->priority < runPtr->priority)
			RK_PEND_CTXTSWTCH
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}

#if (RK_CONF_FUNC_STREAM_PEEK==ON)
RK_ERR kStreamPeek( RK_STREAM *const kobj, VOID * recvPtr)
{
	RK_CR_AREA
	RK_CR_ENTER
	if ((kobj == NULL) || (recvPtr == NULL) || (kobj->init == 0))
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (kobj->mesgCnt == 0)
	{
		RK_CR_EXIT
		return (RK_ERR_STREAM_EMPTY);
	}

	ULONG size = kobj->mesgSize;/* number of words to copy */
	ULONG *readPtrTemp = kobj->readPtr;/* make a local copy */
	ULONG *dstPtr = (ULONG*) recvPtr;

	RK_CPYQ( readPtrTemp, dstPtr, size);

	RK_CR_EXIT
	return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_STREAM_JAM==ON)

RK_ERR kStreamJam( RK_STREAM *const kobj, VOID * sendPtr,
		const RK_TICK timeout)
{
	RK_CR_AREA
	RK_CR_ENTER
	if ((kobj == NULL) || (sendPtr == NULL) || (kobj->init == 0))
	{
		RK_CR_EXIT
		return (RK_ERR_OBJ_NULL);
	}
	if (RK_IS_BLOCK_ON_ISR( timeout))
	{
		RK_CR_EXIT
		KERR( RK_FAULT_INVALID_ISR_PRIMITIVE);
	}
	if (kobj->mesgCnt >= kobj->maxMesg)
	{/* Stream full */
		if (timeout == RK_NO_WAIT)
		{
			RK_CR_EXIT
			return (RK_ERR_STREAM_FULL);
		}

		runPtr->status = RK_SENDING;

		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
		{
			RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

			kTimeOut( &runPtr->timeoutNode, timeout);
		}

		kTCBQEnqByPrio( &kobj->waitingQueue, runPtr);

		RK_PEND_CTXTSWTCH
		RK_CR_EXIT
		RK_CR_ENTER
		if (runPtr->timeOut)
		{
			runPtr->timeOut = FALSE;
			RK_CR_EXIT
			return (RK_ERR_TIMEOUT);
		}
		if ((timeout > RK_NO_WAIT) && (timeout != RK_WAIT_FOREVER))
			kRemoveTimeoutNode( &runPtr->timeoutNode);
	}

	ULONG size = kobj->mesgSize;/* number of words to copy */
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
		ULONG *srcPtr = (ULONG*) sendPtr;
		RK_CPYQ( srcPtr, newReadPtr, size);
	}
	/* update read pointer */
	kobj->readPtr = newReadPtr;

	kobj->mesgCnt++;

	/* unblock a reader, if any */
	if ((kobj->waitingQueue.size > 0) && (kobj->mesgCnt == 1))
	{
		RK_TCB *freeTaskPtr;
		kTCBQDeq( &kobj->waitingQueue, &freeTaskPtr);
		kTCBQEnq( &readyQueue[freeTaskPtr->priority], freeTaskPtr);
		freeTaskPtr->status = RK_READY;
		if (freeTaskPtr->priority < runPtr->priority)
			RK_PEND_CTXTSWTCH
	}
	RK_CR_EXIT
	return (RK_SUCCESS);
}
#endif

#if (RK_CONF_FUNC_STREAM_QUERY==ON)
ULONG kStreamQuery( RK_STREAM *const kobj)
{
	if (kobj != NULL)
	{
		return (kobj->mesgCnt);
	}
	return (0UL);
}
#endif

#endif /*RK_CONF_STREAM*/

#if (RK_CONF_MRM==ON)
/******************************************************************************/
/* MRM Buffers                                                                */
/******************************************************************************/

RK_ERR kMRMInit( RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
		VOID * mesgPoolPtr, ULONG const nBufs, ULONG const dataSizeWords)
{
	RK_CR_AREA
	RK_CR_ENTER
	RK_ERR err = RK_ERROR;

	err = kMemInit( &kobj->mrmMem, mrmPoolPtr, sizeof(RK_MRM_BUF), nBufs);
	if (!err)
		err = kMemInit( &kobj->mrmDataMem, mesgPoolPtr, dataSizeWords * 4,
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

ULONG kMRMGetSize( RK_MRM *const kobj)
{
	if (kobj != NULL)
		return (kobj->size);
	return (0);
}

RK_MRM_BUF* kMRMReserve( RK_MRM *const kobj)
{

	RK_CR_AREA
	RK_CR_ENTER
	RK_MRM_BUF *allocPtr = NULL;
	if ((kobj->currBufPtr != NULL))
	{
		if ((kobj->currBufPtr->nUsers == 0))
		{
			allocPtr = kobj->currBufPtr;
#ifndef _STRING_H_
			kMemSet( kobj->currBufPtr->mrmData, 0, kobj->size);
#else
            memset((void*) kobj->currBufPtr->mrmData, (int)0, (unsigned long) kobj->size);
            #endif
		}
		else
		{
			allocPtr = kMemAlloc( &kobj->mrmMem);
			if (allocPtr != NULL)
			{
				allocPtr->mrmData = (ULONG*) kMemAlloc( &kobj->mrmDataMem);
			}
		}
	}
	else
	{
		allocPtr = kMemAlloc( &kobj->mrmMem);
		if (allocPtr != NULL)
		{
			allocPtr->mrmData = kMemAlloc( &kobj->mrmDataMem);
		}
	}

	if (!allocPtr)
	{
		kobj->failReserve++;
	}
	RK_CR_EXIT
	return (allocPtr);
}

RK_ERR kMRMPublish( RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
		VOID * pubMesgPtr)
{
	RK_CR_AREA
	if (!kobj->init)
		return (RK_ERR_OBJ_NOT_INIT);
	RK_CR_ENTER
	/* replace current buffer */
	kobj->currBufPtr = bufPtr;
	/* copy to the data buffer (words) */
	ULONG *mrmMesgPtr_ = (ULONG*) kobj->currBufPtr->mrmData;
	ULONG *pubMesgPtr_ = (ULONG*) pubMesgPtr;
	for (UINT i = 0; i < kobj->size; ++i)
	{

		mrmMesgPtr_[i] = pubMesgPtr_[i];
	}

	RK_CR_EXIT
	return (RK_SUCCESS);
}

RK_MRM_BUF* kMRMGet( RK_MRM *const kobj, VOID * getMesgPtr)
{
	RK_CR_AREA
	if (!kobj->init)
		return (NULL);
	if (kobj->currBufPtr)
	{
		RK_CR_ENTER
		RK_MRM_BUF *ret = kobj->currBufPtr;
		kobj->currBufPtr->nUsers++;
		ULONG *getMesgPtr_ = (ULONG*) getMesgPtr;
		ULONG *cabMesgPtr_ = (ULONG*) kobj->currBufPtr->mrmData;
		for (UINT i = 0; i < kobj->size; ++i)
		{

			getMesgPtr_[i] = cabMesgPtr_[i];
		}
		RK_CR_EXIT
		return (ret);
	}
	return (NULL);
}

RK_ERR kMRMUnget( RK_MRM *const kobj, RK_MRM_BUF *const bufPtr)
{
	if (!kobj->init)
		return (RK_ERR_OBJ_NULL);
	RK_ERR err = 0;
	if (bufPtr)
	{
		RK_CR_AREA
		RK_CR_ENTER
		if (bufPtr->nUsers > 0)
			bufPtr->nUsers--;
		/* deallocate if not used and not the curr buf */
		if ((bufPtr->nUsers == 0) && (kobj->currBufPtr != bufPtr))
		{
			ULONG *mrmDataPtr = bufPtr->mrmData;
			kassert( !kMemFree( &kobj->mrmDataMem, (VOID * )mrmDataPtr));
			err = kMemFree( &kobj->mrmMem, (VOID *) bufPtr);

		}
		RK_CR_EXIT
		return (err);
	}
	return (RK_ERR_OBJ_NULL);
}
#endif

