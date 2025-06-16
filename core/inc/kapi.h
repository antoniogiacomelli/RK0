/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.1
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
 *                     VOID taskFunc(VOID *args)
 *
 * @param taskHandlePtr Pointer to the Handle object for the task.
 *
 * @param taskFunc     Task's entry function.
 *
 * @param argsPtr      Pointer to initial task arguments.
 *
 * @param taskName     Task name. Standard size is 8 bytes. (RK_MAX_NAME)
 *
 * @param stackPtr     Pointer to the task stack (the array's name).
 *
 * @param stackSize    Size of the task stack (in WORDS. 1WORD=4BYTES)
 *
 * @param priority     Task priority - valid range: 0-31.
 *
 * @param preempt   Values: RK_PREEMPT / RK_NO_PREEMPT
 * 					If this parameter is 'RK_NO_PREEMPT', the task once
 *					dispatched although can be interrupted by tick and
 *				    other hardware interrupt lines, won't be preempted by
 *					user tasks until it is READY/WAITING.
 *                  Non-preemptible tasks are normally deferred handlers
 * 					for high-priority ISRs.
 *
 * @return RK_SUCCESS, or specific error
 */
RK_ERR kCreateTask(RK_TASK_HANDLE *taskHandlePtr,
                   const RK_TASKENTRY taskFunc, VOID *argsPtr,
                   CHAR *const taskName, RK_STACK *const stackPtr,
                   const UINT stackSize, const RK_PRIO priority,
                   const ULONG preempt);

/**
 * @brief Initialises the kernel. To be called in main()
 *        after hardware initialisation.
 */
VOID kInit(VOID);

/**
 * @brief Yields the current task.
 */
VOID kYield(VOID);

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
RK_ERR kSignalGet(ULONG const required, UINT const options,
                  ULONG *const gotFlagsPtr,
                  RK_TICK const timeout);

#define kSignalWait (r, p, o, t) kSignalGet(r, p, o, t)

/**
 * @brief 				Post a combination of flags to a task
 * @param taskHandle 	Receiver Task handle
 * @param mask 			Bitmask to signal (non-zero)
 * @return 				RK_SUCCESS or specific error
 */
RK_ERR kSignalSet(RK_TASK_HANDLE const taskHandle, ULONG const mask);

/**
 * @brief 				Reads caller task flags
 * @param taskHandle 	Target task - use NULL if target is
 *						the caller task
 * @param gotFlagsPtr 	Pointer to store the current flags
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kSignalQuery(RK_TASK_HANDLE const taskHandle, ULONG *const gotFlagsPtr);

/**
 * @brief Clears the caller task flags
 * @return RK_SUCCESS or specific error
 */
RK_ERR kSignalClear(VOID);

/******************************************************************************/
/* EVENTS (SLEEP/WAKE/SIGNAL)                                                 */
/******************************************************************************/
#if (RK_CONF_EVENT == ON)
/**
 * @brief 			Initialise an event
 * @param kobj		Pointer to RK_EVENT object
 * @return			RK_SUCCESS/error
 */
RK_ERR kEventInit(RK_EVENT *const kobj);
/**
 * @brief 			Suspends a task waiting for a wake signal
 * @param kobj 		Pointer to a RK_EVENT object
 * @param timeout	Suspension time.
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kEventSleep(RK_EVENT *const kobj, const RK_TICK timeout);

/**
 * @brief 		Broadcast signal for an Event
 * @param kobj 	Pointer to a RK_EVENT object
 * @param nTask		Number of taks to wake (0 if all)
 * @param uTasksPtr	Pointer to store the number
 * 					of unreleased tasks, if any (opt. NULL)
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
RK_ERR kEventSignal(RK_EVENT *const kobj);

/**
 * @brief  Retrieves the number of tasks sleeping on an event.
 * @param  nTasksPtr Pointer to where store the value
 * @return RK_SUCCESS or specific error.
 */
RK_ERR kEventQuery(RK_EVENT const *const kobj, ULONG *const nTasksPtr);

#endif

/******************************************************************************/
/* SEMAPHORES (COUNTING/BINARY)                                               */
/******************************************************************************/
#if (RK_CONF_SEMA == ON)
/**
 * @brief      			Initialise a semaphore
 * @param kobj  		Semaphore address
 * @param semaType		Counting (RK_SEMA_COUNT) or Binary (RK_SEMA_BIN)
 * @param value 		Initial value (>= 0)
 * @return  			RK_SUCCESS, or specific error
 */

RK_ERR kSemaInit(RK_SEMA *const kobj, UINT const semaType, const UINT value);
#define kSemaCountInit(p, v) kSemaInit(p, RK_SEMA_COUNT, v)
#define kSemaBinInit(p, v) kSemaInit(p, RK_SEMA_BIN, v)

/**
 * @brief 			Wait on a semaphore
 * @param kobj 		Semaphore address
 * @param timeout	Maximum suspension time
 * @return   		RK_SUCCESS, or specific error
 */
RK_ERR kSemaPend(RK_SEMA *const kobj, const RK_TICK timeout);

#define kSemaWait(p, t) kSemaPend(p, t) /* alias */

/**
 * @brief 			Signal a semaphore
 * @param kobj 		Semaphore address
 * @return 			RK_SUCCESS, or specific error
 */
RK_ERR kSemaPost(RK_SEMA *const kobj);
#define kSemaSignal(p) kSemaPost(p)

/**
 * @brief 			Broadcast Signal to a semaphore
 * @param kobj 		Semaphore address
 * @param nTask		Number of taks to wake (0 to all)
 * @param uTasksPtr	Pointer to store the number of
 * 					unreleased tasks, if any. (Optional)
 * @return 			RK_SUCCESS, or specific error
 */
RK_ERR kSemaWake(RK_SEMA *const kobj, UINT const nTasks, UINT *const uTasksPtr);
#define kSemaFlush(p) kSemaWake(p, 0, NULL)

/**
 * @brief 		 	Retrieve the counter's value of a semaphore
 * @param  kobj  	Semaphore address
 * @param  countPtr Pointer to store the semaphore's counter value.
 * 					A negative value means the number of blocked
 * 					tasks.
 * @return       RK_SUCCESS or specific error.
 *
 */
RK_ERR kSemaQuery(RK_SEMA const *const kobj, INT *const countPtr);

#endif
/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/
#if (RK_CONF_MUTEX == ON)
/**
 * @brief 		Init a mutex
 * @param kobj 	mutex address
 * @param prioInh Use priority inheritance
 * @return 		RK_SUCCESS or specific error
 */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT prioInh);

