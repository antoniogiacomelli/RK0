/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.71.0 */
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
 * @brief              Initialise a new task. Task prototype:
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
 * @param stackSize    Size of the task stack, in words. Must be at least
 *                     RK_MIN_STACKSIZE and even, so the initial stack frame
 *                     remains 8-byte aligned.
 *
 *
 * @param priority     Task priority - valid range: 0-31.(0 is highest).
 *                     Priority 0 is legal for application tasks, but it is
 *                     also used by PostProcSysTask. Equal-priority readiness
 *                     does not preempt the running task, so long-running
 *                     priority-0 application code can delay callouts and
 *                     deferred wake processing.
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
#define kCreateTask kTaskInit /* alias*/
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

#if (RK_CONF_DYNAMIC_TASK == ON)
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
#endif

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
/**
 * @brief Initialise kernel-owned dynamic object partitions.
 *        Applications normally use RK_INIT_OBJ_PARTITIONS before
 *        kSemaphoreCreate(), kMutexCreate(), kSleepQueueCreate(),
 *        kMesgQueueCreate(), kTimerCreate(), or kMRMCreate().
 * @return RK_ERR_SUCCESS on success, or a propagated partition init error.
 */
RK_ERR kObjPartitionsInit(VOID);
#ifndef RK_INIT_OBJ_PARTITIONS
#define RK_INIT_OBJ_PARTITIONS do { kObjPartitionsInit(); } while(0);
#endif
#else
#ifndef RK_INIT_OBJ_PARTITIONS
#define RK_INIT_OBJ_PARTITIONS do { } while(0);
#endif
#endif

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
/*PREEMPT DISABLE/ENABLE*/
/******************************************************************************/
/**
 * @brief Locks the scheduler so the current task cannot be preempted by another
 *        user task. Locks are nested.
 */
extern VOID kSchLock(VOID);
#ifndef kPreemptDisable
#define kPreemptDisable kSchLock
#endif
/**
 * @brief Unlocks the scheduler. If the number of nested locks is 0, any delayed
 *        task switching happens immediately after unlocking.
 */
extern VOID kSchUnlock(VOID);
#ifndef kPreemptEnable
#define kPreemptEnable kSchUnlock
#endif

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
 * @param gotFlagsPtr    Pointer to RK_TASK_EVENT to store the state of the
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
RK_ERR kEventGet(RK_TASK_EVENT const required, RK_OPTION const options,
                 RK_TASK_EVENT *const gotFlagsPtr, RK_TICK timeout);
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
RK_ERR kEventSet(RK_TASK_HANDLE const taskHandle, RK_TASK_EVENT const mask);

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
                   RK_TASK_EVENT *const gotFlagsPtr);
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
                   RK_TASK_EVENT const flagsToClear);

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
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kSemaphoreCreate(RK_SEMAPHORE_HANDLE *const semaHandlePtr,
                        UINT const initValue,
                        UINT const maxValue);
RK_ERR kSemaphoreDestroy(RK_SEMAPHORE_HANDLE *const semaHandlePtr);
#endif

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
 * @brief             Init a mutex
 * @param kobj        Mutex's address
 * @param protocol    Mutex protocol (RK_PRIO_NONE / RK_PRIO_INHERITANCE).
 * @return            Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_ERROR
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT protocol);

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kMutexCreate(RK_MUTEX_HANDLE *const mutexHandlePtr, UINT protocol);
RK_ERR kMutexDestroy(RK_MUTEX_HANDLE *const mutexHandlePtr);
#endif

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
 * @brief           Initialise a Sleep Queue
 * @param kobj      Pointer to RK_SLEEP_QUEUE object
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 */
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const kobj);
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kSleepQueueCreate(RK_SLEEP_QUEUE_HANDLE *const sleepqHandlePtr);
RK_ERR kSleepQueueDestroy(RK_SLEEP_QUEUE_HANDLE *const sleepqHandlePtr);
#endif
/**
 * @brief           Puts the running task to sleep on a Sleep Queue.
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
RK_ERR kSleepQueueSleep(RK_SLEEP_QUEUE *const kobj, const RK_TICK timeout);

/**
 * @brief       Wakes tasks sleeping on a Sleep Queue.
 * @param kobj  Pointer to a RK_SLEEP_QUEUE object
 * @param nTasks    Number of tasks to wake (0 if all)
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
#ifndef kSleepQueueFlush
#define kSleepQueueFlush(o) kSleepQueueWake(o, 0, NULL)
#endif

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
 *                      Sleep Queue and switched to READY.
 * @param kobj          Pointer to a Sleep Queue.
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
 * @brief               Moves a READY task to a Sleep Queue.
 *                      Tasks in other states, including the running task,
 *                      cannot be blocked with this API.
 * @param kobj          Pointer to a Sleep Queue.
 * @param handle        Handle of the task.
 * @return RK_ERR       Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kSleepQueueUnready(RK_SLEEP_QUEUE *const kobj,
                          RK_TASK_HANDLE handle);

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
#define kMboxInit(kobj, bufPtr, MESG_WORDS)                                    \
    kMesgQueueInit((kobj), (bufPtr), (MESG_WORDS), 1UL)
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kMesgQueueCreate(RK_MESG_QUEUE_HANDLE *const queueHandlePtr,
                        VOID *const bufPtr,
                        ULONG const mesgWords, ULONG const nMesg);
RK_ERR kMesgQueueDestroy(RK_MESG_QUEUE_HANDLE *const queueHandlePtr);
#define kMboxCreate kMesgQueueCreate
#define kMboxDestroy kMesgQueueDestroy
#endif
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
 * @brief           Retrieves message queue counters.
 * @param kobj      (Message Queue) Queue address
 * @param nMesgPtr  Pointer to store the retrieved number (opt NULL).
 * @param nWaitRPtr Pointer to store the number of waiting receivers (opt NULL).
 * @param nWaitSPtr Pointer to store the number of waiting senders (opt NULL).
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_PARAM
 */

RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr,
                       UINT *const nWaitRPtr, UINT *const nWaitSPtr);
#ifndef kMesgQueueQueryMessageCount
#define kMesgQueueQueryMessageCount(KOBJ, N_MESG_PTR)                          \
    kMesgQueueQuery((KOBJ), (N_MESG_PTR), (NULL), (NULL))
#endif
#ifndef kMesgQueueQueryWaitingReceivers
#define kMesgQueueQueryWaitingReceivers(KOBJ, N_WAIT_R_PTR)                    \
    kMesgQueueQuery((KOBJ), (NULL), (N_WAIT_R_PTR), (NULL))
#endif
#ifndef kMesgQueueQueryWaitingSenders
#define kMesgQueueQueryWaitingSenders(KOBJ, N_WAIT_S_PTR)                      \
    kMesgQueueQuery((KOBJ), (NULL), (NULL), (N_WAIT_S_PTR))
#endif
/**
 * @brief           Overwrites the current message.
 *                  Only valid for single-message queues.
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
 * @brief           Broadcast a message to currently blocked broadcast
 *                  receivers. Only valid for single-message queues.
 *                  Fails without depositing the message if no broadcast
 *                  receiver is blocked. If more than one receiver is targeted,
 *                  receiver wakeup is deferred to PostProcSysTask.
 * @param kobj      Queue Address
 * @param sendPtr   Message address
 * @param nRecvPtr  Optional pointer receiving the number of tasks targeted.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_MESGQ_NOT_A_MBOX
 *                                   RK_ERR_BUFFER_FULL
 *                                   RK_ERR_BUFFER_EMPTY
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 */
RK_ERR kMesgQueueBroadcast(RK_MESG_QUEUE *const kobj, VOID *const sendPtr,
                           UINT *const nRecvPtr);
#define kMboxBroadcast kMesgQueueBroadcast /* alias */

/**
 * @brief           Receive a broadcast message from a single-message queue.
 * @param kobj      Queue address
 * @param recvPtr   Receiving address
 * @param timeout   Suspension time
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                      Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgQueueBroadcastRecv(RK_MESG_QUEUE *const kobj,
                               VOID *const recvPtr,
                               const RK_TICK timeout);
#define kMboxBroadcastRecv kMesgQueueBroadcastRecv /* alias */
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

#ifndef kMboxQueryMessageCount
#define kMboxQueryMessageCount(KOBJ, N_MESG_PTR)                               \
    kMesgQueueQueryMessageCount((KOBJ), (N_MESG_PTR))
#endif
#ifndef kMboxQueryWaitingReceivers
#define kMboxQueryWaitingReceivers(KOBJ, N_WAIT_R_PTR)                         \
    kMesgQueueQueryWaitingReceivers((KOBJ), (N_WAIT_R_PTR))
#endif
#ifndef kMboxQueryWaitingSenders
#define kMboxQueryWaitingSenders(KOBJ, N_WAIT_S_PTR)                           \
    kMesgQueueQueryWaitingSenders((KOBJ), (N_WAIT_S_PTR))
#endif
#ifndef kMboxQuery
#define kMboxQuery kMesgQueueQuery /* alias */
#endif
#define kMboxPost kMesgQueueSend /* alias */
#define kMboxPend kMesgQueueRecv /* alias */
#define kMboxReset kMesgQueueReset /* alias */

#ifndef RK_DECLARE_MESG_QUEUE
#define RK_DECLARE_MESG_QUEUE(QUEUE_NAME, BUFNAME, MESG_TYPE, N_MESG)\
    RK_DECLARE_MESG_QUEUE_BUF(BUFNAME, MESG_TYPE,N_MESG)\
    RK_MESG_QUEUE QUEUE_NAME;
#endif

#ifndef RK_DECLARE_MBOX
#define RK_DECLARE_MBOX(MBOX_NAME, BUFNAME, MESG_TYPE)\
    RK_DECLARE_MBOX_BUF(BUFNAME, MESG_TYPE)\
    RK_MBOX MBOX_NAME;
#endif


#endif /* RK_CONF_MESG_QUEUE */

