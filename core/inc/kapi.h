/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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

/*******************************************************************************
 *
 *  @file kapi.h 
 * 
 *  RK0 Public API
 *
 *  The is the public RK0 API to be used on the highest development layer.
 *  By default it is included in app/inc/application.h
 *
 *  *** API Conventions: ***
 *
 * Every kernel call starts with a lower-case 'k';
 * A kernel call always acts on a kernel object - such as a mailbox or a task
 * handle. 
 * 
 * e.g., kSemaPost(&sema); 
 * 
 * If the function does not receive a kernel object as a parameter it is 
 * acting on the caller task.
 * 
 * e.g., kSleepUntil(150);
 * 
 * Timeout specific values: RK_NO_WAIT (try-and-return), 
 * 							RK_WAIT_FOREVER (do not expire)
 * 
 * For a list of ERROR CODES (RK_ERR type) look at kdefs.h.
 *
 ******************************************************************************/

#ifndef RK_API_H
#define RK_API_H

#include <kservices.h>
#include <kenv.h>

/******************************************************************************/
/**
 * @brief 			   Create a new task. Task prototype:
 *
 *                     VOID TaskN( VOID *)
 *
 * @param taskHandle   Handle object for the task.
 *
 * @param taskFuncPtr  Pointer to the task entry function.
 *
 * @param taskName     Task name. Keep it as much as 8 Bytes.
 *
 * @param stackAddrPtr Pointer to the task stack (the array variable).
 *
 * @param stackSize    Size of the task stack (in WORDS. 1WORD=4BYTES)
 *
 * @param argsPtr      Pointer to initial task arguments. *
 *
 * @param priority     Task priority - valid range: 0-31.
 *
 * @param runToCompl   Values: RK_PREEMPT / RK_NO_PREEMPT
 * 					   If this parameter is 'RK_NO_PREEMPT', the task once dispatched
 *                     although can be interrupted by tick and other hardware
 *                     interrupt lines, won't be preempted by user tasks until
 * 					   it is READY/WAITING.
 *                     Run-to-completion tasks are normally deferred handlers for ISRs.
 *
 * @return RK_SUCCESS, or specific error
 */
RK_ERR kCreateTask( RK_TASK_HANDLE *taskHandlePtr,
		const RK_TASKENTRY taskFuncPtr, CHAR *const taskName,
		UINT *const stackAddrPtr, const UINT stackSize, VOID *argsPtr,
		const RK_PRIO priority, const BOOL runToCompl);

/**
 * @brief Initialises the kernel. To be called in main()
 *        after hardware initialisation.
 */
VOID kInit( VOID);

/**
 * @brief Yields the current task.
 *
 */
VOID kYield( VOID);

/******************************************************************************/
/* SIGNALS 			                                                          */
/******************************************************************************/
 /** 
 * @brief				A task pends on its own event flags
 * @param required		Combination of required flags (bitstring, non-zero)
 * @param options 		RK_FLAGS_ANY or RK_FLAGS_ALL
 * @param gotFlagsPtr	Pointer to store the flags when returning (opt. NULL)
 * @param timeout  		Suspension timeout, in case required flags are not met
 * @return 				RK_SUCCESS, RK_ERR_FLAGS_NOT_MET or specific error
 */
RK_ERR kSignalGet( ULONG const required, UINT const options, ULONG *const gotFlagsPtr,
	RK_TICK const timeout);

#define kSignalWait (r, p, o, t) kSignalGet(r, p, o, t) 

/**
* @brief 				Post a combination of flags to a task
* @param taskHandle 	Receiver Task handle
* @param mask 			Bitmask to signal (non-zero)
* @return 				RK_SUCCESS or specific error
*/
RK_ERR kSignalSet( RK_TASK_HANDLE const taskHandle, ULONG const mask);

/**
* @brief 				Reads caller task flags
* @param taskHandle 	Target task - use NULL if target is
*						the caller task
* @param gotFlagsPtr 	Pointer to store the current flags
* @return				RK_SUCCESS or specific error.
*/
RK_ERR kSignalQuery( RK_TASK_HANDLE const taskHandle, ULONG *const gotFlagsPtr);

