/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 *
 ******************************************************************************/
/*******************************************************************************
 *
 *  \brief RK0 Public API
 *
 *  \description
 *  The is the public RK0 API to be used on the highest development layer.
 *  By default it is included in app/inc/application.h
 *
 *  *** API Conventions: ***
 *
 *  Every kernel call starts with a lower-case 'k';
 *  If acting on a kernel object that is not a task, it is followed
 *  by a kernel object name and an action:
 *  e.g., kSemaPost(RK_SEMA *const kobj): posts to a semaphore.
 *  kobj is always a constant pointer to a kernel object.
 *
 *  If not receiving a kobj as the first input paramater,  it is acting on a
 *  task - that might be the caller task or a target task.
 *  If not on the caller task, the first argument will be of the type
 *  RK_TASK_HANDLE
 *  E.g.: kSignal(RK_TASK_HANDLE taskHandle); sends a direct signal to a task.
 *
 *  If acting on the caller task the first argument is not a RK_TASK_HANDLE
 *
 *  E.g.: kSleep(ticks); task suspends sleeping for the given number of ticks.
 *
 *
 ******************************************************************************/

#ifndef RK_API_H
#define RK_API_H

#include <kexecutive.h>

/******************************************************************************/
/**
 * \brief 			   Create a new task. Task prototype:
 *
 *                     VOID TaskN( VOID *)
 *
 * \param taskHandle   Handle object for the task.
 *
 * \param taskFuncPtr  Pointer to the task entry function.
 *
 * \param taskName     Task name. Keep it as much as 8 Bytes.
 *
 * \param stackAddrPtr Pointer to the task stack (the array variable).
 *
 * \param stackSize    Size of the task stack (in WORDS. 1WORD=4BYTES)
 *
 * \param argsPtr      Pointer to initial task arguments.
 *
 * \param timeSlice    Time-slice in ticks
  *
 *
 * \param priority     Task priority - valid range: 0-31.
 *
 * \param runToCompl   If this flag is 'TRUE',  the task once dispatched
 *                     although can be interrupted by tick and other hardware
 *                     interrupt lines, won't be preempted by user tasks.
 *                     runToCompl tasks are normally deferred handlers for ISRs,
 *                     if
 *
 * \return RK_SUCCESS on success, RK_ERROR on failure
 */
RK_ERR kCreateTask( RK_TASK_HANDLE *taskHandlePtr,
        const RK_TASKENTRY taskFuncPtr, CHAR *taskName, INT *const stackAddrPtr,
        const UINT stackSize, VOID *argsPtr,
#if(RK_CONF_SCH_TSLICE==ON)
        const RK_TICK timeSlice,
#endif
        const RK_PRIO priority, const BOOL runToCompl);

/**
 * \brief Initialises the kernel. To be called in main()
 *        after hardware initialisation and task creation.
 *
 */
VOID kInit( VOID);

/**
 * \brief Yields the current task.
 *
 */
VOID kYield( VOID);

/*******************************************************************************
 COUNTER SEMAPHORE
 *******************************************************************************/
#if (RK_CONF_SEMA==ON)
/**
 *\brief      		Initialise a semaphore
 *\param kobj  		Semaphore address
 *\param value 		Initial value
 *\return      \RK_SUCCESS or specific error
 */

RK_ERR kSemaInit( RK_SEMA *const kobj, const LONG value);

/**
 *\brief 			Wait on a semaphore
 *\param kobj 		Semaphore address
 *\param timeout	Maximum suspension time
 */
RK_ERR kSemaPend( RK_SEMA *const kobj, const RK_TICK timeout);

#define kSemaWait(p, t) kSemaPend(p, t) /* alias */

/**
 *\brief Signal a semaphore
 *\param kobj Semaphore address
 */
VOID kSemaPost( RK_SEMA *const kobj);

#define kSemaSignal(p) kSemaPost(p) /* alias */

/**
 *\brief Return the counter's value of a semaphore
 *\param kobj Semaphore address
 *\return      Counter's value
 */
INT kSemaQuery( RK_SEMA *const kobj);

#endif
/*******************************************************************************
 * MUTEX SEMAPHORE
 *******************************************************************************/
#if (RK_CONF_MUTEX==ON)
/**
 *\brief Init a mutex
 *\param kobj mutex address
 *\return RK_SUCCESS / RK_ERROR
 */

