/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.84.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/* KERNEL CONFIGURATION FILE                                                  */
/******************************************************************************/
#ifndef RK_CONFIG_H
#define RK_CONFIG_H

#define ON 1U
#define OFF 0U
#define RK_CONFIG_BOOL_VALID(config) (((config) == ON) || ((config) == OFF))

/******************************************************************************/
/********* CONFIGURATION DEFAULTS *********************************************/
/******************************************************************************/

/***[ TARGET ARCHITECTURE *****************************************************/
#ifndef RK_CONF_ARMV6M
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_6M) ||                    \
    defined(RK_ARCH_ARMV6M)
#define RK_CONF_ARMV6M (ON)
#else
#define RK_CONF_ARMV6M (OFF)
#endif
#endif

/******************************************************************************/
/********* 1. TASKS AND SCHEDULER *********************************************/
/******************************************************************************/

/* System-task stack sizes in words. Keep them 8-byte aligned. */
#define RK_CONF_IDLE_STACKSIZE (128)
#define RK_CONF_POSTPROC_STACKSIZE (256)

/***[ KERNEL TRACE CONSOLE ***************************************************/
#ifndef RK_CONF_TRACE
#define RK_CONF_TRACE (OFF)
#endif

#if (RK_CONF_TRACE == ON)
#ifndef RK_CONF_TRACE_STACKSIZE
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_STACKSIZE (160U)
#else
#define RK_CONF_TRACE_STACKSIZE (480U)
#endif
#endif

#ifndef RK_CONF_TRACE_PRIO
#define RK_CONF_TRACE_PRIO (RK_CONF_MIN_PRIO)
#endif

#ifndef RK_CONF_TRACE_MAX_OBJECTS
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_MAX_OBJECTS (6U)
#else
#define RK_CONF_TRACE_MAX_OBJECTS (16U)
#endif
#endif

#ifndef RK_CONF_TRACE_LINE_LEN
#define RK_CONF_TRACE_LINE_LEN (32U)
#endif

#ifndef RK_CONF_TRACE_RECORD_DEPTH
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_RECORD_DEPTH (2U)
#else
#define RK_CONF_TRACE_RECORD_DEPTH (10U)
#endif
#endif

#ifndef RK_CONF_TRACE_OVERFLOW_BACKLOG
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_OVERFLOW_BACKLOG (0U)
#else
#define RK_CONF_TRACE_OVERFLOW_BACKLOG (8U)
#endif
#endif

#ifndef RK_CONF_TRACE_FRAME_BUFFER_DEPTH
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_FRAME_BUFFER_DEPTH (0U)
#else
#define RK_CONF_TRACE_FRAME_BUFFER_DEPTH (64U)
#endif
#endif
#endif /* RK_CONF_TRACE */

#ifndef RK_CONF_TRACE_FRAME_STDOUT
#define RK_CONF_TRACE_FRAME_STDOUT (OFF)
#endif

#ifndef RK_CONF_TRACE_TASK_PRIO_HISTORY
#if ((RK_CONF_TRACE == ON) && (RK_CONF_ARMV6M == OFF))
#define RK_CONF_TRACE_TASK_PRIO_HISTORY (ON)
#else
#define RK_CONF_TRACE_TASK_PRIO_HISTORY (OFF)
#endif
#endif

/***[ DYNAMIC CREATION *******************************************************/
#ifndef RK_CONF_DYNAMIC_TASK
#define RK_CONF_DYNAMIC_TASK (ON)
#endif

#ifndef RK_CONF_DYNAMIC_OBJECTS
#define RK_CONF_DYNAMIC_OBJECTS (OFF)
#endif

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
#ifndef RK_CONF_DYNAMIC_SEMAPHORES_MAX
#define RK_CONF_DYNAMIC_SEMAPHORES_MAX (4U)
#endif
#ifndef RK_CONF_DYNAMIC_MUTEXES_MAX
#define RK_CONF_DYNAMIC_MUTEXES_MAX (4U)
#endif
#ifndef RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX
#define RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX (4U)
#endif
#ifndef RK_CONF_DYNAMIC_MESG_QUEUES_MAX
#define RK_CONF_DYNAMIC_MESG_QUEUES_MAX (4U)
#endif
#ifndef RK_CONF_DYNAMIC_TIMERS_MAX
#define RK_CONF_DYNAMIC_TIMERS_MAX (4U)
#endif
#ifndef RK_CONF_DYNAMIC_MRMS_MAX
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_DYNAMIC_MRMS_MAX (1U)
#else
#define RK_CONF_DYNAMIC_MRMS_MAX (2U)
#endif
#endif
#endif /* RK_CONF_DYNAMIC_OBJECTS */