/**
 * @brief 			Lock a mutex
 * @param kobj 		mutex address
 * @param timeout	Maximum suspension time
 * @return 			RK_SUCCESS, or specific error
 */
RK_ERR kMutexLock(RK_MUTEX *const kobj, RK_TICK const timeout);

/**
 * @brief 			Unlock a mutex
 * @param kobj 		mutex address
 * @return 			RK_SUCCESS,  or specific error
 */
RK_ERR kMutexUnlock(RK_MUTEX *const kobj);

/**
 * @brief Retrieves the state of a mutex (locked/unlocked)
 * @param statePtr Pointer to store the retrieved state
 * 				   (0 unlocked, 1 locked)
 * @return RK_SUCCESS or specific error
 */
RK_ERR kMutexQuery(RK_MUTEX const *const kobj, UINT *const statePtr);

#endif

/******************************************************************************/
/* MAILBOX (SINGLE-ITEM MAILBOX)                                              */
/******************************************************************************/
#if (RK_CONF_MBOX == ON)

/**
 * @brief               Initialises an indirect single mailbox.
 * @param kobj          Mailbox address.
 * @param initMailPtr   If initialising full, address of initial mail.
 *  					Otherwise NULL.
 * @return              RK_SUCCESS or specific error.
 */

RK_ERR kMboxInit(RK_MBOX *const kobj, VOID *const initMailPtr);

/**
 * @brief            Assigns a task owner for the mailbox
 * @param kobj       Mailbox address
 * @param taskHandle Task Handle
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kMboxSetOwner(RK_MBOX *const kobj, const RK_TASK_HANDLE taskHandle);

/**
 * @brief               Send to a mailbox.
 * @param kobj          Mailbox address.
 * @param sendPtr       Mail address.
 * @param timeout       Suspension time-out
 * @return              RK_SUCCESS or specific error.
 */
RK_ERR kMboxPost(RK_MBOX *const kobj, VOID *sendPtr, RK_TICK const timeout);
/**
 * @brief               Receive from a mailbox.
 *
 * @param kobj          Mailbox address.
 * @param recvPPtr      Address that will store the message address
 * 						(pointer-to-pointer).
 * @param timeout		Suspension time-out
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMboxPend(RK_MBOX *const kobj, VOID **recvPPtr, RK_TICK const timeout);

#if (RK_CONF_FUNC_MBOX_POSTOVW == (ON))

/**
 * @brief			Post to a mailbox even if it is full, overwriting the
 *                  the current mail.
 * @param kobj		Mailbox address.
 * @param sendPtr   Mail address.
 * @return          RK_SUCCESS or specific error
 */
