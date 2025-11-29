/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/**                                                                           */
/** VERSION          :   V0.8.1-dev                                           */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** You may obtain a copy of the License at                                   */
/**                                                                           */
/**     http://www.apache.org/licenses/LICENSE-2.0                            */
/******************************************************************************/
/******************************************************************************/

/* RK0 Public API. Typically included in application.h. */

#ifndef RK_API_H
#define RK_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kexecutive.h>

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
 * @param taskName     Task name. Standard size is 8 bytes.
 *                     (RK_OBJ_MAX_NAME_LEN)
 *
 * @param stackBufPtr     Pointer to the task stack (the array's name).
 *
 * @param stackSize    Size of the task stack (in WORDS. 1WORD=4BYTES)
 *                     The total number of bytes must be a multiple of 8,
 *                     that is the number of words must be even.
 *                     (See RK_DECLARE_TASK convenience macro)
 *
 * @param priority     Task priority - valid range: 0-31.
 *
 * @param preempt   Values: RK_PREEMPT / RK_NO_PREEMPT
 * 					If this parameter is 'RK_NO_PREEMPT', after dispatched it
 *					won't be preempted by user tasks until it is READY/WAITING.
 *                  Non-preemptible tasks are normally deferred handlers
 * 					for high-priority ISRs .
 *
 * @return RK_ERR_SUCCESS, or specific return value
 */
RK_ERR kCreateTask(RK_TASK_HANDLE *taskHandlePtr,
                   const RK_TASKENTRY taskFunc, VOID *argsPtr,
                   CHAR *const taskName, RK_STACK *const stackBufPtr,
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
/* TASK EVENT FLAGS (TASK NOTIFICATION)                                       */
/******************************************************************************/
/** 
 * @brief				A task pends on its own event flags
 * @param required		Combination of required flags (bitstring, non-zero)
 * @param options 		RK_FLAGS_ANY or RK_FLAGS_ALL
 * @param gotFlagsPtr	Pointer to store the flags when conditions were met,
 *                      (before being consumed).
 *                      (opt. NULL) 
 * @param timeout  		Suspension timeout, in case required flags are not met
 * @return 				RK_ERR_SUCCESS, RK_ERR_FLAGS_NOT_MET or specific return 
 *                      value
 */
RK_ERR kTaskFlagsGet(ULONG const required, UINT const options,
                  ULONG *const gotFlagsPtr,
                  RK_TICK const timeout);

 
/**
 * @brief 				Post a combination of flags to a task
 * @param taskHandle 	Receiver Task handle
 * @param mask 			Bitmask to signal (non-zero)
 * @return 				RK_ERR_SUCCESS or specific return value
 */
RK_ERR kTaskFlagsSet(RK_TASK_HANDLE const taskHandle, ULONG const mask);
 
/**
 * @brief 				Query current event register of a task
 * @param taskHandle 	Target task. NULL if target is
 *						the caller.
 * @param gotFlagsPtr 	Pointer to store the current flags
 * @return				RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kTaskFlagsQuery(RK_TASK_HANDLE const taskHandle, ULONG *const gotFlagsPtr);

/**
 * @brief Clears specified flags
 * @param taskHandle   Target task. NULL sets the target as the caller task.
 * @param flagsToClear Positions to clear.
 * @return RK_ERR_SUCCESS or specific return value
 */
RK_ERR kTaskFlagsClear(RK_TASK_HANDLE const taskHandle, ULONG const flagsToClear);

/******************************************************************************/
/* SEMAPHORES (COUNTING/BINARY)                                               */
/******************************************************************************/
#if (RK_CONF_SEMAPHORE == ON)
/**
 * @brief      			Initialise a semaphore
 * @param kobj  		Semaphore address
 * @param initValue     Initial value (0 <= initValue <= maxValue)
 * @param maxValue 		Maximum value - after reaching this value the semaphore
 *                      does not increment its counter.
 * @return  			RK_ERR_SUCCESS or specific return value.
 */

RK_ERR kSemaphoreInit(RK_SEMAPHORE *const kobj, UINT const initValue, const UINT maxValue);
#define kSemaCountInit(p, v) kSemaphoreInit(p, v, 0xFFFFFFFFU)
#define kSemaBinInit(p, v) kSemaphoreInit(p, v, 1U)

/**
 * @brief 			Wait on a semaphore
 * @param kobj 		Semaphore address
 * @param timeout	Maximum suspension time
 * @return   		RK_ERR_SUCCESS, or specific return value
 */
RK_ERR kSemaphorePend(RK_SEMAPHORE *const kobj, const RK_TICK timeout);

/**
 * @brief 			Signal a semaphore
 * @param kobj 		Semaphore address
 * @return 			RK_ERR_SUCCESS, RK_ERR_SEMA_FULL or specific return value.
 */
RK_ERR kSemaphorePost(RK_SEMAPHORE *const kobj);

/**
 * @brief 			Broadcast Signal to a semaphore
 * @param kobj 		Semaphore address
 * @return 			RK_ERR_SUCCESS, or specific return value
 */
RK_ERR kSemaphoreFlush(RK_SEMAPHORE *const kobj);

/**
 * @brief 		 	Retrieve the counter's value of a semaphore
 * @param  kobj  	Semaphore address
 * @param  countPtr Pointer to INT to store the semaphore's counter value.
 * 					A negative value means the number of blocked
 * 					tasks. A non-negative value is the semaphore's count.
 * @return          RK_ERR_SUCCESS or specific return value.
 *
 */
RK_ERR kSemaphoreQuery(RK_SEMAPHORE const *const kobj, INT *const countPtr);

#endif
/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/
#if (RK_CONF_MUTEX == ON)
/**
 * @brief 		  Init a mutex
 * @param kobj    Mutex's address
 * @param prioInh Priority inheritance option (RK_INHERIT / RK_NO_INHERIT)
 * @return 		  RK_ERR_SUCCESS or specific return value
 */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT prioInh);

/**
 * @brief 			Lock a mutex
 * @param kobj 		mutex address
 * @param timeout	Maximum suspension time
 * @return 			RK_ERR_SUCCESS, or specific return value
 */
RK_ERR kMutexLock(RK_MUTEX *const kobj, RK_TICK const timeout);

/**
 * @brief 			Unlock a mutex
 * @param kobj 		mutex address
 * @return 			RK_ERR_SUCCESS,  or specific return value
 */
RK_ERR kMutexUnlock(RK_MUTEX *const kobj);

/**
 * @brief Retrieves the state of a mutex (locked/unlocked)
 * @param statePtr Pointer to store the retrieved state
 * 				   (0 unlocked, 1 locked)
 * @return RK_ERR_SUCCESS or specific return value
 */
RK_ERR kMutexQuery(RK_MUTEX const *const kobj, UINT *const statePtr);

#endif

/******************************************************************************/
/* SLEEP QUEUE                                                                */
/******************************************************************************/
#if (RK_CONF_SLEEP_QUEUE == ON)

/**
 * @brief 			Initialise a sleep queue
 * @param kobj		Pointer to RK_SLEEP_QUEUE object
 * @return			RK_ERR_SUCCESS/error
 */
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const kobj);
/**
 * @brief 			Suspends a task waiting for a wake signal
 * @param kobj 		Pointer to a RK_SLEEP_QUEUE object
 * @param timeout	Suspension time.
 * @return				RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kSleepQueueWait(RK_SLEEP_QUEUE *const kobj, const RK_TICK timeout);


/**
 * @brief 		Broadcast signal on a sleep queue
 * @param kobj 	Pointer to a RK_SLEEP_QUEUE object
 * @param nTask		Number of taks to wake (0 if all)
 * @param uTasksPtr	Pointer to store the number
 * 					of unreleased tasks, if any (opt. NULL)
 * @return 		RK_ERR_SUCCESS or specific return value
 */

