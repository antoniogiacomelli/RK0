/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.19                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/


#ifndef RK_API_H
#define RK_API_H


#include <kexecutive.h>

/******************************************************************************/
/**
 * @brief 			   Create a new task. Task prototype:
 *
 *                     VOID taskFunc(VOID *args)
 *
 *                     (See RK_DECLARE_TASK convenience macro)
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
 *                        Must be aligned to an 8-byte boundary.
 *
 * @param stackSize    Size of the task stack (in WORDS. 1WORD=4BYTES)
 *                     
 *
 * @param priority     Task priority - valid range: 0-31.(0 is highest)
 *
 * @param preempt   Values: RK_PREEMPT / RK_NO_PREEMPT
 * 				        	If this parameter is 'RK_NO_PREEMPT',
 *                  after dispatched it won't be preempted by any user task
 *                  until it is READY/WAITING.
 *                  Non-preemptible tasks are typically used as dedicated
 *                  handlers for high-priority ISRs: they are signalled,
 *                  perform short unblocking work, and sleep again.
 *
 * @return RK_ERR_SUCCESS / RK_ERR_ERROR
 *
 *
 */

RK_ERR kCreateTask(RK_TASK_HANDLE *taskHandlePtr, const RK_TASKENTRY taskFunc,
                   VOID *argsPtr, CHAR *const taskName,
                   RK_STACK *const stackBufPtr, const ULONG stackSize,
                   const RK_PRIO priority, const ULONG preempt);

/**
 * @brief Declare data needed to create a task
 * @param HANDLE Task Handle
 * @param TASKENTRY Task's entry function
 * @param STACKBUF  Array's name for the task's stack
 * @param NWORDS	Stack Size in number of WORDS (even)
 */
#ifndef RK_DECLARE_TASK
#define RK_DECLARE_TASK(HANDLE, TASKENTRY, STACKBUF, NWORDS)                   \
  VOID TASKENTRY(VOID *args);                                                  \
  RK_STACK STACKBUF[NWORDS] K_ALIGN(8);                                        \
  RK_TASK_HANDLE HANDLE;
#endif

/**
 * @brief Initialises the kernel. To be called in main()
 *        after hardware initialisation.
 */
VOID kInit(VOID);

/**
 * @brief Yields the current task.
 *        Note, the highest priority task should be RUNNING.
 *        Yielding is meaningful for FIFO discipline among
 *        tasks of with the same priority.
 */
VOID kYield(VOID);

/******************************************************************************/
/* TASK'S EVENT REGISTER (FLAGS)                                              */
/******************************************************************************/
/**
 * @brief			        	A task check for events set on its 
 *                      event register.
 * @param required		  Events required a bitstring (flags)
 * 
 * @param options 		RK_EVENT_ANY - any of the required event flags 
 *                    satisfies the waiting condition if set.
 * 
 *                    RK_EVENT_ALL - all required flags need to be set
 *                    to satisfy the waiting condition.
 * 
 * @param gotFlagsPtr	  Pointer to ULONG to store the state of the flags when
 *                      condition is met, before being cleared.
 *                      (opt. NULL)
 * 
 * @param timeout  		Waiting time until condition is met.
 * 
 * @return 				Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsucessful:
 *                                   RK_ERR_FLAGS_NOT_MET
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kTaskEventGet(ULONG const required, UINT const options,
                     ULONG *const gotFlagsPtr, RK_TICK const timeout);
#define kTaskFlagsGet(a, b, c, d) kTaskEventGet(a, b, c, d)
#define kTaskEventFlagsGet(a, b, c, d) kTaskEventGet(a, b, c, d)
/**
 * @brief 				    Post a combination of event flags to a task.
 *                    This combination is OR'ed to the current flags.
 * 
 * @param taskHandle 	Receiver Task handle
 * 
 * @param mask 			  Bitmask to be OR'ed (0UL is invalid)
 * 
 * @return 				    
 *                     Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                  RK_ERR_OBJ_NULL
 *                                  RK_ERR_INVALID_PARAM
 */