RK_ERR kMboxPostOvw(RK_MBOX *const kobj, VOID *sendPtr);

#endif

#if (RK_CONF_FUNC_MBOX_PEEK == ON)

/**
 * @brief 			   Reads the mail without extracting it.
 * @param kobj		   Mailbox address.
 * @param peekPPtr	   Pointer to receive address.
 * @return			   RK_SUCCESS or specific error.
 */
RK_ERR kMboxPeek(RK_MBOX *const kobj, VOID **peekPPtr);

#endif

#if (RK_CONF_FUNC_MBOX_QUERY == ON)
/**
 * @brief   Retrieves the state of mailbox
 * @param   statePtr Pointer to store the state
 *          (1 = FULL, 0 = EMPTY)
 * @return  RK_SUCCESS or specific error.
 */
RK_ERR kMboxQuery(RK_MBOX const *const kobj, UINT *const statePtr);

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
RK_ERR kQueueInit(RK_QUEUE *const kobj, VOID *bufPtr,
                  const ULONG maxItems);

/**
 * @brief            Assigns a task owner for the queue
 * @param kobj       Mail Queue address
 * @param memPtr     Task Handle
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kQueueSetOwner(RK_QUEUE *const kobj, const RK_TASK_HANDLE taskHandle);

/**
 * @brief               Send to a multilbox.
 * @param kobj          Mail Queue address.
 * @param sendPtr       Mail address.
 * @param timeout		Suspension time-out
 * @return              RK_SUCCESS or specific error.
 */

RK_ERR kQueuePost(RK_QUEUE *const kobj, VOID *sendPtr,
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
RK_ERR kQueuePend(RK_QUEUE *const kobj, VOID **recvPPtr, RK_TICK const timeout);

#if (RK_CONF_FUNC_QUEUE_PEEK == ON)

/**
 * @brief 			   Reads the head's mail without extracting it.
 * @param kobj		   Mail Queue address.
 * @param peekPPtr	   Pointer to receive address.
 * @return			   RK_SUCCESS or specific error.
 */
RK_ERR kQueuePeek(RK_QUEUE *const kobj, VOID **peekPPtr);

#endif

#if (RK_CONF_FUNC_QUEUE_QUERY == ON)
/**
 * @brief			Retrieves the current number of mails within a queue.
 * @param kobj      Mail Queue address.
 * @param nMailPtr  Pointer to store the number of mails.
 * @return			RK_SUCCESS or specific error.
 */
RK_ERR kQueueQuery(RK_QUEUE const *const kobj, UINT *const nMailPtr);

#endif
#if (RK_CONF_FUNC_QUEUE_JAM == ON)

/**
 * @brief            Sends a message to the queue front.
 * @param kobj       Queue address
 * @param sendPtr    Message address
 * @param timeout    Suspension time
 * @return           RK_SUCCESS or specific error
 */
RK_ERR kQueueJam(RK_QUEUE *const kobj, VOID *sendPtr, RK_TICK const timeout);

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
RK_ERR kStreamInit(RK_STREAM *const kobj, VOID *bufPtr,
                   const ULONG mesgSizeInWords, const ULONG nMesg);

/**
 * @brief            Assigns a task owner for the stream queue
 * @param kobj       Stream Queue address
 * @param taskHandle Task Handle
 * @return           RK_SUCCESS or specific error.
 */
RK_ERR kStreamSetOwner(RK_STREAM *const kobj, const RK_TASK_HANDLE taskHandle);

#if (RK_CONF_FUNC_STREAM_QUERY == ON)

/**
 * @brief 			Retrieves the current number of messages
 * 					within a message queue.
 * @param kobj		(Stream) Queue address
 * @param nMesgPtr  Pointer to store the retrieved number.
 * @return			RK_SUCCESS or specific error.
 */

RK_ERR kStreamQuery(RK_STREAM const *const kobj, UINT *const nMesgPtr);

#endif

#if (RK_CONF_FUNC_STREAM_JAM == ON)

/**
 * @brief 			Sends a message to the queue front.
 * @param kobj		(Stream) Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 * @return			RK_SUCCESS or specific error
 */
RK_ERR kStreamJam(RK_STREAM *const kobj, VOID *sendPtr,
                  const RK_TICK timeout);

#endif

/**
 * @brief 			Receive a message from the queue
 * @param kobj		Queue address
 * @param recvPtr	Receiving address
 * @param timeout	Suspension time
 *  @return			RK_SUCCESS or specific error.
 */
RK_ERR kStreamRecv(RK_STREAM *const kobj, VOID *recvPtr, const RK_TICK timeout);

/**
 * @brief 			Send a message to a message queue
 * @param kobj		Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 *  @return			RK_SUCCESS or specific error.
 */
RK_ERR kStreamSend(RK_STREAM *const kobj, VOID *sendPtr,
                   const RK_TICK timeout);

#if (RK_CONF_FUNC_STREAM_PEEK == ON)

/**
 * @brief 			Receive the front message of a queue
 *					without changing its state
 * @param kobj		(Stream) Queue object address
 * @param recvPtr	Receiving pointer
 * @return			RK_SUCCESS or error.
 */
RK_ERR kStreamPeek(RK_STREAM const *const kobj, VOID *recvPtr);

#endif

#endif /*RK_CONF_STREAM*/

/******************************************************************************/
/* MOST-RECENT MESSAGE PROTOCOL                                               */
/******************************************************************************/
#if (RK_CONF_MRM == ON)
/**
 * @brief			 	Initialise a MRM Control Block
 * @param kobj 			Pointer to a MRM Control Block
 * @param mrmPoolPtr  	Pool of MRM buffers
 * @param mesgPoolPtr 	Pool of message buffers (to be attached to a MRM Buffer)
 * @param nBufs 		Number of MRM Buffers
 * 						(that is the same as the number of messages)
 * @param dataSizeWords Size of a Messsage within a MRM (in WORDS)
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMRMInit(RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
                VOID *mesgPoolPtr, ULONG const nBufs,
                ULONG const dataSizeWords);

/**
 * @brief		Reserves a MRM Buffer to be written
 * @param kobj	Pointer to a MRM Control Block
 * @return 		Pointer to a MRM Buffer
 */
RK_MRM_BUF *kMRMReserve(RK_MRM *const kobj);

/**
 * @brief 			Copies a message into a MRM and makes it the most recent
 * message.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to a MRM Buffer
 * @param dataPtr   Pointer to the message to be published.
 * @return 			RK_SUCCESS or specific error
 */
RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
                   VOID *const dataPtr);