RK_ERR kMutexInit( RK_MUTEX *const kobj);

/**
 *\brief Lock 		a mutex
 *\param kobj 		mutex address
 *\param timeout	Maximum suspension time
 *\return RK_SUCCESS or a specific error \RK_SUCCESS or specific error
 */
RK_ERR kMutexLock( RK_MUTEX *const kobj, RK_TICK timeout);

/**
 *\brief Unlock a mutex
 *\param kobj mutex address
 *\return RK_SUCCESS or specific error
 */
RK_ERR kMutexUnlock( RK_MUTEX *const kobj);

/**
 * \brief Return the state of a mutex (locked/unlocked)
 */
RK_ERR kMutexQuery( RK_MUTEX *const kobj);

#endif

/*******************************************************************************/
/* MAILBOX (SINGLE-ITEM MAILBOX)                                               */
/*******************************************************************************/
#if (RK_CONF_MBOX == ON)

/**
 * \brief               Initialises an indirect single mailbox.
 * \param kobj          Mailbox address.
 * \param initMail		If initialising full, address of initial mail.
 *  					Otherwise NULL.
 * \return              RK_SUCCESS or specific error.
 */

RK_ERR kMboxInit( RK_MBOX *const kobj, ADDR initMail);

/**
 * \brief            Assigns a task owner for the mailbox
 * \param kobj       Mailbox address
 * \param memPtr     Task Handle
 * \return           RK_SUCCESS or specific error.
 */
RK_ERR kMboxSetOwner( RK_MBOX *const kobj, const RK_TASK_HANDLE taskHandle);

/**
 * \brief               Send to a mailbox. Task blocks when full.
 * \param kobj          Mailbox address.
 * \param sendPtr       Mail address.
 * \param timeout       Suspension time-out
 * \return              RK_SUCCESS or specific error.
 */
RK_ERR kMboxPost( RK_MBOX *const kobj, const ADDR sendPtr, RK_TICK timeout);
/**
 * \brief               Receive from a mailbox. Block if empty.
 *
 * \param kobj          Mailbox address.
 * \param recvPPtr      Address that will store the message address (pointer-to-pointer).
 * \param timeout		Suspension time-out
 * \return				RK_SUCCESS or specific error.
 */
RK_ERR kMboxPend( RK_MBOX *const kobj, ADDR *recvPPtr, RK_TICK timeout);

#if (RK_CONF_FUNC_MBOX_POSTOVW==(ON))

/**
 * \brief			Post to a mailbox even if it is full, overwriting the
 *                  the current mail.
 * \param kobj		Mailbox address.
 * \param sendPtr   Mail address.
 * \return          RK_SUCCESS or specific error
 */
RK_ERR kMboxPostOvw( RK_MBOX *const kobj, const ADDR sendPtr);

#endif

#if (RK_CONF_FUNC_MBOX_PEEK==ON)

/**
 * \brief 			   Reads the mail without extracting it.
 * \param kobj		   Mailbox address.
 * \param peekPPtr	   Pointer to receive address.
 * \return			   RK_SUCCESS or specific error.
 */
RK_ERR kMboxPeek( RK_MBOX *const kobj, ADDR *peekPPtr);

#endif

#if (RK_CONF_FUNC_MBOX_ISFULL==ON)
/**
 * \brief   Check if a mailbox is full.
 * \return  TRUE or FALSE.
 */
BOOL kMboxQuery( RK_MBOX *const kobj);

#endif

#endif /* MBOX  */
/******************************************************************************/
/* MESSAGE QUEUES                                                             */
/******************************************************************************/

/* MAIL QUEUE */

#if (RK_CONF_QUEUE == ON)

/**
 * \brief			 Initialises a mail queue.
 * \param kobj		 Mail Queue address
 * \param memPtr     Pointer to the buffer that will store mail addresses
 * \param maxItems   Maximum number of mails.
 * \return           RK_SUCCESS or specific error.
 */
RK_ERR kQueueInit( RK_QUEUE *const kobj, const ADDR memPtr,
        const ULONG maxItems);

/**
 * \brief            Assigns a task owner for the queue
 * \param kobj       Mail Queue address
 * \param memPtr     Task Handle
 * \return           RK_SUCCESS or specific error.
 */
RK_ERR kQueueSetOwner( RK_QUEUE *const kobj, const RK_TASK_HANDLE taskHandle);