RK_ERR kTaskEventSet(RK_TASK_HANDLE const taskHandle, ULONG const mask);
#define kTaskFlagsSet(a, b) kTaskEventSet(a, b)
#define kTaskEventFlagsSet(a, b) kTaskEventSet(a, b)

/**
 * @brief 				    Retrieves current event register state of a task
 * 
 * @param taskHandle 	Handle of the Target task.
 *                    If NULL the target is the caller. (error if on an ISR)
 * 
 * @param gotFlagsPtr 	Pointer to store the current events 
 * @return				Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kTaskEventQuery(RK_TASK_HANDLE const taskHandle,
                       ULONG *const gotFlagsPtr);
/* keep retro */
#define kTaskFlagsQuery(a, b) kTaskEventQuery(a, b)
#define kTaskEventFlagsQuery(a, b) kTaskEventQuery(a, b)

/**
 * @brief Clears specified flags
 * @param taskHandle   Target task. NULL sets the target as the caller task.
 * @param flagsToClear Positions to clear. 0UL is invalid.
 * @return              Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 * 
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE 
 *                                   (if taskHandle == NULL)
 *                                   RK_ERR_INVALID_PARAM
 * 
 */
RK_ERR kTaskEventClear(RK_TASK_HANDLE const taskHandle,
                       ULONG const flagsToClear);
#define kTaskFlagsClear(a, b) kTaskEventClear(a, b)
#define kTaskEventFlagsClear(a, b) kTaskEventClear(a, b)

/******************************************************************************/
/* SEMAPHORES (COUNTING/BINARY)                                               */
/******************************************************************************/
#if (RK_CONF_SEMAPHORE == ON)
/**
 * @brief      			Initialise a semaphore
 * @param kobj  		Semaphore address
 * @param initValue     Initial value (0 <= initValue <= maxValue)
 * @param maxValue 		Maximum value - after reaching this value the
 * semaphore does not increment its counter.
 * @return  			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 *                                   RK_ERR_ERROR
 */

RK_ERR kSemaphoreInit(RK_SEMAPHORE *const kobj, UINT const initValue,
                      const UINT maxValue);
#define kSemaCountInit(p, v) kSemaphoreInit(p, v, 0xFFFFFFFFU)
#define kSemaBinInit(p, v) kSemaphoreInit(p, v, 1U)

/**
 * @brief 			Wait on a semaphore
 * @param kobj 		Semaphore address
 * @param timeout	Maximum suspension time
 * @return   		Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_SEMA_BLOCKED
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSemaphorePend(RK_SEMAPHORE *const kobj, const RK_TICK timeout);

/**
 * @brief 			Signal a semaphore
 * @param kobj 		Semaphore address
 * @return 			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_SEMA_FULL
 *                              
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 */
RK_ERR kSemaphorePost(RK_SEMAPHORE *const kobj);

/**
 * @brief 			    Broadcast Signal to a semaphore.
 *                  All pending tasks switch to READY.
 *                  Count value is remains 0.
 *
 * @param kobj 		Semaphore address
 * @return 			        
 *                      Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_EMPTY_WAITING_QUEUE
 *                                   RK_ERR_NOWAIT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 */
RK_ERR kSemaphoreFlush(RK_SEMAPHORE *const kobj);

/**
 * @brief 		 	Retrieve the counter's value of a semaphore
 * @param  kobj  	Semaphore address
 * @param  countPtr Pointer to INT to store the semaphore's counter value.
 * 					A negative value means the number of
 * blocked tasks. A non-negative value is the semaphore's count.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
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
 * @return 		  Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_ERROR
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT prioInh);

/**
 * @brief 			Lock a mutex
 * @param kobj 		mutex address
 * @param timeout	Maximum suspension time
 * @return 			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MUTEX_LOCKED
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_MUTEX_REC_LOCK
 */
RK_ERR kMutexLock(RK_MUTEX *const kobj, RK_TICK const timeout);

/**
 * @brief 			Unlock a mutex
 * @param kobj 		mutex address
 * @return 			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_MUTEX_NOT_LOCKED
 *                                   RK_ERR_MUTEX_NOT_OWNER
 */
RK_ERR kMutexUnlock(RK_MUTEX *const kobj);

