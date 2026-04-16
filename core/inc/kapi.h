/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.18.1 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* RK0 APPLICATION PROGRAMMING INTERFACE.                                     */
/******************************************************************************/

#ifndef RK_API_H
#define RK_API_H

#include <kexecutive.h>

/******************************************************************************/
/**
 * @brief              Create a new task. Task prototype:
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
 *                        Must not be NULL.
 *
 * @param stackSize    Size of the task stack (in WORDS. 1WORD=4BYTES)
 *
 *
 * @param priority     Task priority - valid range: 0-31.(0 is highest)
 *
 * @param preempt   Scheduling mode for this task:
 *                  RK_PREEMPT or RK_NO_PREEMPT only.
 *
 *                  If RK_NO_PREEMPT is selected, once dispatched the task
 *                  will not be preempted by user tasks until it blocks,
 *                  yields, or otherwise leaves RUNNING.
 *                  Non-preemptible tasks are typically used for short,
 *                  bounded service routines.
 *
 * @return
 *                  RK_ERR_SUCCESS            Task created.
 *                  RK_ERR_OBJ_NULL           Any required pointer is NULL.
 *                  RK_ERR_INVALID_ISR_PRIMITIVE
 *                                              Called from ISR context.
 *                  RK_ERR_INVALID_PARAM      Invalid stack size or preempt mode.
 *                  RK_ERR_INVALID_PRIO       Priority is out of range.
 *                  RK_ERR_TASK_POOL_EMPTY    No free TCB slot in the task pool.
 *                  RK_ERR_ERROR              Internal failure creating the task.
 *
 *
 */

RK_ERR kTaskInit(RK_TASK_HANDLE *taskHandlePtr, const RK_TASKENTRY taskFunc,
                   VOID *argsPtr, CHAR *const taskName,
                   RK_STACK *const stackBufPtr, const ULONG stackSize,
                   const RK_PRIO priority, const RK_OPTION preempt);
#define kCreateTask kTaskInit
#if (RK_CONF_DYNAMIC_TASK == ON)
/**
 * @brief Spawn a runtime task using the shared task pool and a user-selected
 *        stack partition.
 *        The spawned task stack size is the partition block size (in words).
 *        Controlled by RK_CONF_DYNAMIC_TASK in kconfig.h.
 * @param taskAttrPtr Pointer to dynamic task attributes.
 * @param taskHandlePtr Receives task handle.
 * @return
 *                  RK_ERR_SUCCESS            Task spawned.
 *                  RK_ERR_OBJ_NULL           Required attribute pointer is NULL.
 *                  RK_ERR_INVALID_ISR_PRIMITIVE
 *                                              Called from ISR context.
 *                  RK_ERR_INVALID_PARAM      Invalid preempt mode or invalid
 *                                              partition block geometry.
 *                  RK_ERR_INVALID_PRIO       Priority is out of range.
 *                  RK_ERR_INVALID_OBJ        `stackMemPtr` is not a valid
 *                                              initialised memory partition.
 *                  RK_ERR_TASK_POOL_EMPTY    No free stack block in partition
 *                                              or no free TCB in task pool.
 *                  RK_ERR_ERROR              Internal failure creating the task.
 */
RK_ERR kTaskSpawn(RK_DYNAMIC_TASK_ATTR const *taskAttrPtr,
                  RK_TASK_HANDLE *taskHandlePtr);
#endif

/**
 * @brief Terminate a dynamic task and return its resources to the pools.
 *        If the running task terminates itself, the operation is deferred to
 *        PostProc and the caller is pended for a context switch.
 * @param taskHandlePtr Address of a task handle variable.
 *                      On success, *taskHandlePtr is set to NULL.
 * @return
 *                  RK_ERR_SUCCESS            Task terminated.
 *                  RK_ERR_OBJ_NULL           Handle pointer is NULL.
 *                  RK_ERR_TASK_POOL_NOT_INIT Task pool not initialised.
 *                  RK_ERR_INVALID_ISR_PRIMITIVE
 *                                              Called from ISR context.
 *                  RK_ERR_OBJ_NOT_INIT       Target task is not initialised.
 *                  RK_ERR_INVALID_OBJ        System task, static task, or
 *                                              invalid object.
 *                  RK_ERR_TASK_INVALID_ST    Target task cannot be terminated
 *                                              in its current state.
 *                  RK_ERR_NOWAIT             Deferred terminate queue full.
 */