/******************************************************************************/
/* ASYNCHRONOUS DIRECT MESSAGE                                                */
/******************************************************************************/
#if ((RK_CONF_ASYNCH_MESG == ON) && (RK_CONF_MESG_QUEUE == ON))
/**
 * Asynchronous Direct Message is  task-to-task message passing.
 * Messages are fixed-size blocks allocated from an application-provided memory
 * partition. kMesgAlloc() can wait for pool availability. kMesgSend()
 * transfers ownership of an allocated message to a task endpoint without
 * blocking for receiver queue space. The receiver obtains the message pointer
 * with kMesgWait() and returns it to the originating pool with kMesgFree().
 */

/**
 * @brief Initialize a task-backed async direct-message endpoint.
 * @param taskHandle Task that will receive messages with kMesgWait().
 * @return           Successful:
 *                                   RK_ERR_SUCCESS
 *                   Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_HAS_OWNER
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgEndpointInit(RK_TASK_HANDLE const taskHandle);

/**
 * @brief Initialize a pool for fixed-size direct messages.
 *
 *        Pass RK_MESG_PRIO_CEILING_NONE when the pool does not need priority
 *        ceiling. Otherwise, while a task owns at least one message from this
 *        pool, it runs no lower than ceilingPrio until ownership is transferred
 *        or the message is freed.
 *
 * @param poolPtr       Memory partition object used as the message pool.
 * @param memPoolPtr    Aligned backing storage.
 * @param payloadBytes  Payload bytes available after the RK_MESG header.
 * @param nMesg         Number of message blocks in the pool.
 * @param ceilingPrio   Highest priority required while owning pool messages,
 *                      or RK_MESG_PRIO_CEILING_NONE.
 * @return              RK_ERR_SUCCESS, RK_ERR_INVALID_PARAM, or
 *                      RK_ERR_INVALID_PRIO.
 */
RK_ERR kMesgPoolInit(RK_MEM_PARTITION *const poolPtr,
                     VOID *const memPoolPtr,
                     ULONG const payloadBytes,
                     ULONG const nMesg,
                     RK_PRIO const ceilingPrio);

/**
 * @brief Allocate one message from a direct-message pool.
 * @param poolPtr      Message pool initialised with kMesgPoolInit().
 * @param mesgPtrPtr   Receives an allocated message pointer on success.
 * @param timeout      RK_NO_WAIT, RK_WAIT_FOREVER, or bounded ticks.
 * @return             Successful:
 *                                   RK_ERR_SUCCESS
 *                     Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                     Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_TIMEOUT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgAlloc(RK_MEM_PARTITION *const poolPtr,
                  RK_MESG **const mesgPtrPtr,
                  RK_TICK const timeout);

/**
 * @brief Return an allocated or received message to its originating pool.
 */
RK_ERR kMesgFree(RK_MESG *const mesgPtr);

/**
 * @brief Return the application payload address carried by a message.
 */
VOID *kMesgPayload(RK_MESG *const mesgPtr);
VOID const *kMesgPayloadConst(RK_MESG const *const mesgPtr);

/**
 * @brief Return payload capacity in bytes for this message block.
 */
ULONG kMesgPayloadBytes(RK_MESG const *const mesgPtr);

/**
 * @brief Return the sender task handle recorded at kMesgSend().
 */
RK_TASK_HANDLE kMesgGetSenderHandle(RK_MESG const *const mesgPtr);

/**
 * @brief Return the sender task ID recorded at kMesgSend().
 * @param mesgPtr      Message with a recorded sender.
 * @param senderIDPtr  Receives the sender task ID.
 * @return             RK_ERR_SUCCESS, RK_ERR_OBJ_NULL, RK_ERR_INVALID_OBJ, or
 *                     RK_ERR_MESG_INVALID_STATE when no sender has been
 *                     recorded.
 */
RK_ERR kMesgGetSenderID(RK_MESG const *const mesgPtr,
                        RK_PID *const senderIDPtr);

