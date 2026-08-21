/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.0                                                          */
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

/*** [  SYSTEM TASKS STACK SIZE (WORDS) **************************************/
/******************************************************************************/
/* This configuration is exposed so the system programmer can adjust          */
/* the IdleTask stack size to support any hook.                               */
/*                                                                            */
/* The Post-Processing system task stack size must be adjusted to support     */
/* Application Timers callouts.                                               */
/* (!) Minimal stack size is 128                                              */
/* (!) Keep it aligned to a double-word (8-byte) boundary.                    */
/******************************************************************************/
#define RK_CONF_IDLE_STACKSIZE (128)     /* Words */
#define RK_CONF_POSTPROC_STACKSIZE (256) /* Words */

/***[ KERNEL TRACE CONSOLE ***************************************************/
#ifndef RK_CONF_TRACE_SUPPORTED
#define RK_CONF_TRACE_SUPPORTED (ON)
#endif

#ifndef RK_CONF_TRACE
#if (RK_CONF_TRACE_SUPPORTED == ON)
#define RK_CONF_TRACE (ON)
#else
#define RK_CONF_TRACE (OFF)
#endif
#endif

#if ((RK_CONF_TRACE == ON) && (RK_CONF_TRACE_SUPPORTED == OFF))
#error "RK_CONF_TRACE is not supported on this target"
#endif

#if (RK_CONF_TRACE == ON)
#ifndef RK_CONF_TRACE_STACKSIZE
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_STACKSIZE (160U)
#elif defined(QEMU_MACHINE_LM3S6965EVB)
#define RK_CONF_TRACE_STACKSIZE (480U)
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
#ifndef RK_CONF_TRACE_FRAME_STDOUT
#define RK_CONF_TRACE_FRAME_STDOUT (OFF)
#endif
#ifndef RK_CONF_TRACE_TASK_PRIO_HISTORY
#if (RK_CONF_ARMV6M == ON)
#define RK_CONF_TRACE_TASK_PRIO_HISTORY (OFF)
#else
#define RK_CONF_TRACE_TASK_PRIO_HISTORY (ON)
#endif
#endif
#endif

/***[ DYNAMIC TASK CREATION **************************************************/
/* Enables/disables runtime task creation via kTaskSpawn(). */
#ifndef RK_CONF_DYNAMIC_TASK
#if defined(RK_QEMU_UNIT_TEST)
#define RK_CONF_DYNAMIC_TASK (ON)
#else
#define RK_CONF_DYNAMIC_TASK (ON)
#endif
#endif

/***[ DYNAMIC KERNEL OBJECT CREATION ******************************************/
/* Enables/disables runtime creation of non-task kernel objects. */
#ifndef RK_CONF_DYNAMIC_OBJECTS
#define RK_CONF_DYNAMIC_OBJECTS (ON)
#endif
/***[ NUMBER OF KERNEL OBJECTS ************************************************/
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

/***[ MAXIMUM NUMBER OF USER TASKS  ******************************************/
/*
Maximum number of user tasks supported by the kernel, including tasks to be
created after the scheduler starts (so-called "dynamic tasks")
If using the Application Logger facility, the Logger Task should be taken into
account.
 */
#ifndef RK_CONF_N_USRTASKS_MAX
#define RK_CONF_N_USRTASKS_MAX (31)
#endif

/***[ SYSTEM CORE CLOCK  *****************************************************/
/* If using CMSIS-Core HAL you can set this value to 0, so it will fallback   */
/* to CMSIS SystemCoreClock. (Not valid for QEMU buildings).                  */
/* Note CMSIS-Core is not bundled in RK0.                                     */
#define RK_CONF_SYSCORECLK (50000000UL)

/***[ KERNEL TICK ************************************************************/
/* This will set the tick as 1/RK_SYSTICK_DIV millisec                        */
/* 1000 -> 1 ms Tick, 500 -> 2 ms Tick, 100 -> 10ms Tick, and so forth        */
/* Recommended tick for applications running on low-end devices is 10ms       */
#define RK_CONF_SYSTICK_DIV (100UL)