RK_ERR kTaskTerminate(RK_TASK_HANDLE *taskHandlePtr);


/**
 * @brief Terminate the caller dynamic task using deferred self-termination
 *        semantics.
 * @return
 *                  RK_ERR_SUCCESS            Caller accepted termination.
 *                  RK_ERR_INVALID_ISR_PRIMITIVE
 *                                              Called from ISR context.
 *                  RK_ERR_INVALID_OBJ        Caller is invalid, system task,
 *                                              or static task.
 *                  Plus all outputs from kTaskTerminate() for the caller task.
 */
RK_ERR kTaskTerminateSelf(VOID);

/**
 * @brief Declare data needed to create a task
 * @param HANDLE Task Handle
 * @param TASKENTRY Task's entry function
 * @param STACKBUF  Array's name for the task's stack
 * @param NWORDS    Stack Size in number of WORDS (even)
 */
#ifndef RK_DECLARE_TASK
#define RK_DECLARE_TASK(HANDLE, TASKENTRY, STACKBUF, NWORDS)                   \
    VOID TASKENTRY(VOID *args);                                                \
    RK_STACK STACKBUF[NWORDS] K_ALIGN(8);                                      \
    RK_TASK_HANDLE HANDLE;
#endif

/**
 * @brief Declare a dynamic task handle (no static stack buffer).
 */
#ifndef RK_DECLARE_DYNAMIC_TASK
#define RK_DECLARE_DYNAMIC_TASK(HANDLE, TASKENTRY)                              \
    VOID TASKENTRY(VOID *args);                                                \
    RK_TASK_HANDLE HANDLE;
#endif

/**
 * @brief Declare a dynamic task stack partition storage.
 */
#ifndef RK_DECLARE_DYNAMIC_STACK_POOL
#define RK_DECLARE_DYNAMIC_STACK_POOL(PARTITION, STACKBUF, NBLOCKS, NWORDS)     \
    RK_MEM_PARTITION PARTITION;                                                 \
    RK_STACK STACKBUF[NBLOCKS][NWORDS] K_ALIGN(8);
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
/**
 * @brief  Returns the handle of the currently running task.
 * @return Task handle of the caller.
 */
RK_TASK_HANDLE kTaskGetRunningHandle(VOID);

/**
 * @brief  Returns the name of the currently running task (pointer).
 * @return Const pointer to task name string.
 */
const CHAR *kTaskGetRunningName(VOID);

/**
 * @brief  Retrieves a task's PID.
 * @param  taskHandle Target task handle.
 * @return PID of the task.
 */
RK_PID kTaskGetPID(RK_TASK_HANDLE taskHandle);


/**
 * @brief  Copies a task's name into the provided buffer.
 * @param  taskHandle Target task handle.
 * @param  buf        Destination buffer (size >= RK_OBJ_MAX_NAME_LEN).
 * @return RK_ERR_SUCCESS on copy, RK_ERR_OBJ_NULL if params are NULL.
 */
RK_ERR kTaskGetName(RK_TASK_HANDLE taskHandle, CHAR *buf);

/**
 * @brief  Returns a task's current priority.
 * @param  taskHandle Target task handle.
 * @return Priority of the task.
 */
RK_PRIO kTaskGetPrio(RK_TASK_HANDLE taskHandle);

/******************************************************************************/
/*SCHEDULER LOCK                                                              */
/******************************************************************************/
/**
 * @brief Locks the scheduler so the current task cannot be preempted by another
 *        user task. Locks are nested.
 */
VOID kSchLock(VOID);

/**
 * @brief Unlocks the scheduler. If the number of nested locks is 0, any delayed
 *        task switching happens immediately after unlocking.
 */
VOID kSchUnlock(VOID);