/**
 * \brief               Send to a multilbox. Task blocks when full.
 * \param kobj          Mail Queue address.
 * \param sendPtr       Mail address.
 * \param timeout		Suspension time-out
 * \return              RK_SUCCESS or specific error.
 */

RK_ERR kQueuePost( RK_QUEUE *const kobj, const ADDR sendPtr,
        RK_TICK const timeout);

/**
 * \brief               Receive from a mail queue. Block if empty.
 *
 * \param kobj          Mail Queue address.
 * \param recvPPtr      Address that will store the message address
 * 					  (pointer-to-pointer).
 * \param timeout		Suspension time-out
 * \return				RK_SUCCESS or specific error.
 */
RK_ERR kQueuePend( RK_QUEUE *const kobj, ADDR *recvPPtr, RK_TICK const timeout);

#if (RK_CONF_FUNC_QUEUE_PEEK==ON)

/**
 * \brief 			   Reads the head's mail without extracting it.
 * \param kobj		   Mail Queue address.
 * \param peekPPtr	   Pointer to receive address.
 * \return			   RK_SUCCESS or specific error.
 */
RK_ERR kQueuePeek( RK_QUEUE *const kobj, ADDR *peekPPtr);

#endif

#if (RK_CONF_FUNC_QUEUE_QUERY==ON)
/**
 * \brief			Gets the current number of mails within a queue.
 * \param kobj      Mail Queue address.
 * \return			Number of mails.
 */
ULONG kQueueQuery( RK_QUEUE *const kobj);

#endif
#if (RK_CONF_FUNC_QUEUE_JAM==ON)

/**
 * \brief            Sends a message to the queue front.
 * \param kobj       (Mail) Queue address
 * \param sendPtr    Message address
 * \param timeout    Suspension time
 * \return           RK_SUCCESS or specific error
 */
RK_ERR kQueueJam( RK_QUEUE *const kobj, ADDR sendPtr, RK_TICK timeout);

#endif
#endif /* MAIL QUEUE  */

#if (RK_CONF_STREAM == ON)

/* STREAM QUEUE */

/**
 *\brief 			Initialise a Stream MessageQueue
 *\param kobj		Stream Queue address
 *\param buffer		Allocated memory. Size = messsageSiz e *maxMessages
 *\param messageSize Message size
 *\param maxMessage  Max number of messages
 *\return 			 RK_SUCCESS or specific errors
 */
RK_ERR kStreamInit( RK_STREAM *const kobj, const ADDR buf,
        const ULONG mesgSizeInWords, const ULONG nMesg);

/**
 * \brief            Assigns a task owner for the stream queue
 * \param kobj       Stream Queue address
 * \param memPtr     Task Handle
 * \return           RK_SUCCESS or specific error.
 */
RK_ERR kStreamSetOwner( RK_STREAM *const kobj, const RK_TASK_HANDLE taskHandle);

#if (RK_CONF_FUNC_STREAM_QUERY==ON)

/**
 *\brief 			Get the current number of messages within a message queue.
 *\param kobj		(Stream) Queue address
 *\param mesgCntPtr Address to store the message number
 *\return			RK_SUCCESS or a specific error.
 */

ULONG kStreamQuery( RK_STREAM *const kobj);

#endif

#if (RK_CONF_FUNC_STREAM_JAM == ON)

/**
 *\brief 			Sends a message to the queue front.
 *\param kobj		(Stream) Queue address
 *\param sendPtr	Message address
 *\param timeout	Suspension time
 *\return			RK_SUCCESS or specific error
 */
RK_ERR kStreamJam( RK_STREAM *const kobj, const ADDR sendPtr,
        const RK_TICK timeout);

#endif

/**
 *\brief 			Receive a message from the queue
 *\param kobj		(Stream) Queue address
 *\param recvPtr	Receiving address
 *\param Timeout	Suspension time
 */
RK_ERR kStreamRecv( RK_STREAM *const kobj, ADDR recvPtr, const RK_TICK timeout);

/**
 *\brief 			Send a message to a message queue
 *\param kobj		(Stream) Queue address
 *\param recvPtr	Message address
 *\param Timeout	Suspension time
 */
RK_ERR kStreamSend( RK_STREAM *const kobj, const ADDR sendPtr,
        const RK_TICK timeout);