RK_ERR kSleepQueueWake(RK_SLEEP_QUEUE *const kobj, UINT nTasks, UINT *uTasksPtr);
#define kSleepQueueFlush(o) kSleepQueueWake(o, 0, NULL)

/**
 * @brief 		Wakes a single tasK  (by priority)
 * @param kobj 	Pointer to a RK_SLEEP_QUEUE object
 * @return 		RK_ERR_SUCCESS or specific return value
 */
RK_ERR kSleepQueueSignal(RK_SLEEP_QUEUE *const kobj);

/**
 * @brief 		        Wakes a specific task. Task is removed from the    
 *                      Sleeping Queue and switched to ready.
 * @param kobj 	        Pointer to a sleep queue.
 * @param taskHandle    Handle of the task to be woken.
 * @return 		RK_ERR_SUCCESS or specific return value
 */
RK_ERR kSleepQueueReady(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE taskHandle);

/**
 * @brief               Suspends a specific task on the sleep queue.
 *                      Normally a RUNNING task suspends a READY task. 
 * @param kobj          Pointer to a sleep queue.
 * @param handle        Task handle to suspend. If NULL the effect is the same  
                        as kSleepQueueWait(&slpq,  RK_WAIT_FOREVER)
 * @return RK_ERR       RK_ERR_SUCCESS or specific return value
 */