/**
* @brief Clears the caller task flags
* @return RK_SUCCESS or specific error
*/
RK_ERR kSignalClear( VOID);


/******************************************************************************/
/* EVENTS (SLEEP/WAKE/SIGNAL)                                                 */
/******************************************************************************/
#if (RK_CONF_EVENT==ON)
/**
* @brief 			Initialise an event
* @param kobj		Pointer to RK_EVENT object
* @return			RK_SUCCESS/error
*/
RK_ERR kEventInit( RK_EVENT *const kobj);
/**
* @brief 			Suspends a task waiting for a wake signal
* @param kobj 		Pointer to a RK_EVENT object
* @param timeout	Suspension time.
* @return				RK_SUCCESS or specific error.
*/
RK_ERR kEventSleep( RK_EVENT *const kobj, const RK_TICK timeout);

/**
* @brief 		Broadcast signal for an Event 
* @param kobj 	Pointer to a RK_EVENT object
* @param nTask		Number of taks to wake (0 if all)
* @param uTasksPtr	Pointer to store the actual number
* 					of waken tasks (opt. NULL)
* @return 		RK_SUCCESS or specific error
*/
RK_ERR kEventWake(RK_EVENT *const kobj, UINT nTasks, UINT *uTasksPtr);
#define kEventFlush(p) kEventWake(p, 0, NULL)

/**
* @brief 		Wakes a single task sleeping for a specific event
*        		(by priority)
* @param kobj 	Pointer to a RK_EVENT object
* @return 		RK_SUCCESS or specific error
*/
RK_ERR kEventSignal( RK_EVENT *const kobj);

/**
* @brief  Return the number of tasks sleeping on an event.
* @return Number of sleeping tasks; (-1 means error)
*/
INT kEventQuery( RK_EVENT *const kobj);

#endif

/*******************************************************************************/
/* COUNTER SEMAPHORE                                                           */
/*******************************************************************************/
#if (RK_CONF_SEMA==ON)
/**
 * @brief      			Initialise a semaphore
 * @param kobj  		Semaphore address
 * @param semaType		Counter (RK_SEMA_COUNTER) or Binary (RK_SEMA_BIN)
 * @param value 		Initial value (>= 0)
 * @return  			RK_SUCCESS, or specific error
 */

RK_ERR kSemaInit( RK_SEMA *const kobj, UINT const semaType, const INT value);
#define kSemaCounterInit(p,v) kSemaInit(p, RK_SEMA_COUNTER, v)
#define kSemaBinInit(p, v) kSemaInit(p, RK_SEMA_BIN, v)

/**
 * @brief 			Wait on a semaphore
 * @param kobj 		Semaphore address
 * @param timeout	Maximum suspension time
 * @return   		RK_SUCCESS, or specific error
 */
RK_ERR kSemaPend( RK_SEMA *const kobj, const RK_TICK timeout);

#define kSemaWait(p, t) kSemaPend(p, t) /* alias */

/**
 * @brief 			Signal a semaphore
 * @param kobj 		Semaphore address
 * @return 			RK_SUCCESS, or specific error
 */
VOID kSemaPost( RK_SEMA *const kobj);
#define kSemaSignal(p) kSemaPost(p) 

/**
 * @brief 			Broadcast Signal to a semaphore
 * @param kobj 		Semaphore address
 * @param nTask		Number of taks to wake (0 to all) 
 * @param uTasksPtr	Pointer to store the actual number
 * 					of waken tasks (opt. NULL)
 * @return 			RK_SUCCESS, or specific error
 */
RK_ERR kSemaWake( RK_SEMA *const kobj, UINT nTasks, UINT *uTasksPtr);
#define kSemaFlush(p) kSemaWake(p, 0, NULL)