/**
 * @brief Transfer a message to a task endpoint.
 *        On success, the sender must not touch the message again.
 * @param taskHandle Destination task with an async endpoint.
 * @param mesgPtr    Message allocated by kMesgAlloc().
 * @return           Successful:
 *                                   RK_ERR_SUCCESS
 *                   Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_MESG_INVALID_STATE
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgSend(RK_TASK_HANDLE const taskHandle,
                 RK_MESG *const mesgPtr);

/**
 * @brief Wait for one async direct message sent to the running task.
 * @param fromTaskHandle RK_ANY_TASK or a specific sender task handle.
 * @param mesgPtrPtr     Receives the message pointer on success.
 * @param timeout        RK_NO_WAIT, RK_WAIT_FOREVER, or bounded ticks.
 * @return               Successful:
 *                                   RK_ERR_SUCCESS
 *                       Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                       Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_OBJ
 *                                   RK_ERR_INVALID_TIMEOUT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kMesgWait(RK_TASK_HANDLE const fromTaskHandle,
                 RK_MESG **const mesgPtrPtr,
                 RK_TICK const timeout);

#ifndef RK_MESG_BLOCK_SIZE_BYTES
#define RK_MESG_BLOCK_SIZE_BYTES(MESG_TYPE)                                   \
    ((ULONG)((sizeof(RK_MESG) + sizeof(MESG_TYPE) + RK_WORD_SIZE - 1UL) &     \
             ~(RK_WORD_SIZE - 1UL)))
#endif

#ifndef RK_MESG_POOL_WORDS
#define RK_MESG_POOL_WORDS(MESG_TYPE, N_MESG)                                 \
    ((UINT)((RK_MESG_BLOCK_SIZE_BYTES(MESG_TYPE) / RK_WORD_SIZE) * (N_MESG)))
#endif

#ifndef RK_DECLARE_MESG_POOL_BUF
#define RK_DECLARE_MESG_POOL_BUF(BUFNAME, MESG_TYPE, N_MESG)                  \
    ULONG BUFNAME[RK_MESG_POOL_WORDS(MESG_TYPE, N_MESG)] K_ALIGN(4);
#endif

#ifndef RK_DECLARE_MESG_POOL
#define RK_DECLARE_MESG_POOL(POOL_NAME, BUFNAME, MESG_TYPE, N_MESG)           \
    RK_DECLARE_MESG_POOL_BUF(BUFNAME, MESG_TYPE, N_MESG)                      \
    RK_MEM_PARTITION POOL_NAME;
#endif

#ifndef RK_MESG_PAYLOAD
#define RK_MESG_PAYLOAD(MESG_PTR, MESG_TYPE)                                  \
    ((MESG_TYPE *)kMesgPayload((MESG_PTR)))
#endif

#endif /* RK_CONF_ASYNCH_MESG && RK_CONF_MESG_QUEUE */
/**
 * @note
 * A task may be initialised to handle either Direct Synchronous Message or
 * Asynchronous Direct Message, but not both.
 */

/******************************************************************************/
/* SYNCHRONOUS MESSAGE (UNBUFFERED MESSAGE PASSING)                           */
/******************************************************************************/
#if (RK_CONF_SYNCH_MESG == ON)
/**
 * Synchronous Message is unbuffered message passing between two tasks. The
 * endpoint defines one maximum message size at initialisation. The sender gives
 * one non-NULL source buffer plus the actual byte count and remains blocked
 * until the receiver copies that payload into receiver-owned storage.
 * Invocation extends the same rendezvous with a server-side accept and a reply
 * copied back to the blocked caller.
 * A task that owns any mutex must not send or receive through Synchronous
 * Message; those operations return RK_ERR_TASK_INVALID_ST.
 */
/**
 * @brief Initialise the task-backed Synchronous Message endpoint.
 * @param taskHandle Task that owns the single Synchronous Message receive slot.
 * @param maxMesgBytes Maximum message size, in bytes, accepted by this
 *                     endpoint. Must be non-zero and a multiple of
 *                     RK_WORD_SIZE.
 * @return           Successful:
 *                                   RK_ERR_SUCCESS
 *                   Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_HAS_OWNER
 *                                   RK_ERR_INVALID_PARAM
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSynchMesgInit(RK_TASK_HANDLE const taskHandle,
                      ULONG const maxMesgBytes);

/**
 * @brief Send a payload directly to a task and block until copied.
 *        Success means the receiver has copied the payload before the sender
 *        was released; it does not mean the receiver has processed it or
 *        produced an answer.
 *        A bounded timeout covers both waiting for the receive slot and waiting
 *        for the receiver to copy the message.
 * @param taskHandle Receiver task handle.
 * @param mesgPtr    Non-NULL source buffer.
 * @param mesgBytes  Actual message size, in bytes. Must be non-zero, a multiple
 *                   of RK_WORD_SIZE, and no larger than the receiver endpoint
 *                   maximum configured in kSynchMesgInit().
 * @param timeout    Suspension time.
 * @return           Successful:
 *                                   RK_ERR_SUCCESS
 *                   Unsuccessful:
 *                                   RK_ERR_NOWAIT
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                                   RK_ERR_TASK_INVALID_ST
 *                                   RK_ERR_INVALID_MSG_SIZE
 *                   Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_PARAM
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSynchSendWait(RK_TASK_HANDLE const taskHandle,
                      VOID const *const mesgPtr,
                      ULONG const mesgBytes,
                      RK_TICK const timeout);

#ifndef kSynchMesgSend
#define kSynchMesgSend(TASK_HANDLE, MESG_PTR, MESG_BYTES, TIMEOUT)                 \
    kSynchSendWait((TASK_HANDLE), (MESG_PTR), (MESG_BYTES), (TIMEOUT))
#endif

/**
 * @brief Receive the payload sent to the running task.
 *        On success, the payload is copied into recvPtr before the blocked
 *        sender is released. recvPtr must point to storage large enough for
 *        the maximum message size configured in kSynchMesgInit().
 * @param recvPtr      Non-NULL destination buffer.
 * @param mesgBytesPtr Optional pointer receiving the actual copied byte count.
 * @param timeout      Suspension time.
 * @return             Successful:
 *                                   RK_ERR_SUCCESS
 *                     Unsuccessful:
 *                                   RK_ERR_BUFFER_EMPTY
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                                   RK_ERR_TASK_INVALID_ST
 *                     Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_NOT_INIT
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 */
RK_ERR kSyncRecv(VOID *const recvPtr,
                 ULONG *const mesgBytesPtr,
                 RK_TICK const timeout);