RK_ERR kSleepQueueSuspend(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE handle);


/**
 * @brief  Retrieves the number of tasks waiting on the queue.
 * @param  kobj 	 Pointer to a RK_SLEEP_QUEUE object
 * @param  nTasksPtr Pointer to where store the value
 * @return RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const *const kobj, ULONG *const nTasksPtr);



#endif

#if (RK_CONF_MESG_QUEUE == ON)

/******************************************************************************/
/* MESSAGE QUEUES / MAILBOXES                                                 */
/******************************************************************************/
/**
 * @brief 					 Initialise a Message Queue
 * @param kobj			  	 Queue address
 * @param bufPtr		 	 Allocated memory. See convenience macro
 *                           K_MESGQ_DECLARE_BUF  
 * @param mesgSizeInWords 	 Message size in words (1, 2, 4 or 8)
 *                           See convenience macro RK_MESGQ_MESG_SIZE_WORDS
 * @param nMesg  			 Max number of messages
 * @return 					 RK_ERR_SUCCESS or specific errors
 */
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                   const ULONG mesgSizeInWords, const ULONG nMesg);

/**
 * @brief            Assigns a task owner for the queue
 * @param kobj       Queue address
 * @param taskHandle Task Handle
 * @return           RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const kobj, const RK_TASK_HANDLE taskHandle);

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

/**
 * @brief            Install callback invoked after a successful send.
 * @param kobj       Queue address
 * @param cbk        Callback pointer executed within a successful send
 *                   - must be short, non-blocking.  
 *                   (NULL to remove)
 * @return           RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kMesgQueueInstallSendCbk(RK_MESG_QUEUE *const kobj,
                             VOID (*cbk)(RK_MESG_QUEUE *));

#endif

/**
 * @brief 			Receive a message from a queue
 * @param kobj		Queue address
 * @param recvPtr	Receiving address
 * @param timeout	Suspension time
 *  @return			RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const kobj, VOID *const recvPtr, const RK_TICK timeout);

/**
 * @brief 			Send a message to a message queue
 * @param kobj		Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 *  @return			RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                   const RK_TICK timeout);


/**
 * @brief           Resets a Message Queue to its initial state.
 *                  Any blocked tasks are released.
 * @param kobj      Message Queue address.
 * @return          RK_ERR_SUCCESS or specific return value. 
 */

RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);

/**
 * @brief 			Receive the front message of a queue
 *					without changing its state
 * @param kobj		Queue object address
 * @param recvPtr	Receiving pointer
 * @return			RK_ERR_SUCCESS/RK_MESG_QUEUE_EMPTY or specific return value.
 */
RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const kobj, VOID *const recvPtr);


/**
 * @brief 			Sends a message to the queue front.
 * @param kobj		(Message Queue) Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 * @return			RK_ERR_SUCCESS or specific return value
 */
RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                  const RK_TICK timeout);


/**
 * @brief 			Retrieves the current number of messages
 * 					within a message queue.
 * @param kobj		(Message Queue) Queue address
 * @param nMesgPtr  Pointer to store the retrieved number.
 * @return			RK_ERR_SUCCESS or specific return value.
 */

RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr);

/**
 * @brief           Overwrites the current message.
 *                  Only valid for one-slot queues.
 * @param kobj      Queue Address
 * @param sendPtr   Message address
 * @return          RK_ERR_SUCCESS or specific return vallue.
 */
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);

/*****  MAILBOXES API *****/
/*
  Mailboxes are single-message queues with  a message size of 1 
  word. 
 */

/**
 * @brief           Initialises a mailbox. (empty) 
 * @param kobj      Pointer to a mailbox.
 */
static inline RK_ERR kMailboxInit(RK_MAILBOX *const kobj) 
{
    return (kMesgQueueInit(&kobj->box, kobj->slot, 1, 1));
}