/**
 * @brief 		Return the counter's value of a semaphore
 * @param kobj  Semaphore address
 * @return      Counter's value, (number of blocked tasks if < 0)
 *             	RK_INT_MAX if error
 */
INT kSemaQuery( RK_SEMA *const kobj);


#endif
/*******************************************************************************/
/* MUTEX SEMAPHORE                                                             */
/*******************************************************************************/
#if (RK_CONF_MUTEX==ON)
/**
 * @brief 		Init a mutex
 * @param kobj 	mutex address
 * @return 		RK_SUCCESS or specific error
 */
RK_ERR kMutexInit( RK_MUTEX *const kobj);


/**
 * @brief 			Lock a mutex
 * @param kobj 		mutex address
 * @param prioInh	Apply priority inheritance (RK_INHERIT/RK_NO_INHERIT)
 * @param timeout	Maximum suspension time
 * @return 			RK_SUCCESS, or specific error
 */
RK_ERR kMutexLock( RK_MUTEX *const kobj, BOOL const prioInh, RK_TICK const timeout);

/**
 * @brief 			Unlock a mutex
 * @param kobj 		mutex address
 * @return 			RK_SUCCESS,  or specific error
 */
RK_ERR kMutexUnlock( RK_MUTEX *const kobj);

/**
 * @brief Return the state of a mutex (locked/unlocked)
 * @return 1 if locked, 0 unlocked, -1 if invalid mutex state
 */
INT kMutexQuery( RK_MUTEX *const kobj);

#endif

/*******************************************************************************/
/* MAILBOX (SINGLE-ITEM MAILBOX)                                               */
/*******************************************************************************/
#if (RK_CONF_MBOX == ON)

/**
 * @brief               Initialises an indirect single mailbox.
 * @param kobj          Mailbox address.
 * @param initMailPtr   If initialising full, address of initial mail.
 *  					Otherwise NULL.
 * @return              RK_SUCCESS or specific error.
 */

RK_ERR kMboxInit( RK_MBOX *const kobj, VOID *const initMailPtr);

/**
 * @brief            Assigns a task owner for the mailbox
 * @param kobj       Mailbox address
 * @param taskHandle Task Handle
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kMboxSetOwner( RK_MBOX *const kobj, const RK_TASK_HANDLE taskHandle);

/**
 * @brief               Send to a mailbox. 
 * @param kobj          Mailbox address.
 * @param sendPtr       Mail address.
 * @param timeout       Suspension time-out
 * @return              RK_SUCCESS or specific error.
 */
RK_ERR kMboxPost( RK_MBOX *const kobj, VOID *sendPtr, RK_TICK const timeout);
/**
 * @brief               Receive from a mailbox. 
 *
 * @param kobj          Mailbox address.
 * @param recvPPtr      Address that will store the message address (pointer-to-pointer).
 * @param timeout		Suspension time-out
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMboxPend( RK_MBOX *const kobj, VOID **recvPPtr, RK_TICK const timeout);

#if (RK_CONF_FUNC_MBOX_POSTOVW==(ON))

/**
 * @brief			Post to a mailbox even if it is full, overwriting the
 *                  the current mail.
 * @param kobj		Mailbox address.
 * @param sendPtr   Mail address.
 * @return          RK_SUCCESS or specific error
 */
RK_ERR kMboxPostOvw( RK_MBOX *const kobj, VOID *sendPtr);

#endif

#if (RK_CONF_FUNC_MBOX_PEEK==ON)

/**
 * @brief 			   Reads the mail without extracting it.
 * @param kobj		   Mailbox address.
 * @param peekPPtr	   Pointer to receive address.
 * @return			   RK_SUCCESS or specific error.
 */
RK_ERR kMboxPeek( RK_MBOX *const kobj, VOID **peekPPtr);

#endif

#if (RK_CONF_FUNC_MBOX_QUERY==ON)
/**
 * @brief   Verify if a mailbox is FULL or EMPTY
 * @return  1 (FULL), 0 (EMPTY), -1 (ERROR)
 */
INT kMboxQuery( RK_MBOX const * const kobj);