#if (RK_CONF_FUNC_STREAM_PEEK==ON)

/**
 *\brief 			Receive the front message of a queue
 *					without changing its state
 *\param kobj		(Stream) Queue object address
 *\paramrecvPtr		Receiving pointer address
 *\return			RK_SUCCESS or error.
 */
RK_ERR kStreamPeek( RK_STREAM *const kobj, ADDR *const recvPtr);

#endif

#endif /*RK_CONF_STREAM*/

/******************************************************************************
 * TASK BINARY SEMAPHORE
 ******************************************************************************/
#if (RK_CONF_BIN_SEMA==ON)
/**
 * \brief A task pends on its own binary semaphore
 * \param timeout Suspension time until signalled
 * \return RK_SUCCESS or specific error
 */
RK_ERR kPend( const RK_TICK timeout);

/**
 * \brief Signal a task's binary semaphore
 * \param taskHandle Task handle
 * \return RK_SUCCESS or specific error
 */
RK_ERR kSignal( const RK_TASK_HANDLE taskHandle);
#endif
/******************************************************************************
 * TASK FLAGS
 ******************************************************************************/
#if (RK_CONF_TASK_FLAGS==ON)
/**
 * \brief A task pends on its own event flags
 * \param required Combination of required flags (bitstring)
 * \param gotFlagsPtr Pointer to store the flags when returning
 * \param options  RK_FLAGS_ANY/RK_FLAGS_ALL
 * \param timeout  Suspension timeout, in case required flags are not met
 * \param RK_SUCCESS, RK_ERR_FLAGS_NOT_MET or specific error
 */
RK_ERR kFlagsPend( ULONG const required, ULONG *const gotFlagsPtr,
        ULONG const options, RK_TICK const timeout);
/**
 * \brief Post a combination of flags to a task
 * \param taskHandle Receiver Task handle
 * \param mask Combination of flags
 * \param operation RK_FLAGS_OR\AND\OVW
 * \return RK_SUCCESS or specific error
 */
RK_ERR kFlagsPost( RK_TASK_HANDLE const taskHandle, ULONG const mask,
        ULONG const operation);

/**
 * \brief Reads caller task flags
 * \param taskHandle Target task
 * \param gotFlagsPtr Pointer to store the current flags
 * \return Current flags value
 */
RK_ERR kFlagsQuery( ULONG * const gotFlagsPtr);

/**
 * \brief Clears the caller task flags
 * \return RK_SUCCESS or specific error
 */
RK_ERR kFlagsClear(VOID);
#endif
/******************************************************************************
 * EVENTS
 ******************************************************************************/

#if (RK_CONF_EVENT==ON)
/**
 * \brief 			Initialise an event
 * \param kobj		Pointer to RK_EVENT object
 * \return			RK_SUCCESS/error
 */
RK_ERR kEventInit( RK_EVENT *const kobj);
/**
 * \brief 			Suspends a task waiting for a wake signal
 * \param kobj 		Pointer to a RK_EVENT object
 * \param timeout	Suspension time.
 */
RK_ERR kEventSleep( RK_EVENT *const kobj, const RK_TICK timeout);

/**
 * \brief Wakes all tasks sleeping for a specific event
 * \param kobj Pointer to a RK_EVENT object
 * \return RK_SUCCESS or specific error
 */
RK_ERR kEventWake( RK_EVENT *const kobj);

/**
 * \brief Wakes a single task sleeping for a specific event
 *        (by priority)
 * \param kobj Pointer to a RK_EVENT object
 * \return RK_SUCCESS or specific error
 */
RK_ERR kEventSignal( RK_EVENT *const kobj);

/**
 * \brief  Return the number of tasks sleeping on an event.
 * \return Number of sleeping tasks;
 */
UINT kEventQuery( RK_EVENT *const kobj);

#if (RK_CONF_EVENT_FLAGS==ON)
/**
 * \brief  Post a combination of event flags on an Event object and
 *         wakes any tasks that happens to be waiting on that
 *         combination
 *
 * \param kobj Pointer to the event object
 * \param flagMask Input bitmask operand
 * \param updatedFlagsPtr Address to return the updated flags
 * \param options RK_FLAGS_OR / RK_FLAGS_AND - bitwise operation to perform over
 * 				  the current Flags, having flagMask as the operand
 * \return RK_SUCCESS or specific error
 */