/**
 * @brief           Send to a mailbox. If successful mailbox will be
                    FULL.
 * 
 * @param kobj       Mailbox Adddress.
 * @param sendPtr    Address of the messaage to send. 
 * @param timeout    Suspension tiime.
 * @return           Specific code.
 */
static inline RK_ERR kMailboxPost(RK_MAILBOX *const kobj, VOID* sendPtr, RK_TICK timeout)
{
    return (kMesgQueueSend(&kobj->box, sendPtr, timeout));
}

/**
 * @brief           Receive from a mailbox. If successful mailbox will be
                    EMPTY.
 * 
 * @param kobj       Mailbox Adddress.
 * @param recvPtr    Address to store the received message.
 * @param timeout    Suspension tiime.
 * @return           Specific code.
 */
static inline RK_ERR kMailboxPend(RK_MAILBOX *const kobj, VOID* recvPtr, RK_TICK timeout)
{
    
    return (kMesgQueueRecv(&kobj->box, recvPtr, timeout));
}

/**
 * @brief           Resets a mailbox to its initial state (empty).
 *                  Any blocked tasks are released.
 * @param kobj      Mailbox address.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxReset(RK_MAILBOX *const kobj)
{
    return (kMesgQueueReset(&kobj->box));
}

/**
 * @brief           Peek the current message of a mailbox without changing its  state.
 * @param kobj      Mailbox address. 
 * @param recvPtr   Pointer to store the message.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxPeek(RK_MAILBOX *const kobj, VOID* recvPtr)
{
    return (kMesgQueuePeek(&kobj->box, recvPtr));
}

/**
 * @brief           Overwrites the current message of a mailbox.
 * 
 * @param kobj      Mailbox address.
 * @param sendPtr   Message address.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxPostOvw(RK_MAILBOX *const kobj, VOID* sendPtr)
{
    return (kMesgQueuePostOvw(&kobj->box, sendPtr));
}

/**
 * @brief           Assigns a task owner for the mailbox.
 * 
 * @param kobj      Mailbox address.
 * @param owner     Owner task handle.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxSetOwner(RK_MAILBOX *const kobj, RK_TASK_HANDLE owner)
{
    return (kMesgQueueSetOwner(&kobj->box, owner));
}

#if (RK_CONF_PORTS == ON)

/*****  PORTS API *****/
/* 
A PORT is an server endpoint (mesg queue+owner) that runs a 'procedure 
call' at the client's priority and finishes the transaction (opt., with a reply)
*/
/**
 * @brief  Initialise a Port (message queue + single server owner).
 * @param  kobj      Port object address
 * @param  buf       Pointer to the allocated buffer 
 *                   (see convenience macro K_MESGQ_DECLARE_BUF)
 * @param  msgWords  Message size in words (1, 2, 4 or 8)
 * @param  nMesg     Max number of messages
 * @param  owner     Server task handle (unique receiver)
 * @return RK_ERR_SUCCESS or specific code
 */
RK_ERR kPortInit(RK_PORT *const kobj, VOID *const buf,
                 const ULONG msgWords, const ULONG nMesg,
                 RK_TASK_HANDLE const owner);


/**
 * @brief  Send a message to a port.
 * @param  kobj    Port object address
 * @param  msg     Pointer to the message buffer (word-aligned)
 * @param  timeout Suspension time (RK_NO_WAIT, RK_WAIT_FOREVER, ticks)
 * @return RK_ERR_SUCCESS or specific error code
 */
RK_ERR kPortSend(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout);

/**
 * @brief  Receive a message from a port (only the owner can call).
 * @param  kobj    Port object address
 * @param  msg     Pointer to the destination buffer (word-aligned)
 * @param  timeout Suspension time (RK_NO_WAIT, RK_WAIT_FOREVER, ticks)
 * @return RK_ERR_SUCCESS or specific error code
 */
RK_ERR kPortRecv(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout);

/**
 * @brief  End of server transaction. Restores owner's real priority
 *         if adoption occurred while in server mode.
 * @param  kobj Port object address
 * @return RK_ERR_SUCCESS or specific error code
 */
RK_ERR kPortServerDone(RK_PORT *const kobj);

/** 
 * @brief  Send a message and wait for a UINT reply (RPC helper).
 *         See RK_PORT_MESG_2/4/8/COOKIE for message format.
 * 
 * @param  kobj          Port object address
 * @param  msgWordsPtr      Pointer to message words (at least 2 words)
 * @param  replyBox      Reply mailbox.
 * @param  replyCodePtr  Pointer to store the UINT reply code
 * @param  timeout       Suspension if blocking.
 * @return RK_ERR_SUCCESS or specific error code
 */