#endif

#endif /* MBOX  */
/******************************************************************************/
/* MAIL QUEUES                                                                */
/******************************************************************************/
#if (RK_CONF_QUEUE == ON)

/**
 * @brief			 Initialises a mail queue.
 * @param kobj		 Mail Queue address
 * @param bufPtr     Pointer to the buffer that will store mail addresses
 * @param maxItems   Maximum number of mails.
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kQueueInit( RK_QUEUE *const kobj, VOID *bufPtr,
		const ULONG maxItems);

/**
 * @brief            Assigns a task owner for the queue
 * @param kobj       Mail Queue address
 * @param memPtr     Task Handle
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kQueueSetOwner( RK_QUEUE *const kobj, const RK_TASK_HANDLE taskHandle);

/**
 * @brief               Send to a multilbox.  
 * @param kobj          Mail Queue address.
 * @param sendPtr       Mail address.
 * @param timeout		Suspension time-out
 * @return              RK_SUCCESS or specific error.
 */

RK_ERR kQueuePost( RK_QUEUE *const kobj, VOID *sendPtr,
		RK_TICK const timeout);

/**
 * @brief               Receive from a mail queue. 
 *
 * @param kobj          Mail Queue address.
 * @param recvPPtr      Address that will store the message address
 * 					  (pointer-to-pointer).
 * @param timeout		Suspension time-out
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kQueuePend( RK_QUEUE *const kobj, VOID **recvPPtr, RK_TICK const timeout);

#if (RK_CONF_FUNC_QUEUE_PEEK==ON)

/**
 * @brief 			   Reads the head's mail without extracting it.
 * @param kobj		   Mail Queue address.
 * @param peekPPtr	   Pointer to receive address.
 * @return			   RK_SUCCESS or specific error.
 */
RK_ERR kQueuePeek( RK_QUEUE *const kobj, VOID **peekPPtr);

#endif

#if (RK_CONF_FUNC_QUEUE_QUERY==ON)
/**
 * @brief			Gets the current number of mails within a queue.
 * @param kobj      Mail Queue address.
 * @return			Number of mails. (-1 if error)
 */
INT kQueueQuery( RK_QUEUE const * const kobj);

#endif
#if (RK_CONF_FUNC_QUEUE_JAM==ON)

/**
 * @brief            Sends a message to the queue front.
 * @param kobj       Queue address
 * @param sendPtr    Message address
 * @param timeout    Suspension time
 * @return           RK_SUCCESS or specific error
 */
RK_ERR kQueueJam( RK_QUEUE *const kobj, VOID *sendPtr, RK_TICK const timeout);

#endif
#endif /* MAIL QUEUE  */

#if (RK_CONF_STREAM == ON)

/******************************************************************************/
/* STREAM QUEUE                                                               */
/******************************************************************************/

/**
 * @brief 					 Initialise a Stream MessageQueue
 * @param kobj			  	 Stream Queue address
 * @param bufPtr		 	 Allocated memory.
 * @param mesgSizeInWords 	 Message size (min=1WORD)
 * @param nMesg  			 Max number of messages
 * @return 					 RK_SUCCESS or specific errors
 */
RK_ERR kStreamInit( RK_STREAM *const kobj, VOID *bufPtr,
		const ULONG mesgSizeInWords, const ULONG nMesg);

/**
 * @brief            Assigns a task owner for the stream queue
 * @param kobj       Stream Queue address
 * @param taskHandle Task Handle
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kStreamSetOwner( RK_STREAM *const kobj, const RK_TASK_HANDLE taskHandle);

#if (RK_CONF_FUNC_STREAM_QUERY==ON)

/**
 * @brief 			Get the current number of messages within a message queue.
 * @param kobj		(Stream) Queue address
 * @return			Number of messages in the queue.
 * 					(-1 if error)
 */

INT kStreamQuery( RK_STREAM *const kobj);

#endif

#if (RK_CONF_FUNC_STREAM_JAM == ON)