/******************************************************************************/
/********* 2. APPLICATION TIMER  **********************************************/
/******************************************************************************/

#ifndef RK_CONF_CALLOUT_TIMER
#if defined(RK_QEMU_UNIT_TEST)
#define RK_CONF_CALLOUT_TIMER (ON)
#else
#define RK_CONF_CALLOUT_TIMER (ON)
#endif
#endif

/******************************************************************************/
/********* 3. INTER-TASK COMMUNICATION ****************************************/
/******************************************************************************/

/*** SHARED-STATE MECHANISMS ***/

/* SEMAPHORES (COUNTING/BINARY) */
#ifndef RK_CONF_SEMAPHORE
#define RK_CONF_SEMAPHORE (ON)
#endif

/* MUTEX LOCK */
#ifndef RK_CONF_MUTEX
#define RK_CONF_MUTEX (ON)
#endif

/* SLEEP QUEUE */
#ifndef RK_CONF_SLEEP_QUEUE
#define RK_CONF_SLEEP_QUEUE (ON)
#endif

#if (RK_CONF_SLEEP_QUEUE == ON && RK_CONF_MUTEX == ON)
/* Condition Variable Model Helpers */
#ifndef RK_CONF_CONDVAR
#define RK_CONF_CONDVAR (ON)
#endif
#endif


/*** MESSAGE-PASSING MECHANISMS ***/

/* MESSAGE QUEUE  */

#ifndef RK_CONF_MESG_QUEUE
#define RK_CONF_MESG_QUEUE (ON)
#endif

#if (RK_CONF_MESG_QUEUE == ON)

#ifndef RK_CONF_MESG_QUEUE_SEND_CALLBACK
#define RK_CONF_MESG_QUEUE_SEND_CALLBACK (ON)
#endif

/* ASYNCHRONOUS DIRECT MESSAGE */
#ifndef RK_CONF_ASYNCH_MESGS
#define RK_CONF_ASYNCH_MESG (ON)
#endif

#endif /* RK_CONF_MESG_QUEUE */

/* SYNCHRONOUS UNBUFFERED MESSAGE */
#ifndef RK_CONF_SYNCH_MESG
#define RK_CONF_SYNCH_MESG (ON)
#endif

/* MRM PROTOCOL */
#ifndef RK_CONF_MRM
#define RK_CONF_MRM (ON)
#endif

/******************************************************************************/
/********* 4. ERROR CHECKING    ***********************************************/
/******************************************************************************/
/* The kernel can return error codes (RK_CONF_ERR_CHECK) plus also halting    */
/* execution (RK_CONF_FAULT) upon faulty operations request, such as a        */
/* blocking call within an ISR.                                               */
/* Note that an unsuccessful return value is not synonymous with error.       */
/* An unsuccesful 'try' post to a full single-slot queue or a 'signal' to a   */
/* empty RK_SLEEP_QUEUE, for instance are well-defined operations, that do not*/
/* lead to system failure.                                                    */
/* SUCCESSFUL operations return 0. UNSUCCESFUL are > 0. ERRORS are < 0.       */

#if !defined(NDEBUG)
#define RK_CONF_ERR_CHECK (ON)
#if (RK_CONF_ERR_CHECK == ON)
#define RK_CONF_FAULT (ON)
#define RK_CONF_FAULT_PRINT_STDERR (ON)
#endif
#endif

/* DO NOT CHANGE THIS ONE */
#if defined(RK_QEMU_UNIT_TEST)
/***  FOR UNIT TESTS THESE MUST BE THE CONFIGURATIONS */
#define RK_CONF_UNIT_TEST_TASKS 4
/* QEMU unit tests rely on fixed task-count/tick settings across modules. */
#undef RK_CONF_N_USRTASKS_MAX
#define RK_CONF_N_USRTASKS_MAX RK_CONF_UNIT_TEST_TASKS

#undef RK_CONF_SYSTICK_DIV
#define RK_CONF_SYSTICK_DIV (100UL)
#endif

#endif /* KCONFIG_H */