RK_ERR kPortSendRecv(RK_PORT *const kobj,
                     ULONG *const msgWordsPtr,
                     RK_MAILBOX *const replyBox,
                     UINT *const replyCodePtr,
                     const RK_TICK timeout);

/**
 * @brief  Server-side reply helper (RPC helper).
 * @param  kobj      Port object address
 * @param  msgWords  Pointer to the received message words 
 *                   (see RK_PORT_MESG_META for meta header)
 * @param  replyCode UINT reply code to send to client
 * @return RK_ERR_SUCCESS or specific error code
 */
RK_ERR kPortReply(RK_PORT *const kobj, ULONG const *const msgWords, const UINT replyCode);

/**
 * @brief  Server-side helper: reply and end transaction.
 * @param  kobj      Port object address
 * @param  msgWords  Pointer to the received message words 
 * @param  replyCode UINT reply code to send to client
 * @return RK_ERR_SUCCESS or specific error code
 */
RK_ERR kPortReplyDone(RK_PORT *const kobj,
                      ULONG const *const msgWords,
                      const UINT replyCode);

#endif
#endif /* RK_CONF_MESG_QUEUE */

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
 * @return				RK_ERR_SUCCESS or specific return value.
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
 * @return 			RK_ERR_SUCCESS or specific return value
 */
RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
                   VOID const *dataPtr);

/**
 * @brief 			Receives the most recent published message within a
 * MRM Block.
 * @param kobj      Pointer to a MRM Control Block
 * @param getMesgPtr   Pointer to where the message will be copied.
 * @return 			Pointer to the MRM from which message was retrieved
 *        		    (to be used afterwards on kMRMUnget()).
 */
RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr);

/**
 * @brief 			Releases a MRM Buffer which message has been consumed.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to the MRM Buffer (returned by kMRMGet())
 * @return 			RK_ERR_SUCCESS or specific return value
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
 * @param reload RK_TRUE for reloading after timer-out. RK_FALSE for an one-shot
 * @return		 RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kTimerInit(RK_TIMER *const kobj, const RK_TICK phase,
                  const RK_TICK countTicks, const RK_TIMER_CALLOUT funPtr,
                  VOID *argsPtr, const UINT reload);

/**
 * @brief 		Cancel an active timer
 * @param kobj  Timer object address
 * @return 		RK_ERR_SUCCESS or specific return value
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
 * @return 		RK_ERR_SUCCESS or specific return value
 */
RK_ERR kSleepDelay(const RK_TICK ticks);
#define kSleep(t) kSleepDelay(t)

/**
 * @brief	Intended for periodic activations.  Period defined once, at
 *          the FIRST call.
 *          The kernel keeps track of delays to preserve phase accross calls.
 * @param	period Period in ticks
 * @return	RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kSleepPeriod(RK_TICK const period);

/**
 * @brief Gets the current number of  ticks
 * @return Global system tick value
 */
RK_TICK kTickGet(VOID);


/**
 * @brief Gets the current number of ticks
 *        in milliseconds
 * @return Global system tick value [ms]
 */
RK_TICK kTickGetMs(VOID);

/**
 * @brief 	Active wait for a number of ticks
 * @param	ticks Number of ticks for busy-wait
 * @return 	RK_ERR_SUCCESS or specific return value
 */
RK_ERR kDelay(RK_TICK const ticks);
#define kBusyDelay(t) kDelay(t)


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
 * @return				RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kMemPartitionInit(RK_MEM_PARTITION *const kobj, VOID *memPoolPtr, ULONG blkSize,
                const ULONG numBlocks);

/**
 * @brief Allocate memory from a block pool
 * @param kobj Pointer to the block pool
 * @return Pointer to the allocated block, or NULL on failure
 */
VOID *kMemPartitionAlloc(RK_MEM_PARTITION *const kobj);

/**
 * @brief Free a memory block back to the block pool
 * @param kobj Pointer to the block pool
 * @param blockPtr Pointer to the block to free
 * @return				RK_ERR_SUCCESS or specific return value.
 */
RK_ERR kMemPartitionFree(RK_MEM_PARTITION *const kobj, VOID *blockPtr);

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
RK_FORCE_INLINE
static inline 
VOID kDisableIRQ(VOID)
{
    __ASM volatile("CPSID I" : : : "memory");
}
/**
 * @brief Enables global interrupts
 */