/**
 * @brief 			Sends a message to the queue front.
 * @param kobj		(Stream) Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 * @return			RK_SUCCESS or specific error
 */
RK_ERR kStreamJam( RK_STREAM *const kobj, VOID *sendPtr,
		const RK_TICK timeout);

#endif

/**
 * @brief 			Receive a message from the queue
 * @param kobj		Queue address
 * @param recvPtr	Receiving address
 * @param timeout	Suspension time
*  @return			RK_SUCCESS or specific error.
 */
RK_ERR kStreamRecv( RK_STREAM *const kobj, VOID *recvPtr, const RK_TICK timeout);

/**
 * @brief 			Send a message to a message queue
 * @param kobj		Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
*  @return			RK_SUCCESS or specific error.
 */
RK_ERR kStreamSend( RK_STREAM *const kobj, VOID *sendPtr,
		const RK_TICK timeout);

#if (RK_CONF_FUNC_STREAM_PEEK==ON)

/**
 * @brief 			Receive the front message of a queue
 *					without changing its state
 * @param kobj		(Stream) Queue object address
 * @param recvPtr	Receiving pointer 
 * @return			RK_SUCCESS or error.
 */
RK_ERR kStreamPeek( RK_STREAM const * const kobj, VOID *recvPtr);

#endif

#endif /*RK_CONF_STREAM*/

/******************************************************************************/
/* MOST-RECENT MESSAGE PROTOCOL                                               */
/******************************************************************************/
#if (RK_CONF_MRM==ON)
/**
 * @brief			 	Initialise a MRM Control Block
 * @param kobj 			Pointer to a MRM Control Block
 * @param mrmPoolPtr  	Pool of MRM buffers
 * @param mesgPoolPtr 	Pool of message buffers (to be attached to a MRM Buffer)
 * @param nBufs 		Number of MRM Buffers (that is the same as the number of messages)
 * @param dataSizeWords Size of a Messsage within a MRM (in WORDS)
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMRMInit( RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
		VOID *mesgPoolPtr, ULONG const nBufs, ULONG const dataSizeWords);

/**
 * @brief		Reserves a MRM Buffer to be written
 * @param kobj	Pointer to a MRM Control Block
 * @return 		Pointer to a MRM Buffer
 */
RK_MRM_BUF* kMRMReserve( RK_MRM *const kobj);

/**
 * @brief 			Copies a message into a MRM and makes it the most recent message.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to a MRM Buffer
 * @param dataPtr   Pointer to the message to be published.
 * @return 			RK_SUCCESS or specific error
 */
RK_ERR kMRMPublish( RK_MRM *const kobj, RK_MRM_BUF *const bufPtr, VOID const *dataPtr);

/**
 * @brief 			Receives the most recent published message within a MRM Block.
 * @param kobj      Pointer to a MRM Control Block
 * @param dataPtr   Pointer to where the message will be copied.
 * @return 			Pointer to the MRM from which message was retrieved
 *        		    (to be used afterwards on kMRMUnget()).
 */
RK_MRM_BUF* kMRMGet( RK_MRM *const kobj, VOID *getMesgPtr);

/**
 * @brief 			Releases a MRM Buffer which message has been consumed.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to the MRM Buffer (returned by kMRMGet())
 * @return 			RK_SUCCESS or specific error
 */
RK_ERR kMRMUnget( RK_MRM *const kobj, RK_MRM_BUF *const bufPtr);

#endif

#if (RK_CONF_CALLOUT_TIMER==ON)
/******************************************************************************/
/* APPLICATION TIMER                                                          */
/******************************************************************************/
/**
 * @brief Initialises an application timer
 * @param kobj  Timer Object address
 * @param phase Initial phase delay (does not applied on reload)
 * @param countTicks Time until it expires in ticks
 * @param funPtr Callout Function when it expires (callback)
 * @param argsPtr Generic pointer to callout arguments
 * @param reload TRUE for reloading after timer-out. FALSE for an one-shot
 * @return		 RK_SUCCESS or specific error.
 */