/**
 * @brief Retrieves the state of a mutex (locked/unlocked)
 * @param statePtr Pointer to store the retrieved state
 * 				   (0 unlocked, 1 locked)
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
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
 * @return			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const kobj);
/**
 * @brief 			Suspends a task waiting for a wake signal
 * @param kobj 		Pointer to a RK_SLEEP_QUEUE object
 * @param timeout	Suspension time.
 * @return				Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_NOWAIT
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSleepQueueWait(RK_SLEEP_QUEUE *const kobj, const RK_TICK timeout);

/**
 * @brief 		Broadcast signal on a sleep queue
 * @param kobj 	Pointer to a RK_SLEEP_QUEUE object
 * @param nTask		Number of tasks to wake (0 if all)
 * @param uTasksPtr	Pointer to store the number
 * 					of unreleased tasks, if any (opt. NULL).
 *                  If called from ISR, execution may be deferred to the
 *                  post-processing system task and uTasksPtr must be NULL.
 * @return 		Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_EMPTY_WAITING_QUEUE
 *                                   RK_ERR_NOWAIT

 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_PARAM
 */

RK_ERR kSleepQueueWake(RK_SLEEP_QUEUE *const kobj, UINT nTasks,
                       UINT *uTasksPtr);
#define kSleepQueueFlush(o) kSleepQueueWake(o, 0, NULL)

/**
 * @brief 		Wakes a single task  (by priority)
 * @param kobj 	Pointer to a RK_SLEEP_QUEUE object
 * @return 		Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_EMPTY_WAITING_QUEUE
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 */
RK_ERR kSleepQueueSignal(RK_SLEEP_QUEUE *const kobj);

/**
 * @brief 		        Wakes a specific task. Task is removed from the
 *                      Sleeping Queue and switched to READY.
 * @param kobj 	        Pointer to a sleep queue.
 * @param taskHandle    Handle of the task to be woken.
 * @return 		Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_EMPTY_WAITING_QUEUE
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 */
RK_ERR kSleepQueueReady(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE taskHandle);

/**
 * @brief               Moves a READY task to a SLEEPING
 *                      state, enqueuing it on a sleeping queue.
 *                      Tasks in other states cannot be suspended.
 * @param kobj          Pointer to a sleep queue.
 * @param handle        Handle of the task.
 * @return RK_ERR       Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kSleepQueueSuspend(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE handle);

/**
 * @brief  Retrieves the number of tasks waiting on the queue.
 * @param  kobj 	 Pointer to a RK_SLEEP_QUEUE object
 * @param  nTasksPtr Pointer to where store the value
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 */
RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const *const kobj,
                        ULONG *const nTasksPtr);

#endif

#if (RK_CONF_MESG_QUEUE == ON)

/******************************************************************************/
/* MESSAGE QUEUES                                                             */
/******************************************************************************/
/**
 * @note
 * A RK_MESG_QUEUE is the general type that holds N messages of S size. S is 1,
 *  2, 4 or 8 words.
 * 
 * A RK_MAILBOX is a specialisation with N=1, S=1. 
 * Differently from N>1 Queues, an Mbox supports last-message semantics 
 * There is no special object, but we call a Mesg Queue with N>1 and S=1 a 
 * 'Mail Queue'.
 * 
 
 * An RK_PORT is an extension is a message-queue that acts as server end-point.
 * It is for fully synchronous communication with a different set of features.
 * 
 */

/**
 * @brief 					 Initialise a Message Queue
 * @param kobj			  	 Queue address
 * @param bufPtr		 	 Allocated memory. See convenience macro
 *                           K_MESGQ_DECLARE_BUF
 * @param mesgSizeInWords 	 Message size in words (1, 2, 4 or 8)
 *                           See convenience macro RK_MESGQ_MESG_SIZE_WORDS
 * @param nMesg  			 Max number of messages
 * @return 					 Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_MESGQ_INVALID_MESG_SIZE
 *                                   RK_ERR_MESGQ_INVALID_SIZE
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                      const ULONG mesgSizeInWords, const ULONG nMesg);

#define kMailQueueInit(k, b, z, n) kMesgQueueInit(k, b, 1, n)                      
/**
 * @brief            Assigns a task owner for the queue
 * @param kobj       Queue address
 * @param taskHandle Task Handle
 * @return           Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_HAS_OWNER
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const kobj,
                          const RK_TASK_HANDLE taskHandle);

#if (RK_CONF_MESG_QUEUE_NOTIFY == ON)

/**
 * @brief            Install callback invoked after a successful send.
 * @param kobj       Queue address
 * @param cbk        Callback pointer executed within a successful send
 *                   - must be short, non-blocking.
 *                   (NULL to remove)
 * @return           Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 */