/******************************************************************************/
/* TASK'S EVENT REGISTER (EVENT FLAGS)                                        */
/******************************************************************************/
/**
 * @brief               A task check for events set on its
 *                        event register.
 * @param required      Events required a bitstring (flags)
 *
 * @param options       RK_EVENT_ANY - any of the required event flags
 *                      satisfies the waiting condition if set.
 *                      RK_EVENT_ALL - all required flags need to be set
 *                      to satisfy the waiting condition.
 *
 * @param gotFlagsPtr    Pointer to RK_EVENT_FLAG to store the state of the
 *                       flags when condition is met, before they are cleared.
 *                      (opt. NULL)
 *
 * @param timeout       Waiting time until condition is met.
 *
 * @return              Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsucessful:
 *                                   RK_ERR_FLAGS_NOT_MET
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kEventGet(RK_EVENT_FLAG const required, RK_OPTION const options,
                 RK_EVENT_FLAG *const gotFlagsPtr, RK_TICK timeout);
/**
 * @brief             Post a combination of event flags to a task.
 *                    This combination is OR'ed to the current flags.
 *
 * @param taskHandle    Receiver Task handle
 *
 * @param mask         Bitmask to be OR'ed (0UL is invalid)
 *
 * @return
 *                     Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                  RK_ERR_OBJ_NULL
 *                                  RK_ERR_INVALID_PARAM
 */
RK_ERR kEventSet(RK_TASK_HANDLE const taskHandle, RK_EVENT_FLAG const mask);

/**
 * @brief                   Retrieves current event register state of a task
 *
 * @param taskHandle    Handle of the Target task.
 *                    If NULL the target is the caller. (error if on an ISR)
 *
 * @param gotFlagsPtr   Pointer to store the current events
 * @return              Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kEventQuery(RK_TASK_HANDLE const taskHandle,
                   RK_EVENT_FLAG *const gotFlagsPtr);
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
RK_ERR kEventClear(RK_TASK_HANDLE const taskHandle,
                   RK_EVENT_FLAG const flagsToClear);

/******************************************************************************/
/* TASK MAIL                                                                 */
/******************************************************************************/
/**
 * @brief Deposits a message-pointer (VOID*) on the mail slot
 *         of a task (overwrites unread).
 * @param receiverTask Destination task handle (must not be NULL).
 * @param sendPtr   Message pointer to send.
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMailPost(RK_TASK_HANDLE receiverTask, VOID *const sendPtr);
#define kMailSend(r, p) kMailPost(r, p)

/**
 * @brief Receive from own task mail slot.
 * @param recvPPtr  Double pointer to store the received message-pointer.
 * @param timeout   RK_NO_WAIT, bounded ticks, or RK_WAIT_FOREVER.
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_TASKMAIL_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMailPend(VOID **const recvPPtr, RK_TICK timeout);
#define kMailRecv(pp, t) kMailPend(pp, t)

/**
 * @brief Non-destructive read of the task mail slot.
 * @param peekPPtr  Double pointer to store the peeked message-pointer.
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                 RK_ERR_TASKMAIL_EMPTY
 *
 *            Errors:                   RK_ERR_OBJ_NULL
 */
RK_ERR kMailPeek(VOID **const peekPPtr);

/**
 * @brief Check the the status of a task mail slot (FULL/EMPTY)
 * @param task taskHandle to check for status (NULL for caller).
 * @return RK_ERR_TASKMAIL_FULL or RK_ERR_TASKMAIL_EMPTY.
 */
RK_ERR kMailQuery(RK_TASK_HANDLE taskHandle);