RK_ERR kEventFlagsPost( RK_EVENT *const kobj, const ULONG flagMask,
        ULONG *const updatedFlagsPtr, const ULONG options);
/**
 * \brief Pends on a combination of event flags.
 *        If they are already met, task proceeds. Otherwise, task is optionally
 *        suspended.
 * \param kobj Pointer to the event object
 * \param requiredFlags Combination of flags to be met
 * \param gotFlagsPtr Which flags have been captured as set
 * \param options RK_FLAGS_ANY_(KEEP/CONSUME),
 *                RK_FLAGS_ALL_(KEEP/CONSUME)
 *                Condition is met if ANY flag is true or if ALL flags
 *                are true. KEEP the matched flags on the kernel object
 *                or CONSUME them.
 * \param timeout  Suspension time (in ticks)
 * \return RK_SUCCESS or specific error
 */
RK_ERR kEventFlagsPend( RK_EVENT *const kobj, const ULONG requiredFlags,
        ULONG *const gotFlagsPtr, const ULONG options, const RK_TICK timeout);

/**
 * \brief 	Returns the current event flags within an event object.
 * \param kobj Address to the event object.
 * \return  ULONG value representing the current flags.
 */
ULONG kEventFlagsQuery( RK_EVENT *const kobj);

#endif

/******************************************************************************
 * CONDITION VARIABLES
 ******************************************************************************/

/**
 * \brief (Helper) Condition Variable Wait. This function must be called
 *        within a mutex critical region when in the need to wait for a
 *        a condition. It atomically put the task to sleep and unlocks
 *        the mutex.
 * \param eventPtr Pointer to event associated to a condition variable.
 * \param mutexPtr Pointer to mutex associated to a condition variable.
 * \param timeout  Suspension timeout.
 * \return RK_SUCCESS or specific error
 */

__attribute__((always_inline))
  inline RK_ERR kCondVarWait( RK_EVENT *eventPtr,
        RK_MUTEX *mutexPtr, RK_TICK timeout);

/**
 * \brief The same as kEventSignal - for readability
 * \param eventPtr Pointer to event
 * \return RK_SUCCESS or specific error
 */
__attribute__((always_inline))
  inline RK_ERR kCondVarSignal( RK_EVENT *eventPtr);
/**
 * \brief The same as kEventWake (signal broadcast) - for readability
 * \param eventPtr Pointer to event
 * \return RK_SUCCESS or specific error
 */
__attribute__((always_inline))
  inline RK_ERR kCondVarBroad( RK_EVENT *eventPtr);

#endif

#if (RK_CONF_CALLOUT_TIMER==ON)
/*******************************************************************************
 * APPLICATION TIMER AND DELAY
 ******************************************************************************/
/**
 * \brief Initialises an application timer
 * \param kobj  Timer Object address
 * \param phase Initial phase delay (does not applied on reload)
 * \param countTicks Time until it expires in ticks
 * \param funPtr Callout Function when it expires (callback)
 * \param argsPtr Generic pointer to callout arguments
 * \param reload TRUE for reloading after timer-out. FALSE for an one-shot
 * \return RK_SUCCESS/RK_ERROR
 */
RK_ERR kTimerInit( RK_TIMER *const kobj, const RK_TICK phase,
        const RK_TICK countTicks, const RK_TIMER_CALLOUT funPtr,
        const ADDR argsPtr, const BOOL reload);

#endif

/**
 * \brief Busy-wait for a specified time in ticks.
 * \param delay The busy-wait time in ticks
 */
VOID kBusyDelay( const RK_TICK delay);
/**
 * \brief Put the current task to sleep for a number of ticks.
 *        Task switches to SLEEPING state.
 * \param ticks Number of ticks to sleep
 */
VOID kSleep( const RK_TICK ticks);

#if (RK_CONF_SCH_TSLICE==OFF)

/**
 * \brief	Sleep for an absolute period of time adjusting for
 * 			eventual jitters, suitable for periodic tasks.
 */
RK_ERR kSleepUntil( RK_TICK period);

#endif
/**
 * \brief Gets the current number of  ticks
 * \return Global system tick value
 */
RK_TICK kTickGet( VOID);

/*******************************************************************************
 * BLOCK MEMORY POOL
 ******************************************************************************/