#ifndef kSynchMesgRecv
#define kSynchMesgRecv(RECV_PTR, MESG_BYTES_PTR, TIMEOUT)                          \
    kSyncRecv((RECV_PTR), (MESG_BYTES_PTR), (TIMEOUT))
#endif

/**
 * @brief Invoke a server task and wait for its reply.
 *        The request is copied into server storage by kSynchMesgAccept().
 *        The caller remains blocked until kSynchMesgReply() copies a reply
 *        back, or until the timeout expires.
 * @param taskHandle Server task handle.
 * @param attrPtr    Non-NULL invocation attributes. reqPtr/replyPtr must be
 *                   non-NULL. reqBytes is the request size. replyMaxBytes is
 *                   the caller reply-buffer capacity. replyBytesPtr optionally
 *                   receives the actual reply byte count.
 * @param timeout    RK_WAIT_FOREVER or bounded ticks. RK_NO_WAIT invalid.
 */
RK_ERR kSynchMesgCall(RK_TASK_HANDLE const taskHandle,
                      RK_SYNCH_ATTR const *const attrPtr,
                      RK_TICK const timeout);

#ifndef kSynchMesgInvoke
#define kSynchMesgInvoke(TASK_HANDLE, ATTR_PTR, TIMEOUT)                      \
    kSynchMesgCall((TASK_HANDLE), (ATTR_PTR), (TIMEOUT))
#endif

/**
 * @brief Accept one pending invocation on the running task.
 *        On success, the request is copied into recvPtr, callPtr is filled
 *        with server-local rendezvous metadata, and the caller remains blocked
 *        until kSynchMesgReply().
 */
RK_ERR kSynchMesgAccept(RK_SYNCH_CALL_DATA *const callPtr,
                        VOID *const recvPtr,
                        ULONG *const reqBytesPtr,
                        RK_TICK const timeout);

/**
 * @brief Reply to a previously accepted invocation.
 *        If the caller timed out after accept, this completes the abandoned
 *        rendezvous and no reply is copied.
 */
RK_ERR kSynchMesgReply(RK_SYNCH_CALL_DATA const *const callPtr,
                       VOID const *const replyPtr,
                       ULONG const replyBytes);

#if defined(RK_QEMU_UNIT_TEST) && !defined(RK_SOURCE_CODE)
static inline RK_ERR kSynchSendWaitDefaultBytes_(
    RK_TASK_HANDLE const taskHandle,
    VOID const *const mesgPtr,
    RK_TICK const timeout)
{
    ULONG const mesgBytes =
        (taskHandle != NULL) ? taskHandle->synchMesgMaxBytes : 0UL;
    return kSynchSendWait(taskHandle, mesgPtr, mesgBytes, timeout);
}

static inline RK_ERR kSyncRecvNoSize_(VOID *const recvPtr,
                                      RK_TICK const timeout)
{
    return kSyncRecv(recvPtr, NULL, timeout);
}

#define K_SYNCH_SEND_WAIT_3_(TASK_HANDLE, MESG_PTR, TIMEOUT)                  \
    kSynchSendWaitDefaultBytes_((TASK_HANDLE), (MESG_PTR), (TIMEOUT))
#define K_SYNCH_SEND_WAIT_4_(TASK_HANDLE, MESG_PTR, MESG_BYTES, TIMEOUT)      \
    kSynchSendWait((TASK_HANDLE), (MESG_PTR), (MESG_BYTES), (TIMEOUT))
#define K_SYNCH_SEND_WAIT_SELECT_(_1, _2, _3, _4, NAME, ...) NAME
#define kSynchSendWait(...)                                                    \
    K_SYNCH_SEND_WAIT_SELECT_(__VA_ARGS__, K_SYNCH_SEND_WAIT_4_,              \
                              K_SYNCH_SEND_WAIT_3_)(__VA_ARGS__)

#define K_SYNC_RECV_2_(RECV_PTR, TIMEOUT)                                      \
    kSyncRecvNoSize_((RECV_PTR), (TIMEOUT))
#define K_SYNC_RECV_3_(RECV_PTR, MESG_BYTES_PTR, TIMEOUT)                     \
    kSyncRecv((RECV_PTR), (MESG_BYTES_PTR), (TIMEOUT))
#define K_SYNC_RECV_SELECT_(_1, _2, _3, NAME, ...) NAME
#define kSyncRecv(...)                                                         \
    K_SYNC_RECV_SELECT_(__VA_ARGS__, K_SYNC_RECV_3_,                          \
                       K_SYNC_RECV_2_)(__VA_ARGS__)
#endif /* defined(RK_QEMU_UNIT_TEST) && !defined(RK_SOURCE_CODE) */
#endif /* RK_CONF_SYNCH_MESG */