RK_ERR kMesgQueueInstallSendCbk(RK_MESG_QUEUE *const kobj,
                                VOID (*cbk)(RK_MESG_QUEUE *));

#endif

/**
 * @brief 			Receive a message from a queue
 * @param kobj		Queue address
 * @param recvPtr	Receiving address
 * @param timeout	Suspension time
 *  @return			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_NOT_OWNER
 *                                   RK_ERR_MESGQ_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const kobj, VOID *const recvPtr,
                      const RK_TICK timeout);

/**
 * @brief 			Send a message to a message queue
 * @param kobj		Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 *  @return			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                      const RK_TICK timeout);

 /**
  * @brief           Resets a Message Queue to its initial state.
  *                  Any blocked tasks are released.
  *                  If called from ISR, execution may be deferred to the
  *                  post-processing system task.
  * @param kobj      Message Queue address.
  * @return          Successful:
  *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 */

RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);

/**
 * @brief 			Receive the front message of a queue
 *					without changing its state
 * @param kobj		Queue object address
 * @param recvPtr	Receiving pointer
 * @return			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_EMPTY
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const kobj, VOID *const recvPtr);

/**
 * @brief 			Sends a message to the queue front.
 * @param kobj		(Message Queue) Queue address
 * @param sendPtr	Message address
 * @param timeout	Suspension time
 * @return			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                     const RK_TICK timeout);

/**
 * @brief 			Retrieves the current number of messages
 * 					within a message queue.
 * @param kobj		(Message Queue) Queue address
 * @param nMesgPtr  Pointer to store the retrieved number.
 * @return			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 */

RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr);

/**
 * @brief           Overwrites the current message.
 *                  Only valid for one-slot queues.
 * @param kobj      Queue Address
 * @param sendPtr   Message address
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_NOT_A_MBOX
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);

/**
 * Mailboxes are 1-slot queues, 1 word message-size (4-byte).
 */

/**
 * @brief           Initialises a mailbox. (empty)
 * @param kobj      Pointer to a mailbox.
 */