RK_ERR kTimerInit( RK_TIMER *const kobj, const RK_TICK phase,
		const RK_TICK countTicks, const RK_TIMER_CALLOUT funPtr,
		VOID *argsPtr, const BOOL reload);

/**
 * @brief 		Cancel an active timer
 * @param kobj  Timer object address
 * @return 		RK_SUCCESS, RK_ERR_OBJ_NULL, 
 * 		   		RK_ERROR if invalid Timer object
 * 		   		(e.g., a timer already cancelled) 
 */
RK_ERR kTimerCancel( RK_TIMER *const kobj);
#endif

/******************************************************************************/
/* SLEEP AND OTHER TIME RELATED                                               */
/******************************************************************************/
/**
 * @brief 		Put the current task to sleep for a number of ticks.
 *        		Task switches to SLEEPING state.
 * @param ticks Number of ticks to sleep
 * @return 		RK_SUCCESS or specific error
 */
RK_ERR kSleep( const RK_TICK ticks);

 
/**
 * @brief	Sleep for an absolute period of time adjusting for
 * 			eventual jitters, suitable for periodic tasks.
 * @param	period Period in ticks
 * @return	RK_SUCCESS or specific error.
 */
RK_ERR kSleepUntil( RK_TICK const period);

 /**
 * @brief Gets the current number of  ticks
 * @return Global system tick value
 */
RK_TICK kTickGet( VOID);

/**
 * @brief 	Active wait for a number of ticks 
 * @param	ticks Number of ticks for busy-wait
 * @return 	RK_SUCCESS or RK_INVALID_PARAM
 */
RK_ERR kBusyWait( RK_TICK const ticks);

/******************************************************************************/
/* MEMORY POOL (ALLOCATOR)                                                    */
/******************************************************************************/
/**
 * @brief Memory Pool Control Block Initialisation
 * @param kobj Pointer to a pool control block
 * @param memPoolPtr Address of a pool (typically an array) @
  * 		  of objects to be handled
 * @param blkSize Size of each block in bytes
 * @param numBlocks Number of blocks
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMemInit( RK_MEM *const kobj, VOID *memPoolPtr, ULONG blkSize,
		const ULONG numBlocks);

/**
 * @brief Allocate memory from a block pool
 * @param kobj Pointer to the block pool
 * @return Pointer to the allocated block, or NULL on failure
 */
VOID *kMemAlloc( RK_MEM *const kobj);

/**
 * @brief Free a memory block back to the block pool
 * @param kobj Pointer to the block pool
 * @param blockPtr Pointer to the block to free
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMemFree( RK_MEM *const kobj, VOID *blockPtr);


/******************************************************************************/
/* MISC/HELPERS                                                               */
/******************************************************************************/
/**
 * @brief Returns the kernel version.
 * @return Kernel version as an unsigned integer.
 */
unsigned int kGetVersion( void);

#if !defined(UNUSED)
#define UNUSED(x) (void)x
#endif

/* Running Task Get */
extern RK_TCB *runPtr;
/* Convenience Macros */
#define RK_RUNNING_PID (runPtr->pid)
#define RK_RUNNING_PRIO (runPtr->priority)
#define RK_RUNNING_HANDLE (runPtr)
#define RK_RUNNING_NAME (runPtr->taskName)
#define RK_TASK_PID(taskHandle) (taskHandle->pid)
#define RK_TASK_NAME(taskHandle) (taskHandle->taskName)
#define RK_TASK_PRIO(taskHandle) (taskHandle->priority)

/* Enable/Disable global interrupts */
/* Note: use this on application-level only.
 * If tweaking kernel code, look at RK_CR_*
 * system macros.
 */
__RK_INLINE
static inline VOID kDisableIRQ( VOID)
{
	__ASM volatile ("CPSID I" : : : "memory");
}

__RK_INLINE
static inline VOID kEnableIRQ( VOID)
{
	__ASM volatile ("CPSIE I" : : : "memory");
}

#endif /* KAPI_H */