#ifndef RK_CONF_N_USRTASKS_MAX
#define RK_CONF_N_USRTASKS_MAX (31)
#endif

/***[ CLOCK AND TIMING *******************************************************/
/* A zero core clock selects the platform default. */
#ifndef RK_CONF_SYSCORECLK
#define RK_CONF_SYSCORECLK (0UL)
#endif

/* Tick frequency in Hz: 1000 = 1 ms, 500 = 2 ms, 100 = 10 ms. */
#ifndef RK_CONF_SYSTICK_DIV
#define RK_CONF_SYSTICK_DIV (100UL)
#endif

#ifndef RK_CONF_ROUND_UP_MS_TO_TICKS
#define RK_CONF_ROUND_UP_MS_TO_TICKS (OFF)
#endif

#ifndef RK_CONF_SLEEP_RELEASE
#define RK_CONF_SLEEP_RELEASE (ON)
#endif

#ifndef RK_CONF_SLEEP_UNTIL
#define RK_CONF_SLEEP_UNTIL (ON)
#endif

#ifndef RK_CONF_BUSY_DELAY
#define RK_CONF_BUSY_DELAY (ON)
#endif

/******************************************************************************/
/********* 2. APPLICATION TIMER ***********************************************/
/******************************************************************************/

#ifndef RK_CONF_CALLOUT_TIMER
#define RK_CONF_CALLOUT_TIMER (OFF)
#endif

/******************************************************************************/
/********* 3. INTER-TASK COMMUNICATION ***************************************/
/******************************************************************************/

/*** SHARED-STATE MECHANISMS ***/
#ifndef RK_CONF_SEMAPHORE
#define RK_CONF_SEMAPHORE (OFF)
#endif

#ifndef RK_CONF_MUTEX
#define RK_CONF_MUTEX (OFF)
#endif

#ifndef RK_CONF_SLEEP_QUEUE
#define RK_CONF_SLEEP_QUEUE (OFF)
#endif

#ifndef RK_CONF_BARRIER
#define RK_CONF_BARRIER (OFF)
#endif

#ifndef RK_CONF_CONDVAR
#define RK_CONF_CONDVAR (ON)
#endif

/*** MESSAGE-PASSING MECHANISMS ***/
#ifndef RK_CONF_EXCHG
#define RK_CONF_EXCHG (OFF)
#endif

#ifndef RK_CONF_EXCHG_BROADCAST
#define RK_CONF_EXCHG_BROADCAST (OFF)
#endif

#ifndef RK_CONF_MESG_QUEUE
#define RK_CONF_MESG_QUEUE (OFF)
#endif

#ifndef RK_CONF_MESG_QUEUE_SEND_CALLBACK
#if (RK_CONF_MESG_QUEUE == ON)
#define RK_CONF_MESG_QUEUE_SEND_CALLBACK (ON)
#else
#define RK_CONF_MESG_QUEUE_SEND_CALLBACK (OFF)
#endif
#endif

#ifndef RK_CONF_MESG_QUEUE_PEEK
#define RK_CONF_MESG_QUEUE_PEEK (OFF)
#endif

#ifndef RK_CONF_MESG_QUEUE_JAM
#define RK_CONF_MESG_QUEUE_JAM (OFF)
#endif

#ifndef RK_CONF_MESG_QUEUE_OVERWRITE
#define RK_CONF_MESG_QUEUE_OVERWRITE (OFF)
#endif

#ifndef RK_CONF_MESG_QUEUE_QUERY
#define RK_CONF_MESG_QUEUE_QUERY (OFF)
#endif

#ifndef RK_CONF_MESG_QUEUE_RESET
#define RK_CONF_MESG_QUEUE_RESET (OFF)
#endif

#ifndef RK_CONF_MBOX_BROADCAST
#define RK_CONF_MBOX_BROADCAST (OFF)
#endif