/******************************************************************************/
/* SEMAPHORES (COUNTING/BINARY)                                               */
/******************************************************************************/
#if (RK_CONF_SEMAPHORE == ON)
/**
 * @brief               Initialise a semaphore
 * @param kobj          Semaphore address
 * @param initValue     Initial value (0 <= initValue <= maxValue)
 * @param maxValue      Maximum value - after reaching this value the
 * semaphore does not increment its counter.
 * @return              Successful:
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
 * @brief           Wait on a semaphore
 * @param kobj      Semaphore address
 * @param timeout   Maximum suspension time
 * @return          Successful:
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
 * @brief           Signal a semaphore
 * @param kobj      Semaphore address
 * @return          Successful:
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
 * @brief           Retrieve the counter's value of a semaphore
 * @param  kobj     Semaphore address
 * @param  countPtr Pointer to INT to store the semaphore's counter value.
 *                  A negative value means the number of
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
 * @brief         Init a mutex
 * @param kobj    Mutex's address
 * @param prioInh Priority inheritance option (RK_INHERIT / RK_NO_INHERIT)
 * @return        Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_ERROR
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT prioInh);

/**
 * @brief           Lock a mutex
 * @param kobj      mutex address
 * @param timeout   Maximum suspension time
 * @return          Successful:
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
 * @brief           Unlock a mutex
 * @param kobj      mutex address
 * @return          Successful:
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
 *                 (0 unlocked, 1 locked)
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_FULL
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
 * @brief           Initialise a sleep queue
 * @param kobj      Pointer to RK_SLEEP_QUEUE object
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const kobj);
/**
 * @brief           Suspends a task waiting for a wake signal
 * @param kobj      Pointer to a RK_SLEEP_QUEUE object
 * @param timeout   Suspension time.
 * @return              Successful:
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
 * @brief       Broadcast signal on a sleep queue
 * @param kobj  Pointer to a RK_SLEEP_QUEUE object
 * @param nTask     Number of tasks to wake (0 if all)
 * @param uTasksPtr Pointer to store the number
 *                  of unreleased tasks, if any (opt. NULL).
 *                  If called from ISR, execution may be deferred to the
 *                  post-processing system task and uTasksPtr must be NULL.
 * @return      Successful:
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
 * @brief       Wakes a single task  (by priority)
 * @param kobj  Pointer to a RK_SLEEP_QUEUE object
 * @return      Successful:
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
 * @brief               Wakes a specific task. Task is removed from the
 *                      Sleeping Queue and switched to READY.
 * @param kobj          Pointer to a sleep queue.
 * @param taskHandle    Handle of the task to be woken.
 * @return      Successful:
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
 * @param  kobj      Pointer to a RK_SLEEP_QUEUE object
 * @param  nTasksPtr Pointer to where store the value
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_FULL
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
/* MESSAGE QUEUE                                                              */
/******************************************************************************/
/**
 * @brief                         Initialise a Message Queue
 * @param kobj                Queue address
 * @param bufPtr                Allocated memory. See convenience macro
 *                        K_MESGQ_DECLARE_BUF to declare a buffer
 *                        given providing a the item data type and the
 *                        desired number of items.
 *
 * @param mesgWords         Message size in words (1, 2, 4 or 8)
 *                        See convenience macro RK_MESGQ_MESG_SIZE_WORDS
 *
 *
 * @param nMesg                 Max number of messages
 * @return                        Successful:
 *                                   RK_ERR_SUCCESS
 *                        Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_MSG_SIZE
 *                                   RK_ERR_INVALID_DEPTH
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj, VOID *const bufPtr,
                      const ULONG mesgWords, const ULONG nMesg);

#if (RK_CONF_MESG_QUEUE_SEND_CALLBACK == ON)

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
 * @brief           Receive a message from a queue
 * @param kobj      Queue address
 * @param recvPtr   Receiving address
 * @param timeout   Suspension time
 *  @return         Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
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
 * @brief           Send a message to a message queue
 * @param kobj      Queue address
 * @param sendPtr   Message address
 * @param timeout   Suspension time
 *  @return         Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_FULL
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
 * @brief           Assigns a single owner task to a message queue.
 *                  When an owner is set, only that task can receive.
 * @param kobj      Queue address.
 * @param ownerTask Task handle that owns the queue.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const kobj,
                          RK_TASK_HANDLE const ownerTask);

/**
 * @brief           Send a message to a queue owned by a task.
 *                  Convenience macro that checks the owner task for an
 *                  attached queue and forwards to kMesgQueueSend.
 * @param ownerTask Owner task handle (queue is attached to this task).
 * @param sendPtr   Message address.
 * @param timeout   Suspension time.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_BUFFER_FULL
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                  Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
#ifndef kMesgSend
#define kMesgSend(OWNER_TASK, SEND_PTR, TIMEOUT)                               \
    (((OWNER_TASK) == NULL)                                                    \
         ? RK_ERR_OBJ_NULL                                                     \
         : (((OWNER_TASK)->queuePortPtr == NULL)                         \
                ? RK_ERR_INVALID_OBJ                                           \
                : kMesgQueueSend((OWNER_TASK)->queuePortPtr, (SEND_PTR), \
                                 (TIMEOUT))))
#endif

/**
 * @brief           Send a message to the front of a queue owned by a task.
 *                  Convenience macro that checks the owner task for an
 *                  attached queue and forwards to kMesgQueueJam.
 * @param ownerTask Owner task handle (queue is attached to this task).
 * @param sendPtr   Message address.
 * @param timeout   Suspension time.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_BUFFER_FULL
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                  Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
#ifndef kMesgJam
#define kMesgJam(OWNER_TASK, SEND_PTR, TIMEOUT)                                \
    (((OWNER_TASK) == NULL)                                                    \
         ? RK_ERR_OBJ_NULL                                                     \
         : (((OWNER_TASK)->queuePortPtr == NULL)                         \
                ? RK_ERR_INVALID_OBJ                                           \
                : kMesgQueueJam((OWNER_TASK)->queuePortPtr, (SEND_PTR),  \
                                (TIMEOUT))))
#endif

/**
 * @brief           Overwrite the current message of an owned queue.
 *                  Convenience macro that checks the owner task for an
 *                  attached queue and forwards to kMesgQueuePostOvw.
 * @param ownerTask Owner task handle (queue is attached to this task).
 * @param sendPtr   Message address.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_MESG_DEPTH
 *                  Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
#ifndef kMesgPostOvw
#define kMesgPostOvw(OWNER_TASK, SEND_PTR)                                     \
    (((OWNER_TASK) == NULL)                                                    \
         ? RK_ERR_OBJ_NULL                                                     \
         : (((OWNER_TASK)->queuePortPtr == NULL)                         \
                ? RK_ERR_INVALID_OBJ                                           \
                : kMesgQueuePostOvw((OWNER_TASK)->queuePortPtr,          \
                                    (SEND_PTR))))
#endif

/**
 * @brief           Receive a message from the owned queue of the caller task.
 *                  Convenience macro that checks the running task for an
 *                  attached queue and forwards to kMesgQueueRecv.
 * @param recvPtr   Receiving address.
 * @param timeout   Suspension time.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                  Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_NOT_OWNER
 */