/**
 * @brief 			Receives the most recent published message within a
 * MRM Block.
 * @param kobj      Pointer to a MRM Control Block
 * @param getMesgPtr   Pointer to where the message will be copied.
 * @return 			Pointer to the MRM from which message was retrieved
 *        		    (to be used afterwards on kMRMUnget()).
 */
RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *getMesgPtr);

/**
 * @brief 			Releases a MRM Buffer which message has been consumed.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to the MRM Buffer (returned by kMRMGet())
 * @return 			RK_SUCCESS or specific error
 */
RK_ERR kMRMUnget(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr);

#endif

#if (RK_CONF_CALLOUT_TIMER == ON)
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
RK_ERR kTimerInit(RK_TIMER *const kobj, const RK_TICK phase,
                  const RK_TICK countTicks, const RK_TIMER_CALLOUT funPtr,
                  VOID *argsPtr, const BOOL reload);

/**
 * @brief 		Cancel an active timer
 * @param kobj  Timer object address
 * @return 		RK_SUCCESS, RK_ERR_OBJ_NULL,
 * 		   		RK_ERROR if invalid Timer object
 * 		   		(e.g., a timer already cancelled)
 */
RK_ERR kTimerCancel(RK_TIMER *const kobj);
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
RK_ERR kSleep(const RK_TICK ticks);

/**
 * @brief	Sleep for an absolute period of time adjusting for
 * 			eventual jitters, suitable for periodic tasks.
 * @param	period Period in ticks
 * @return	RK_SUCCESS or specific error.
 */
RK_ERR kSleepUntil(RK_TICK const period);

/**
 * @brief Gets the current number of  ticks
 * @return Global system tick value
 */
RK_TICK kTickGet(VOID);

/**
 * @brief 	Active wait for a number of ticks
 * @param	ticks Number of ticks for busy-wait
 * @return 	RK_SUCCESS or RK_INVALID_PARAM
 */
RK_ERR kBusyWait(RK_TICK const ticks);

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
RK_ERR kMemInit(RK_MEM *const kobj, VOID *memPoolPtr, ULONG blkSize,
                const ULONG numBlocks);

/**
 * @brief Allocate memory from a block pool
 * @param kobj Pointer to the block pool
 * @return Pointer to the allocated block, or NULL on failure
 */