RK_FORCE_INLINE
static inline 
VOID kEnableIRQ(VOID)
{
    __ASM volatile("CPSIE I" : : : "memory");
}

/**
 * @brief Locks scheduler (makes current task non-preemptible)
 */
RK_FORCE_INLINE
static inline 
VOID kSchLock(VOID)
{
    if (runPtr->preempt == 0UL)
    {
        return;
    }
    RK_CR_AREA
    RK_CR_ENTER
    runPtr->schLock++;
    RK_CR_EXIT   

}
/**
 * @brief Unlocks scheduler
 */
RK_FORCE_INLINE
static inline 
VOID kSchUnlock(VOID)
{
    if (runPtr->schLock == 0UL)
    {
       return;
    }
    RK_CR_AREA
    RK_CR_ENTER
    if (--runPtr->schLock == 0 && isPendingCtxtSwtch)
    {
        isPendingCtxtSwtch = 0;
        RK_PEND_CTXTSWTCH
    }
    RK_CR_EXIT
}

/**
 * @brief Condition Variable Wait. Unlocks associated mutex and suspends task.
 * 		  When waking up, task is within the mutex critical section again.
 */
#if ((RK_CONF_SLEEP_QUEUE == ON) && (RK_CONF_MUTEX == ON))
RK_FORCE_INLINE
static inline 
RK_ERR kCondVarWait(RK_SLEEP_QUEUE *const cv, RK_MUTEX *const mutex,
                                  RK_TICK timeout)
{

    kSchLock();

    kMutexUnlock(mutex);

    RK_ERR err = kSleepQueueWait(cv, timeout);

    kSchUnlock();

    if (err != RK_ERR_SUCCESS)
        return (err);

    return (kMutexLock(mutex, timeout));
}

/**
 * @brief Alias for kSleepQueueSignal
 *
 */
RK_FORCE_INLINE
static inline 
RK_ERR kCondVarSignal(RK_SLEEP_QUEUE *const cv)
{
    return (kSleepQueueSignal(cv));
}

/**
 * @brief Alias for kSleepQueueFlush 
 *
 */
RK_FORCE_INLINE
static inline RK_ERR kCondVarBroadcast(RK_SLEEP_QUEUE *const cv)
{
    return (kSleepQueueWake(cv, 0, NULL));
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
 * @brief Get active task effective priority
 */
#define RK_RUNNING_PRIO (runPtr->priority)


/**
 * @brief Get active task real priority
 */
#define RK_RUNNING_REAL_PRIO (runPtr->prioReal)


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
 * @param HANDLE Task Handle
 * @param TASKENTRY Task's entry function
 * @param STACKBUF  Array's name for the task's stack
 * @param NWORDS	Stack Size in number of WORDS (even)
 */
#ifndef RK_DECLARE_TASK
#define  RK_DECLARE_TASK(HANDLE, TASKENTRY, STACKBUF, NWORDS) \
    VOID TASKENTRY(VOID *args);                              \
    RK_STACK STACKBUF[NWORDS] K_ALIGN(8);                  \
    RK_TASK_HANDLE HANDLE;
#endif

#if (RK_CONF_MESG_QUEUE == ON)


/**
 * @brief Declares the appropriate buffer to be used
 *        by a Message Queue.
 * 
 * @param MESG_TYPE Type of the message.
 * @param N_MESG   Number of messages       
 *
 */
#ifndef RK_DECLARE_MESG_QUEUE_BUF
#define RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE, N_MESG) \
    ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif
/**
 * @brief Declares the appropriate buffer to be used
 *        by a Message Queue.
 * 
 * @param MESG_TYPE  RK_PORT_MESG_2WORDS, RK_PORT_MESG_4WORDS,
 *                  RK_PORT_MESG_8WORDS, RK_PORT_MESG_COOKIE
 * @param N_MESG   Number of messages       
 *
 */

#ifndef RK_DECLARE_PORT_BUF
#define RK_DECLARE_PORT_BUF(BUFNAME, MESG_TYPE, N_MESG) \
    ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif




#define RK_PORT_MSG_META_WORDS RK_PORT_META_WORDS

#endif /* RK_CONF_MESG_QUEUE */
#ifdef __cplusplus
    }
#endif


#endif /* KAPI_H */