#ifndef RK_CONF_ASYNCH_MESG
#if (RK_CONF_MESG_QUEUE == ON)
#define RK_CONF_ASYNCH_MESG (ON)
#else
#define RK_CONF_ASYNCH_MESG (OFF)
#endif
#endif

#ifndef RK_CONF_SYNCH_MESG
#define RK_CONF_SYNCH_MESG (OFF)
#endif

#ifndef RK_CONF_MRM
#define RK_CONF_MRM (OFF)
#endif

/******************************************************************************/
/********* 4. ERROR HANDLING **************************************************/
/******************************************************************************/

#ifndef RK_CONF_ERR_CHECK
#if defined(NDEBUG)
#define RK_CONF_ERR_CHECK (OFF)
#else
#define RK_CONF_ERR_CHECK (ON)
#endif
#endif

#ifndef RK_CONF_FAULT
#if (RK_CONF_ERR_CHECK == ON)
#define RK_CONF_FAULT (ON)
#else
#define RK_CONF_FAULT (OFF)
#endif
#endif

#ifndef RK_CONF_FAULT_PRINT_STDERR
#if (RK_CONF_ERR_CHECK == ON)
#define RK_CONF_FAULT_PRINT_STDERR (ON)
#else
#define RK_CONF_FAULT_PRINT_STDERR (OFF)
#endif
#endif

/******************************************************************************/
/********* CONFIGURATION CHECKS ***********************************************/
/******************************************************************************/

#if (!RK_CONFIG_BOOL_VALID(RK_CONF_ARMV6M) ||                               \
     !RK_CONFIG_BOOL_VALID(RK_CONF_TRACE) ||                                \
     !RK_CONFIG_BOOL_VALID(RK_CONF_TRACE_FRAME_STDOUT) ||                   \
     !RK_CONFIG_BOOL_VALID(RK_CONF_TRACE_TASK_PRIO_HISTORY) ||              \
     !RK_CONFIG_BOOL_VALID(RK_CONF_DYNAMIC_TASK) ||                         \
     !RK_CONFIG_BOOL_VALID(RK_CONF_DYNAMIC_OBJECTS))
#error "Task and scheduler switches must be ON or OFF"
#endif

#if (!RK_CONFIG_BOOL_VALID(RK_CONF_ROUND_UP_MS_TO_TICKS) ||                 \
     !RK_CONFIG_BOOL_VALID(RK_CONF_SLEEP_RELEASE) ||                        \
     !RK_CONFIG_BOOL_VALID(RK_CONF_SLEEP_UNTIL) ||                          \
     !RK_CONFIG_BOOL_VALID(RK_CONF_BUSY_DELAY) ||                           \
     !RK_CONFIG_BOOL_VALID(RK_CONF_CALLOUT_TIMER))
#error "Timing switches must be ON or OFF"
#endif

#if (!RK_CONFIG_BOOL_VALID(RK_CONF_SEMAPHORE) ||                            \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MUTEX) ||                                \
     !RK_CONFIG_BOOL_VALID(RK_CONF_SLEEP_QUEUE) ||                          \
     !RK_CONFIG_BOOL_VALID(RK_CONF_BARRIER) ||                              \
     !RK_CONFIG_BOOL_VALID(RK_CONF_CONDVAR) ||                              \
     !RK_CONFIG_BOOL_VALID(RK_CONF_EXCHG) ||                                \
     !RK_CONFIG_BOOL_VALID(RK_CONF_EXCHG_BROADCAST) ||                      \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE) ||                           \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE_SEND_CALLBACK) ||             \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE_PEEK) ||                      \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE_JAM) ||                       \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE_OVERWRITE) ||                 \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE_QUERY) ||                     \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MESG_QUEUE_RESET) ||                     \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MBOX_BROADCAST) ||                       \
     !RK_CONFIG_BOOL_VALID(RK_CONF_ASYNCH_MESG) ||                          \
     !RK_CONFIG_BOOL_VALID(RK_CONF_SYNCH_MESG) ||                           \
     !RK_CONFIG_BOOL_VALID(RK_CONF_MRM))
#error "Inter-task communication switches must be ON or OFF"
#endif

#if (!RK_CONFIG_BOOL_VALID(RK_CONF_ERR_CHECK) ||                            \
     !RK_CONFIG_BOOL_VALID(RK_CONF_FAULT) ||                                \
     !RK_CONFIG_BOOL_VALID(RK_CONF_FAULT_PRINT_STDERR))