static inline RK_ERR kMailboxInit(RK_MAILBOX *const kobj) {
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
static inline RK_ERR kMailboxPost(RK_MAILBOX *const kobj, VOID *sendPtr,
                                  RK_TICK timeout) {
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
static inline RK_ERR kMailboxPend(RK_MAILBOX *const kobj, VOID *recvPtr,
                                  RK_TICK timeout) {

  return (kMesgQueueRecv(&kobj->box, recvPtr, timeout));
}

/**
 * @brief           Resets a mailbox to its initial state (empty).
 *                  Any blocked tasks are released.
 * @param kobj      Mailbox address.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxReset(RK_MAILBOX *const kobj) {
  return (kMesgQueueReset(&kobj->box));
}

/**
 * @brief           Peek the current message of a mailbox without changing its
 * state.
 * @param kobj      Mailbox address.
 * @param recvPtr   Pointer to store the message.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxPeek(RK_MAILBOX *const kobj, VOID *recvPtr) {
  return (kMesgQueuePeek(&kobj->box, recvPtr));
}

/**
 * @brief           Overwrites the current message of a mailbox.
 *
 * @param kobj      Mailbox address.
 * @param sendPtr   Message address.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxPostOvw(RK_MAILBOX *const kobj, VOID *sendPtr) {
  return (kMesgQueuePostOvw(&kobj->box, sendPtr));
}

/**
 * @brief           Assigns a task owner for the mailbox.
 *
 * @param kobj      Mailbox address.
 * @param owner     Owner task handle.
 * @return          Specific code.
 */
static inline RK_ERR kMailboxSetOwner(RK_MAILBOX *const kobj,
                                      RK_TASK_HANDLE owner) {
  return (kMesgQueueSetOwner(&kobj->box, owner));
}

/**
 * @brief Declares the appropriate buffer to be used
 *        by a Message Queue.
 * @param BUFNAME Name of the array.
 * @param MESG_TYPE Type of the message.
 * @param N_MESG   Number of messages
 *
 */
#ifndef RK_DECLARE_MESG_QUEUE_BUF
#define RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE, N_MESG)                  \
  ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif

#if (RK_CONF_PORTS == ON)
/**
*A PORT is an server endpoint (mesg queue+owner) that runs a 'procedure
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
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_MESGQ_INVALID_MESG_SIZE
 *                                   RK_ERR_MESGQ_INVALID_SIZE
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_MESGQ_HAS_OWNER
 */
RK_ERR kPortInit(RK_PORT *const kobj, VOID *const buf, const ULONG msgWords,
                 const ULONG nMesg, RK_TASK_HANDLE const owner);

/**
 * @brief  Send a message to a port.
 * @param  kobj    Port object address
 * @param  msg     Pointer to the message buffer (word-aligned)
 * @param  timeout Suspension time (RK_NO_WAIT, RK_WAIT_FOREVER, ticks)
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kPortSend(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout);

/**
 * @brief  Receive a message from a port (only the owner can call).
 * @param  kobj    Port object address
 * @param  msg     Pointer to the destination buffer (word-aligned)
 * @param  timeout Suspension time (RK_NO_WAIT, RK_WAIT_FOREVER, ticks)
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_NOT_OWNER
 *                                   RK_ERR_MESGQ_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kPortRecv(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout);

/**
 * @brief  End of server transaction. Restores owner's real priority
 *         if adoption occurred while in server mode.
 * @param  kobj Port object address
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kPortServerDone(RK_PORT *const kobj);

/**
 * @brief  Send a message and wait for a UINT reply (RPC helper).
 *         See RK_PORT_MESG_2/4/8/COOKIE for message format.
 *
 * @param  kobj         Port object address
 * @param  msgWordsPtr  Pointer to message words (at least 2 words)
 * @param  replyBoxPtr  Reply mailbox used to receive the reply code
 * @param  replyCodePtr Pointer to store the UINT reply code
 * @param  timeout      Suspension if blocking.
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                                   RK_ERR_MESGQ_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_MESGQ_INVALID_MESG_SIZE
 *         Notes:
 *                                   replyBox can be unowned or owned by
 *                                   the caller task.
 */
RK_ERR kPortSendRecv(RK_PORT *const kobj, ULONG *const msgWordsPtr,
                     RK_MAILBOX *const replyBoxPtr,
                     UINT *const replyCodePtr, const RK_TICK timeout);

/**
 * @brief  Server-side reply helper (RPC helper).
 * @param  kobj      Port object address
 * @param  msgWords  Pointer to the received message words
 *                   (see RK_PORT_MESG_META for meta header)
 * @param  replyCode UINT reply code to send to client
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_MESGQ_INVALID_MESG_SIZE
 */
RK_ERR kPortReply(RK_PORT *const kobj, ULONG const *const msgWords,
                  const UINT replyCode);

/**
 * @brief  Server-side helper: reply and end transaction.
 * @param  kobj      Port object address
 * @param  msgWords  Pointer to the received message words
 * @param  replyCode UINT reply code to send to client
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_FULL
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_MESGQ_INVALID_MESG_SIZE
 */
RK_ERR kPortReplyDone(RK_PORT *const kobj, ULONG const *const msgWords,
                      const UINT replyCode);

/**
 * @brief Declares the appropriate buffer to be used
 *        by a PORT.
 *
 * @param BUFNAME.  Buffer name
 * @param MESG_TYPE  RK_PORT_MESG_2WORDS, RK_PORT_MESG_4WORDS,
 *                  RK_PORT_MESG_8WORDS, RK_PORT_MESG_COOKIE
 * @param N_MESG   Number of messages
 *
 */

#ifndef RK_DECLARE_PORT_BUF
#define RK_DECLARE_PORT_BUF(BUFNAME, MESG_TYPE, N_MESG)                        \
  ULONG BUFNAME[RK_MESGQ_BUF_SIZE(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif

#ifndef RK_PORT_MSG_META_WORDS
#define RK_PORT_MSG_META_WORDS RK_PORT_META_WORDS
#endif

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
 * @param nBufs 		Number of MRM Buffers (that is the same as the number
 *                  of messages)
 * @param dataSizeWords Size of a Messsage within a MRM (in WORDS)
 * @return				Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
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
 * @brief 			Copies a message into a MRM and makes it the
 * most recent message.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to a MRM Buffer
 * @param dataPtr   Pointer to the message to be published.
 * @return 			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
                   VOID const *dataPtr);

/**
 * @brief 			Receives the most recent published message
 * within a MRM Block.
 * @param kobj      Pointer to a MRM Control Block
 * @param getMesgPtr   Pointer to where the message will be copied.
 * @return 			Pointer to the MRM from which message was
 * retrieved (to be used afterwards on kMRMUnget()).
 */
RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr);

/**
 * @brief 			Releases a MRM Buffer which message has been
 * consumed.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to the MRM Buffer (returned by kMRMGet())
 * @return 			Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
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
 * @param reload RK_TIMER_RELOAD for reloading after timer-out.
 *               RK_TIMER_ONESHOT for an one-shot

 * @return		 Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kTimerInit(RK_TIMER *const kobj, const RK_TICK phase,
                  const RK_TICK countTicks, const RK_TIMER_CALLOUT funPtr,
                  VOID *argsPtr, const UINT reload);

/**
 * @brief 		Cancel an active timer
 * @param kobj  Timer object address
 * @return 		Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_ERROR
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
 * @return 		Successful:
 *                                   RK_ERR_SUCCESS
 *                   Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_TASK_INVALID_ST
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kSleepDelay(const RK_TICK ticks);
#define kSleep(t) kSleepDelay(t)

/**
 * @brief	  Suspends and release a task periodically, compensating for
 *          drifts and locking phase. Lateness smaller than 1 period
 *          will shorten the time until the next activation, so
 *          phase is kept constant accross calls.
 *          Overruns higher than 1 period cannot be compensated, and
 *          are skipped. Release is scheduled to the next valid time
 *          slot.
 *          Set priorities accordingly.          
 * 
 * @details
 *          Tasks are kept aligned to a phase grid:
 *          ..., kP | (k+1)P | (k+2)P | ...
 * 
 *          If the activation supposed to happen on (k+1)P slot
 *          drifts within the (k+2)P,
 *          task will not execute until somewhere in (k+3)P.
 *
 *          
 * @param   period period in ticks
 * @return	Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_INVALID_PARAM
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSleepRelease(RK_TICK const period);
#ifndef kSleepPeriodic
#define kSleepPeriodic(t) kSleepRelease(t)
#endif

/**
 * @brief	  Suspends a task so it is released periodically.
 *          Differently from kSleepRelease, the reference is local 
 *          for each task. Compensation can either shorten the time
 *          between two activations (when overrun is less than 1 period)
 *          or return immediately. It does not skip.
 * 
 * 
 *  Example: 500 ticks periodic task
 *  @code{c}
 *
 *          VOID task(VOID* args)
 *          {
 *
 *              RK_TICK anchor = kTickGet();
 *              while(1)
 *              {
 *                  work();
 *
 *                  kSleepUntil(&anchor, 500);
 *
 *             }
 *          }
 * @endcode
 * 
 * @param	period Period in ticks
 * @param   lastTickPtr Address of the anchored time reference.
 * @return	Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_ELAPSED_PERIOD
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_PARAM
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSleepUntil(RK_TICK *lastTickPtr, RK_TICK const period);

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
 * @brief 	Active wait for a number of ticks. Task is not suspended.
 * @param	ticks Number of ticks for busy-wait
 * @return 	Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kDelay(RK_TICK const ticks);
#define kBusyDelay(t) kDelay(t)

/******************************************************************************/
/*  MEMORY PARTITION                                                          */
/******************************************************************************/
/**
 * @brief Memory Partition Control Block Initialisation
 * @param kobj Pointer to a  control block
 * @param memPoolPtr Address of a pool (typically an array) 
 * 		               of objects to be handled
 * @param blkSize Size of each block IN BYTES.
 * @param numBlocks Number of blocks
 * @return				    Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kMemPartitionInit(RK_MEM_PARTITION *const kobj, VOID *memPoolPtr,
                         ULONG blkSize, const ULONG numBlocks);

/**
 * @brief Allocate memory partition from a pool
 * @param kobj Pointer to the partition pool
 * @return Address of a memory block, or NULL on failure
 */
VOID *kMemPartitionAlloc(RK_MEM_PARTITION *const kobj);

/**
 * @brief Free a memory block (Returns it to the pool)
 * @param kobj Pointer to the partition pool
 * @param blockPtr Pointer to the block to free
 * @return				Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MEM_FREE
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
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
static inline VOID kDisableIRQ(VOID) 
{
  RK_ASM volatile("CPSID I" : : : "memory");
}
/**
 * @brief Enables global interrupts
 */
RK_FORCE_INLINE
static inline VOID kEnableIRQ(VOID) 
{
  RK_ASM volatile("CPSIE I" : : : "memory");
}

/**
 * @brief Condition Variable Wait. 
 *        Unlocks associated mutex and suspends task.
 * 		    When waking up, task is within the 
 *        mutex critical section again.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_NOWAIT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                  Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   (plus propagated mutex/sleepq errors)
 */
#if ((RK_CONF_SLEEP_QUEUE == ON) && (RK_CONF_MUTEX == ON))
RK_ERR kCondVarWait(RK_SLEEP_QUEUE *const cv,
                    RK_MUTEX *const mutex, RK_TICK timeout);

/**
 * @brief Wakes a single waiter task on a condition variable.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_EMPTY_WAITING_QUEUE
 *                  Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   (plus propagated sleepq errors)
 */
RK_ERR kCondVarSignal(RK_SLEEP_QUEUE *const cv);

/**
 * @brief Wakes all waiter tasks on a condition variable.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_EMPTY_WAITING_QUEUE
 *                  Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   (plus propagated sleepq errors)
 */
RK_ERR kCondVarBroadcast(RK_SLEEP_QUEUE *const cv);
#endif
/******************************************************************************/
/* CONVENIENCE MACROS                                                         */
/******************************************************************************/

/* Running Task Get */
extern RK_TCB *RK_gRunPtr;

/**
 * @brief Get active task ID
 */
#ifndef RK_RUNNING_PID
#define RK_RUNNING_PID (RK_gRunPtr->pid)
#endif

/**
 * @brief Get active task effective priority
 */
#ifndef RK_RUNNING_PRIO
#define RK_RUNNING_PRIO (RK_gRunPtr->priority)
#endif

/**
 * @brief Get active task nominal (real/assigned) priority
 */
#ifndef RK_RUNNING_NOM_PRIO
#define RK_RUNNING_NOM_PRIO (RK_gRunPtr->prioNominal)
#endif
/* for backwards compatibility */
#ifndef RK_RUNNING_REAL_PRIO
#define RK_RUNNING_REAL_PRIO (RK_gRunPtr->prioNominal)
#endif

/**
 * @brief Get active task handle
 */
#ifndef RK_RUNNING_HANDLE
#define RK_RUNNING_HANDLE (RK_gRunPtr)
#endif
/**
 * @brief Get active task name
 */
#ifndef RK_RUNNING_NAME
#define RK_RUNNING_NAME (RK_gRunPtr->taskName)
#endif
/**
 * @brief Get a task ID
 * @param taskHandle Task Handle
 */
#ifndef RK_TASK_PID
#define RK_TASK_PID(taskHandle) (taskHandle->pid)
#endif

/**
 * @brief Get a task name
 * @param taskHandle Task Handle
 */
#ifndef RK_TASK_NAME
#define RK_TASK_NAME(taskHandle) (taskHandle->taskName)
#endif

/**
 * @brief Get a task priority
 * @param taskHandle Task Handle
 */
#ifndef RK_TASK_PRIO
#define RK_TASK_PRIO(taskHandle) (taskHandle->priority)
#endif


#endif /* KAPI_H */