/******************************************************************************/
/* KERNEL TRACE CONSOLE                                                       */
/******************************************************************************/
#if (RK_CONF_TRACE == ON)
/**
 * @brief Start the UART-backed kernel trace console task.
 *
 *        The console reads characters with kTraceUartGetc(). The platform UART
 *        backend should enable RX interrupts with kTraceUartRxEnable(), buffer
 *        received characters in the UART ISR, then call
 *        kTraceInputSignalFromISR() so the trace task wakes by task event.
 *        Applications must provide those UART hooks when the weak defaults are
 *        not sufficient.
 *        The console prompt accepts:
 *
 *        top           Task run count, CPU/window, priority, stack watermark,
 *                      and events.
 *        list kobjects Registered trace objects and last recorded operation.
 *        list kmesg    Registered message queues.
 *        list kipc     Task-backed Synchronous/Invocation and Asynchronous
 *                      Direct Message endpoint and wait state; SVEF/SVNOM show
 *                      server/receiver effective/nominal priority.
 *        list ksema    Registered semaphores and mutexes.
 *        list kmem     Registered memory partitions.
 *        list ksleepq  Sleep queue state.
 *        list kmrm     Most-recent-message state.
 *        list ktimers  Application timer state.
 *        list ktimerq  Raw application timer delta list.
 *        hist [name]   Operation history for one named object, or all objects.
 *        hist task/X   Priority-change history for task name or PID X.
 *        history ...   Alias for hist.
 *        dump [frames] Flush buffered KTRACE_FRAME records to trace output.
 *        help          Command summary.
 *
 *        When a trace history ring overwrites an old record, the evicted record
 *        is queued to the trace task and passed to kTraceOverflowPersist().
 *        By default that hook buffers `KTRACE_FRAME` hex records so the trace
 *        console is not flooded; the `dump` command flushes those records to
 *        the trace output. Targets can set RK_CONF_TRACE_FRAME_STDOUT to ON for
 *        immediate printing, or override the weak hook with a board-specific
 *        flash append routine.
 *
 * @return RK_ERR_SUCCESS on success. If trace was already started, the call is
 *         idempotent and also returns RK_ERR_SUCCESS. Otherwise returns the
 *         kTaskInit() error for the trace console task.
 */
RK_ERR kTraceInit(VOID);

/**
 * @brief Poll the trace UART input and execute complete console commands.
 *
 *        kTraceInit() creates a task that calls this function when UART RX
 *        signals its task event. kTracePoll() remains public for applications
 *        that want to drain trace input from their own service loop.
 */
VOID kTracePoll(VOID);

/**
 * @brief Wake the trace console task after UART RX input is buffered.
 *
 *        This function is ISR-safe when a trace task exists because it signals
 *        that task explicitly with a task event. It is a no-op before
 *        kTraceInit() creates the trace task.
 */
VOID kTraceInputSignalFromISR(VOID);

/**
 * @brief Attach a short user name to a registered kernel object.
 *
 *        The name is stored in the object's objName field and truncated to fit
 *        RK_NAME_SIZE, including the trailing NUL. Use kTraceNameObject() as the
 *        public convenience macro:
 *
 *        kTraceNameObject(&queue, "UartQ");
 *
 * @param objPtr  Pointer to an initialised traceable kernel object.
 * @param namePtr NUL-terminated name string.
 * @return RK_ERR_SUCCESS on success, RK_ERR_OBJ_NULL for NULL parameters, or
 *         RK_ERR_INVALID_OBJ if the object is not traceable/registered.
 */
RK_ERR kTraceObjectNameSet(VOID *const objPtr, CHAR const *const namePtr);

/**
 * @brief Record one operation in an object's circular trace history.
 *
 *        This is mainly used by kernel object implementations. Application code
 *        normally only names objects and reads snapshots/history. The record
 *        stores the current tick, running task PID, operation, return code, and
 *        one operation-specific numeric value. Trace operations ending in `_BLOCK`
 *        identify the operation that suspended the running task.
 *
 * @param objPtr Pointer to the registered object.
 * @param op     Operation code.
 * @param result Return/error code associated with the operation.
 * @param value  Operation-specific value, such as queue depth or timer delay.
 */
VOID kTraceRecordObject(VOID *const objPtr, RK_TRACE_OP const op,
                        RK_ERR const result, ULONG const value);

/**
 * @brief Record one effective-priority change for a task.
 *
 *        Kernel priority-inheritance/adoption code calls this after changing
 *        taskHandle->priority. The counter is surfaced by the trace `top`
 *        command, and the detailed circular history is surfaced by
 *        `hist task/<name>` or `hist task/<pid>`.
 *
 * @param taskHandle  Task whose effective priority changed.
 * @param oldPriority Previous effective priority.
 * @param newPriority New effective priority.
 */
VOID kTraceRecordTaskPrio(RK_TASK_HANDLE const taskHandle,
                          RK_PRIO const oldPriority,
                          RK_PRIO const newPriority);