#ifndef kMesgRecv
#define kMesgRecv(RECV_PTR, TIMEOUT)                                           \
    ((RK_gRunPtr->queuePortPtr == NULL)                                  \
         ? RK_ERR_INVALID_OBJ                                                  \
         : kMesgQueueRecv(RK_gRunPtr->queuePortPtr, (RECV_PTR),          \
                          (TIMEOUT)))
#endif

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
 * @brief           Receive the front message of a queue
 *                  without changing its state
 * @param kobj      Queue object address
 * @param recvPtr   Receiving pointer
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const kobj, VOID *const recvPtr);

/**
 * @brief           Sends a message to the queue front.
 * @param kobj      (Message Queue) Queue address
 * @param sendPtr   Message address
 * @param timeout   Suspension time
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_FULL
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
 * @brief           Retrieves the current number of messages
 *                  within a message queue.
 * @param kobj      (Message Queue) Queue address
 * @param nMesgPtr  Pointer to store the retrieved number.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 */

RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr);

/**
 * @brief           Overwrites the current message.
 *                  Only valid for single-message queues.
 * @param kobj      Queue Address
 * @param sendPtr   Message address
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESG_DEPTH
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);

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

#endif /* RK_CONF_MESG_QUEUE */

/******************************************************************************/
/* CHANNEL (PROCEDURE CALL)                                                  */
/******************************************************************************/
#if (RK_CONF_CHANNEL == ON)
/**
 * @brief  Initialise a Channel (procedure-call request queue).
 *         The channel carries request pointers only.
 * @param  kobj       Channel object address.
 * @param  buf        Pointer to the allocated buffer
 *                    (see convenience macro RK_DECLARE_CHANNEL_BUF).
 * @param  depth      Max number of outstanding requests.
 * @param  serverTask Server task handle (unique receiver).
 * @param  reqPartPtr Request-envelope partition (blkSize >=
 * sizeof(RK_REQ_BUF)).
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_PARAM
 *                                   RK_ERR_INVALID_DEPTH
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kChannelInit(RK_CHANNEL *const kobj, VOID *const buf, const ULONG depth,
                    RK_TASK_HANDLE const serverTask,
                    RK_MEM_PARTITION *const reqPartPtr);

/**
 * @brief Client-side send+wait flow for server tasks.
 *        The descriptor is queued to the channel and caller blocks on
 *        channel-owned waiter queue until server calls kChannelDone().
 * @param serverTask Server task handle (channel is attached to this task).
 * @param reqBufPtr Request descriptor (from reqPartPtr allocation).
 * @param timeout   RK_WAIT_FOREVER or bounded ticks (RK_NO_WAIT invalid).
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_BUFFER_FULL
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_INVALID_MSG_SIZE
 */