/**
 * \brief Memory Pool Control Block Initialisation
 * \param kobj Pointer to a pool control block
 * \param memPoolPtr Address of a pool (typically an array) \
 * 		  of objects to be handled
 * \param blkSize Size of each block in bytes
 * \param numBlocks Number of blocks
 * \return RK_ERROR/RK_SUCCESS
 */
RK_ERR kMemInit( RK_MEM *const kobj, const ADDR memPoolPtr, ULONG blkSize,
        const ULONG numBlocks);

/**
 * \brief Allocate memory from a block pool
 * \param kobj Pointer to the block pool
 * \return Pointer to the allocated block, or NULL on failure
 */
ADDR kMemAlloc( RK_MEM *const kobj);

/**
 * \brief Free a memory block back to the block pool
 * \param kobj Pointer to the block pool
 * \param blockPtr Pointer to the block to free
 * \return Pointer to the allocated memory. NULL on failure.
 */
RK_ERR kMemFree( RK_MEM *const kobj, const ADDR blockPtr);

/*******************************************************************************
 * MOST-RECENT MESSAGE BUFFER
 ******************************************************************************/
#if (RK_CONF_MRM==ON)
/**
 *\brief Initialise a MRM Control Block
 *\param kobj Pointer to a MRM Control Block
 *\param mrmPoolPtr  Pool of MRM buffers
 *\param mesgPoolPtr Pool of message buffers (to be attached to a MRM Buffer)
 *\param nBufs Number of MRM Buffers (that is the same as the number of messages)
 *\param dataSizeWords Size of a Messsage within a MRM (in WORDS)
 *\return K_SUCCESS or specific error.
 */
RK_ERR kMRMInit( RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
        ADDR const mesgPoolPtr, ULONG const nBufs, ULONG const dataSizeWords);

/**
 *\brief Reserves a MRM Buffer to be written
 *\param kobj Pointer to a MRM Control Block
 *\return Pointer to a MRM Buffer
 */
RK_MRM_BUF* kMRMReserve( RK_MRM *const kobj);

/**
 *\brief Copies a message into a MRM and makes it the most recent message.
 *\param kobj      Pointer to a MRM Control Block
 *\param bufPtr    Pointer to a MRM Buffer
 *\param dataPtr   Pointer to the message to be published.
 *\return K_SUCCESS or specific error
 */
RK_ERR kMRMPublish( RK_MRM *const kobj, RK_MRM_BUF *const bufPtr, ADDR dataPtr);

/**
 *\brief Receives the most recent published message within a MRM Block.
 *\param kobj      Pointer to a MRM Control Block
 *\param dataPtr   Pointer to where the message will be copied.
 *\return Pointer to the MRM from which message was retrieved
 *         (to be used afterwards on kMRMUnget()).
 */
RK_MRM_BUF* kMRMGet( RK_MRM *const kobj, ADDR getMesgPtr);

/**
 *\brief Releases a MRM Buffer which message has been consumed.
 *\param kobj      Pointer to a MRM Control Block
 *\param bufPtr    Pointer to the MRM Buffer (returned by kMRMGet())
 *\return K_SUCCESS or specific error
 */
RK_ERR kMRMUnget( RK_MRM *const kobj, RK_MRM_BUF *const bufPtr);

#endif
/*******************************************************************************
 * MISC
 ******************************************************************************/
/**
 * \brief Returns the kernel version.
 * \return Kernel version as an unsigned integer.
 */
unsigned int kGetVersion( void);

#if !defined(UNUSED)
#define UNUSED(x) (void)x
#endif

/* Running Task Get */
extern RK_TCB *runPtr;
#define RK_RUNNING_PID (runPtr->pid)
#define RK_RUNNING_PRIO (runPtr->priority)
/* Get PID from task handle */
#define RK_GET_TASK_PID(taskHandle) (taskHandle->pid)

/* Enable/Disable global interrupts */
/* Note: use this on application-level only.
 * If tweaking kernel code, look at RK_CR_*
 * system macros.
 */
__attribute__((always_inline))
  static inline VOID kDisableIRQ( VOID)
{
    __ASM volatile ("CPSID I" : : : "memory");
}

__attribute__((always_inline))
  static inline VOID kEnableIRQ( VOID)
{
    __ASM volatile ("CPSIE I" : : : "memory");
}

/*[EOF]*/

#endif /* KAPI_H */