/**
 * @brief Record one periodic-task overrun trace event.
 *
 *        Release overruns come from kSleepRelease() when one or more release
 *        slots were missed. Until overruns come from kSleepUntil() when the
 *        anchored release time has already elapsed. The default trace hook
 *        emits these records as buffered `KTRACE_FRAME` events so the timeline
 *        report can show when each task overran and whether it came from
 *        Release or Until.
 *
 * @param kind    RK_TRACE_OVERRUN_RELEASE or RK_TRACE_OVERRUN_UNTIL.
 * @param period  Period requested by the task.
 * @param lateBy  Ticks late at the overrun point.
 * @param skipped Release slots skipped; zero for Until.
 */
VOID kTraceRecordTaskOverrun(RK_TRACE_OVERRUN_KIND const kind,
                             RK_TICK const period, RK_TICK const lateBy,
                             ULONG const skipped);

/**
 * @brief Persist one deferred trace event from trace task context.
 *
 *        This weak hook is called after a trace record has been copied into the
 *        trace backlog. It does not run in the trace hot path. By default it
 *        buffers hex-encoded binary `KTRACE_FRAME` records; the trace `dump`
 *        command prints them so `make qemu > out.log` captures overflow and
 *        task-overrun history without flooding the console during normal use.
 *        Set RK_CONF_TRACE_FRAME_STDOUT to ON for immediate printing, or
 *        override this function to use a board-specific flash append routine.
 *
 * @param infoPtr Deferred object, task-priority, or task-overrun record.
 */
VOID kTraceOverflowPersist(RK_TRACE_OVERFLOW_INFO const *const infoPtr);

/**
 * @brief Copy the current task trace snapshot into a user buffer.
 *
 *        eventCurr is the task's current event register. eventReq/eventOpt
 *        describe the currently wanted event mask and ANY/ALL mode only while
 *        the task status is RK_SLEEPING_EV_FLAG; otherwise eventReq is 0 and
 *        eventOpt is 0. stackFirstPtr/stackLastPtr bound the task stack buffer.
 *        stackLowWaterPtr is derived from the stack paint pattern and points to
 *        the lowest stack word observed as used.
 *
 * @param infoPtr Destination array.
 * @param maxInfo Number of entries available in infoPtr.
 * @return Number of entries written.
 */
UINT kTraceTaskSnapshot(RK_TRACE_TASK_INFO *const infoPtr, UINT const maxInfo);

/**
 * @brief Copy message-passing object state into a user buffer.
 *
 *        Includes registered message queues when enabled.
 *
 * @param infoPtr Destination array.
 * @param maxInfo Number of entries available in infoPtr.
 * @return Number of entries written.
 */
#if (RK_CONF_MESG_QUEUE == ON)
UINT kTraceMesgSnapshot(RK_TRACE_OBJECT_INFO *const infoPtr,
                        UINT const maxInfo);
#endif

/**
 * @brief Copy semaphore and mutex state into a user buffer.
 *
 * @param infoPtr Destination array.
 * @param maxInfo Number of entries available in infoPtr.
 * @return Number of entries written.
 */
#if ((RK_CONF_SEMAPHORE == ON) || (RK_CONF_MUTEX == ON))
UINT kTraceSemaSnapshot(RK_TRACE_SYNC_INFO *const infoPtr, UINT const maxInfo);
#endif

/**
 * @brief Copy application timer state into a user buffer.
 *
 *        remainingTicks is the remaining delta-list time from now, including
 *        any initial phase still pending. phase reports the configured initial
 *        phase value.
 *
 * @param infoPtr Destination array.
 * @param maxInfo Number of entries available in infoPtr.
 * @return Number of entries written.
 */
#if (RK_CONF_CALLOUT_TIMER == ON)
UINT kTraceTimerSnapshot(RK_TRACE_TIMER_INFO *const infoPtr,
                         UINT const maxInfo);
#endif

/**
 * @brief Copy an object's operation history into a user buffer.
 *
 *        The newest records are returned first. The maximum available depth is
 *        RK_CONF_TRACE_RECORD_DEPTH.
 *
 * @param objPtr  Pointer to the registered object.
 * @param infoPtr Destination array.
 * @param maxInfo Number of entries available in infoPtr.
 * @return Number of entries written.
 */
UINT kTraceRecordSnapshot(VOID *const objPtr,
                          RK_TRACE_RECORD_INFO *const infoPtr,
                          UINT const maxInfo);

/**
 * @brief Copy a task's effective-priority change history into a user buffer.
 *
 *        Records are returned oldest first. The maximum available depth is
 *        RK_CONF_TRACE_RECORD_DEPTH.
 *
 * @param taskHandle Target task.
 * @param infoPtr    Destination array.
 * @param maxInfo    Number of entries available in infoPtr.
 * @return Number of entries written.
 */
UINT kTraceTaskPrioSnapshot(RK_TASK_HANDLE const taskHandle,
                            RK_TRACE_PRIO_RECORD_INFO *const infoPtr,
                            UINT const maxInfo);

/**
 * @brief Public name helper for kTraceObjectNameSet().
 *
 * @param OBJ_PTR  Pointer to an initialised traceable kernel object.
 * @param NAME_PTR NUL-terminated object name.
 * @return See kTraceObjectNameSet().
 */
#ifndef kTraceNameObject
#define kTraceNameObject(OBJ_PTR, NAME_PTR)                                    \
    kTraceObjectNameSet((VOID *)(OBJ_PTR), (NAME_PTR))