#error "Error-handling switches must be ON or OFF"
#endif

#if ((RK_CONF_EXCHG == OFF) && (RK_CONF_EXCHG_BROADCAST == ON))
#error "RK_CONF_EXCHG_BROADCAST requires RK_CONF_EXCHG"
#endif

#if ((RK_CONF_MESG_QUEUE == OFF) &&                                         \
     ((RK_CONF_MESG_QUEUE_PEEK == ON) ||                                    \
      (RK_CONF_MESG_QUEUE_JAM == ON) ||                                     \
      (RK_CONF_MESG_QUEUE_OVERWRITE == ON) ||                               \
      (RK_CONF_MESG_QUEUE_QUERY == ON) ||                                   \
      (RK_CONF_MESG_QUEUE_RESET == ON) ||                                   \
      (RK_CONF_MBOX_BROADCAST == ON)))
#error "Message-queue options require RK_CONF_MESG_QUEUE"
#endif

/******************************************************************************/
/********* QEMU UNIT TEST CONFIGURATION ***************************************/
/******************************************************************************/
#if defined(RK_QEMU_UNIT_TEST)
/* Unit tests compile all exercised services into every test image. */
#undef RK_CONF_DYNAMIC_TASK
#define RK_CONF_DYNAMIC_TASK (ON)
#undef RK_CONF_SLEEP_RELEASE
#define RK_CONF_SLEEP_RELEASE (ON)
#undef RK_CONF_SLEEP_UNTIL
#define RK_CONF_SLEEP_UNTIL (ON)
#undef RK_CONF_BUSY_DELAY
#define RK_CONF_BUSY_DELAY (ON)
#undef RK_CONF_CALLOUT_TIMER
#define RK_CONF_CALLOUT_TIMER (ON)

#undef RK_CONF_SEMAPHORE
#define RK_CONF_SEMAPHORE (ON)
#undef RK_CONF_MUTEX
#define RK_CONF_MUTEX (ON)
#undef RK_CONF_SLEEP_QUEUE
#define RK_CONF_SLEEP_QUEUE (ON)

#undef RK_CONF_EXCHG
#define RK_CONF_EXCHG (ON)
#undef RK_CONF_EXCHG_BROADCAST
#define RK_CONF_EXCHG_BROADCAST (ON)
#undef RK_CONF_MESG_QUEUE
#define RK_CONF_MESG_QUEUE (ON)
#undef RK_CONF_MESG_QUEUE_SEND_CALLBACK
#define RK_CONF_MESG_QUEUE_SEND_CALLBACK (ON)
#undef RK_CONF_MESG_QUEUE_PEEK
#define RK_CONF_MESG_QUEUE_PEEK (ON)
#undef RK_CONF_MESG_QUEUE_JAM
#define RK_CONF_MESG_QUEUE_JAM (ON)
#undef RK_CONF_MESG_QUEUE_OVERWRITE
#define RK_CONF_MESG_QUEUE_OVERWRITE (ON)
#undef RK_CONF_MESG_QUEUE_QUERY
#define RK_CONF_MESG_QUEUE_QUERY (ON)
#undef RK_CONF_MESG_QUEUE_RESET
#define RK_CONF_MESG_QUEUE_RESET (ON)
#undef RK_CONF_MBOX_BROADCAST
#define RK_CONF_MBOX_BROADCAST (ON)
#undef RK_CONF_ASYNCH_MESG
#define RK_CONF_ASYNCH_MESG (ON)
#undef RK_CONF_SYNCH_MESG
#define RK_CONF_SYNCH_MESG (ON)
#undef RK_CONF_MRM
#define RK_CONF_MRM (ON)

#undef RK_CONF_UNIT_TEST_TASKS
#define RK_CONF_UNIT_TEST_TASKS (4)
#undef RK_CONF_N_USRTASKS_MAX
#define RK_CONF_N_USRTASKS_MAX RK_CONF_UNIT_TEST_TASKS
#undef RK_CONF_SYSTICK_DIV
#define RK_CONF_SYSTICK_DIV (1000UL)
#endif /* RK_QEMU_UNIT_TEST */

#endif /* RK_CONFIG_H */