RK_ERR kChannelCall(RK_TASK_HANDLE const serverTask,
                    RK_REQ_BUF *const reqBufPtr, const RK_TICK timeout);

/**
 * @brief Server-side accept helper for kChannelCall().
 *        Receives one request descriptor from the channel.
 *        On successful accept, server adopts caller effective priority
 *        until kChannelDone() is called.
 * @param kobj      Channel object address.
 * @param reqBufPPtr Pointer to store accepted request descriptor.
 * @param timeout    RK_NO_WAIT, RK_WAIT_FOREVER, or bounded ticks.
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_NOT_OWNER
 *                                   RK_ERR_INVALID_MSG_SIZE
 */
RK_ERR kChannelAccept(RK_CHANNEL *const kobj, RK_REQ_BUF **const reqBufPPtr,
                      const RK_TICK timeout);

/**
 * @brief Server-side completion helper for kChannelCall().
 *        Dequeues and readies reqBufPtr->sender from the channel requester
 * queue. Restores server nominal priority. It also returns the request
 * descriptor to the pool.
 * @param reqBufPtr Request descriptor previously accepted.
 * @return Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_MEM_FREE
 */
RK_ERR kChannelDone(RK_REQ_BUF *const reqBufPtr);

/**
 * @brief Declares the appropriate buffer to be used
 *        by a CHANNEL.
 * @param BUFNAME Buffer name.
 * @param DEPTH   Number of request pointers.
 */
#ifndef RK_DECLARE_CHANNEL_BUF
#define RK_DECLARE_CHANNEL_BUF(BUFNAME, DEPTH)                                 \
    ULONG BUFNAME[(UINT)(DEPTH)] K_ALIGN(4);
#endif
#endif /* RK_CONF_CHANNEL */

/******************************************************************************/
/* MOST-RECENT MESSAGE PROTOCOL                                               */
/******************************************************************************/
#if (RK_CONF_MRM == ON)
/**
 * @brief               Initialise a MRM Control Block
 * @param kobj          Pointer to a MRM Control Block
 * @param mrmPoolPtr    Pool of MRM buffers
 * @param mesgPoolPtr   Pool of message buffers (to be attached to a MRM Buffer)
 * @param nBufs         Number of MRM Buffers (that is the same as the number
 *                  of messages)
 * @param dataSizeWords Size of a Messsage within a MRM (in WORDS)
 * @return              Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kMRMInit(RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
                VOID *mesgPoolPtr, ULONG const nBufs,
                ULONG const dataSizeWords);

/**
 * @brief       Reserves a MRM Buffer to be written
 * @param kobj  Pointer to a MRM Control Block
 * @return      Pointer to a MRM Buffer
 */
RK_MRM_BUF *kMRMReserve(RK_MRM *const kobj);

/**
 * @brief           Copies a message into a MRM and makes it the
 * most recent message.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to a MRM Buffer
 * @param dataPtr   Pointer to the message to be published.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
                   VOID const *dataPtr);

/**
 * @brief           Receives the most recent published message
 * within a MRM Block.
 * @param kobj      Pointer to a MRM Control Block
 * @param getMesgPtr   Pointer to where the message will be copied.
 * @return          Pointer to the MRM from which message was
 * retrieved (to be used afterwards on kMRMUnget()).
 */
RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr);

/**
 * @brief           Releases a MRM Buffer which message has been
 * consumed.
 * @param kobj      Pointer to a MRM Control Block
 * @param bufPtr    Pointer to the MRM Buffer (returned by kMRMGet())
 * @return          Successful:
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

 * @return       Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kTimerInit(RK_TIMER *const kobj, const RK_TICK phase,
                  const RK_TICK countTicks, const RK_TIMER_CALLOUT funPtr,
                  VOID *argsPtr, const RK_OPTION reload);

/**
 * @brief       Cancel an active timer
 * @param kobj  Timer object address
 * @return      Successful:
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
 * @brief       Put the current task to sleep for a number of ticks.
 *              Task switches to SLEEPING state.
 * @param ticks Number of ticks to sleep
 * @return      Successful:
 *                                   RK_ERR_SUCCESS
 *                   Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   RK_ERR_TASK_INVALID_ST
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kSleepDelay(const RK_TICK ticks);
#define kSleep(t) kSleepDelay(t)

/**
 * @brief     Suspends and release a task periodically, compensating for
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
 * @return  Successful:
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
 * @brief     Suspends a task so it is released periodically.
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
 * @param   period Period in ticks
 * @param   lastTickPtr Address of the anchored time reference.
 * @return  Successful:
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
 * @brief   Active wait for a number of ticks. Task is not suspended.
 * @param   ticks Number of ticks for busy-wait
 * @return  Successful:
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
 *                     of objects to be handled
 * @param blkSize Size of each block IN BYTES.
 * @param numBlocks Number of blocks
 * @return                  Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kMemPartitionInit(RK_MEM_PARTITION *const kobj, VOID *memPoolPtr,
                         ULONG blkSize, const ULONG numBlocks);
#ifndef RK_DECLARE_MEM_POOL
#define RK_DECLARE_MEM_POOL(TYPE, BUFNAME, N_BLOCKS)                           \
    TYPE BUFNAME[N_BLOCKS] K_ALIGN(4);
#endif
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
 * @return              Successful:
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
 *          When waking up, task is within the
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
RK_ERR kCondVarWait(RK_SLEEP_QUEUE *const cv, RK_MUTEX *const mutex,
                    RK_TICK timeout);

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
 * @brief Convert ticks to milliseconds
 * @param ticks Number of ticks
 * @return Equivalent number of milliseconds
 */
#ifndef RK_TICKS_TO_MS
#define RK_TICKS_TO_MS(ticks) ((ticks) * RK_TICK_INTERVAL_MS)
#endif

/**
 * @brief Convert milliseconds to ticks
 * @param ms Number of milliseconds
 * @return Equivalent number of ticks
 */
#ifndef RK_MS_TO_TICKS
#define RK_MS_TO_TICKS(ms) ((ms) / RK_TICK_INTERVAL_MS)
#endif


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
/**
 * @brief Get active task handle
 */
#ifndef RK_RUNNING_HANDLE
#define RK_RUNNING_HANDLE (kTaskGetRunningHandle())
#endif
/**
 * @brief Get active task name
 */
#ifndef RK_RUNNING_NAME
#define RK_RUNNING_NAME (kTaskGetRunningName())
#endif
/**
 * @brief Get a task ID
 * @param taskHandle Task Handle
 */
#ifndef RK_TASK_PID
#define RK_TASK_PID(taskHandle) (kTaskGetPID(taskHandle))
#endif

/**
 * @brief Get a task priority
 * @param taskHandle Task Handle
 */
#ifndef RK_TASK_PRIO
#define RK_TASK_PRIO(taskHandle) (kTaskGetPrio(taskHandle))
#endif

/**
 * @brief Get the address of the task name string (CHAR*)
 * @param taskHandle Task Handle
 */
#ifndef RK_TASKNAME_PTR
#define RK_TASKNAME_PTR(taskHandle) (taskHandle->taskName)
#endif

#endif /* KAPI_H */