#endif
#endif /* RK_CONF_TRACE */

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
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kMRMInit(RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
                VOID *mesgPoolPtr, ULONG const nBufs,
                ULONG const dataSizeWords);
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kMRMCreate(RK_MRM_HANDLE *const mrmHandlePtr,
                  RK_MRM_BUF *const mrmPoolPtr,
                  VOID *mesgPoolPtr, ULONG const nBufs,
                  ULONG const dataSizeWords);
RK_ERR kMRMDestroy(RK_MRM_HANDLE *const mrmHandlePtr);
#endif

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
 *                                   RK_ERR_MEM_FREE
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
 *                                   RK_ERR_MEM_FREE
 */
RK_ERR kMRMUnget(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr);

#endif

#if (RK_CONF_CALLOUT_TIMER == ON)
/******************************************************************************/
/* APPLICATION TIMER                                                          */
/******************************************************************************/
/**
 * @brief Initialises and arms an application timer.
 *        The first expiry is ordered by phase + countTicks. A fired one-shot
 *        timer remains initialised but inactive; there is no public rearm API.
 * @param kobj  Timer Object address
 * @param phase Initial phase delay; does not apply to reloads.
 * @param countTicks Period/expiry delay in ticks. Must be non-zero.
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
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kTimerCreate(RK_TIMER_HANDLE *const timerHandlePtr,
                    RK_TICK const phase,
                    RK_TICK const countTicks,
                    RK_TIMER_CALLOUT const funPtr,
                    VOID *const argsPtr, RK_OPTION const reload);
RK_ERR kTimerDestroy(RK_TIMER_HANDLE *const timerHandlePtr);
#endif

/**
 * @brief       Cancel an initialised timer. Cancelling an inactive one-shot is
 *              a successful no-op.
 * @param kobj  Timer object address
 * @return      Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_INVALID_OBJ
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
 *              This is a relative delay and is not suitable for periodic
 *              tasks because execution time and release jitter accumulate
 *              across iterations. Use kSleepPeriodic()/kSleepRelease() or
 *              kSleepUntil() for periodic task releases.
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
 *          Each skipped release records a task overrun event and increments
 *          the task overrun counter shown by trace `top`.
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
 *          or return RK_ERR_ELAPSED_PERIOD immediately. It does not skip.
 *          Each elapsed-period return records an Until overrun event and
 *          increments the task overrun counter shown by trace `top`.
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
 *          Counts only ticks observed while the caller is running; time
 *          elapsed during preemption is not accumulated.
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
 * @param memPoolPtr Address of a word-aligned pool (typically declared with
 *                   RK_DECLARE_MEM_POOL()).
 * @param blkSize Size of each block in bytes; rounded up to a word internally.
 * @param numBlocks Number of blocks; must be at least 1.
 * @return                  Successful:
 *                                   RK_ERR_SUCCESS
 *                      Errors:
 *                                   RK_ERR_OBJ_NULL
 *                                   RK_ERR_OBJ_DOUBLE_INIT
 *                                   RK_ERR_INVALID_PARAM
 */
RK_ERR kMemPartitionInit(RK_MEM_PARTITION *const kobj, VOID *memPoolPtr,
                         ULONG blkSize, const ULONG numBlocks);
#ifndef RK_DECLARE_MEM_POOL
#define RK_DECLARE_MEM_POOL(TYPE, BUFNAME, N_BLOCKS)                           \
    ULONG BUFNAME[N_BLOCKS][RK_TYPE_WORD_COUNT(TYPE)] K_ALIGN(4);
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
 *                                   RK_ERR_MEM_FREE       Invalid pointer,
 *                                                         misaligned pointer,
 *                                                         or double free.
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
#if ((RK_CONF_SLEEP_QUEUE == ON) && (RK_CONF_MUTEX == ON) &&                  \
     (RK_CONF_CONDVAR == ON))


/**
 * @brief   Initialises a pair (Sleep Queue, Mutex), with PIP enabled.
 *
 *  @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_NOWAIT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                  Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   (plus propagated mutex/Sleep Queue errors)
 */

RK_ERR kCondVarInit(RK_SLEEP_QUEUE *const cv, RK_MUTEX *const mutex);
/**
 * @brief Condition Variable Wait.
 *        Unlocks associated mutex and suspends task.
 *        If the mutex was successfully unlocked, the function attempts to
 *        reacquire it before returning, including timeout and no-wait returns.
 *        The timeout bounds both cond wait + lock.
 * @return          Successful:
 *                                   RK_ERR_SUCCESS
 *                  Unsuccessful:
 *                                   RK_ERR_TIMEOUT
 *                                   RK_ERR_NOWAIT
 *                                   RK_ERR_INVALID_TIMEOUT
 *                  Errors:
 *                                   RK_ERR_INVALID_ISR_PRIMITIVE
 *                                   (plus propagated mutex/Sleep Queue errors)
 */
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
 *                                   (plus propagated Sleep Queue errors)
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
 *                                   (plus propagated Sleep Queue errors)
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