VOID *kMemAlloc(RK_MEM *const kobj);

/**
 * @brief Free a memory block back to the block pool
 * @param kobj Pointer to the block pool
 * @param blockPtr Pointer to the block to free
 * @return				RK_SUCCESS or specific error.
 */
RK_ERR kMemFree(RK_MEM *const kobj, VOID *blockPtr);

/******************************************************************************/
/* MISC/HELPERS                                                               */
/******************************************************************************/
/**
 * @brief Returns the kernel version.
 * @return Kernel version as an unsigned integer.
 */
unsigned int kGetVersion(void);
/**
 * @brief Generic error handler
 */
void kErrHandler(RK_FAULT fault);

/**
 * @brief Disables global interrupts
 */
__RK_INLINE
static inline VOID kDisableIRQ(VOID)
{
    __ASM volatile("CPSID I" : : : "memory");
}
/**
 * @brief Enables global interrupts
 */
__RK_INLINE
static inline VOID kEnableIRQ(VOID)
{
    __ASM volatile("CPSIE I" : : : "memory");
}

/**
 * @brief Locks scheduler (makes current task non-preemptible)
 */

static inline VOID kSchLock(VOID)
{
    if (runPtr->preempt == 0UL)
        return;

    RK_TICK_MASK

    runPtr->schLock++;

    RK_TICK_UNMASK
}
/**
 * @brief Unlocks scheduler
 */
__RK_INLINE
static inline VOID kSchUnlock(VOID)
{
    if (runPtr->schLock == 0UL)
        return;

    RK_TICK_MASK

    if (--runPtr->schLock == 0 && isPendingCtxtSwtch)
    {
        isPendingCtxtSwtch = 0;
        RK_PEND_CTXTSWTCH
    }

    RK_TICK_UNMASK
}

/**
 * @brief Condition Variable Wait. Unlocks associated mutex and suspends task.
 * 		  When waking up, task is within the mutex critical section again.
 */
#if ((RK_CONF_EVENT == ON) && (RK_CONF_MUTEX == ON))
__RK_INLINE
static inline RK_ERR kCondVarWait(RK_EVENT *const cv, RK_MUTEX *const mutex,
                                  RK_TICK timeout)
{

    kSchLock();

    kMutexUnlock(mutex);

    RK_ERR err = kEventSleep(cv, timeout);

    kSchUnlock();

    if (err < 0)
        return (err);

    return (kMutexLock(mutex, timeout));
}

/**
 * @brief Alias for kEventSignal
 *
 */
__RK_INLINE
static inline RK_ERR kCondVarSignal(RK_EVENT *const cv)
{
    return (kEventSignal(cv));
}

/**
 * @brief Alias for kEventFlush
 *
 */
__RK_INLINE
static inline RK_ERR kCondVarBroadcast(RK_EVENT *const cv)
{
    return (kEventFlush(cv));
}
#endif

/******************************************************************************/
/* CONVENIENCE MACROS                                                         */
/******************************************************************************/

/* Running Task Get */
extern RK_TCB *runPtr;

/**
 * @brief Get active task ID
 */
#define RK_RUNNING_PID (runPtr->pid)

/**
 * @brief Get active task priority
 */
#define RK_RUNNING_PRIO (runPtr->priority)

/**
 * @brief Get active task handle
 */
#define RK_RUNNING_HANDLE (runPtr)

/**
 * @brief Get active task name
 */
#define RK_RUNNING_NAME (runPtr->taskName)

/**
 * @brief Get a task ID
 * @param taskHandle Task Handle
 */
#define RK_TASK_PID(taskHandle) (taskHandle->pid)

/**
 * @brief Get a task name
 * @param taskHandle Task Handle
 */
#define RK_TASK_NAME(taskHandle) (taskHandle->taskName)

/**
 * @brief Get a task priority
 * @param taskHandle Task Handle
 */
#define RK_TASK_PRIO(taskHandle) (taskHandle->priority)

/**
 * @brief Declare data needed to create a task
 * @param handle Task Handle
 * @param taskEntry Task's entry function
 * @param stackBuf  Array's name for the task's stack
 * @param nWords	Stack Size in number of WORDS
 */
#define K_DECLARE_TASK(handle, taskEntry, stackBuf, nWords) \
    VOID taskEntry(VOID *args);                             \
    RK_STACK stackBuf[nWords] __K_ALIGN(8);                 \
    RK_TASK_HANDLE handle;

#endif /* KAPI_H */
