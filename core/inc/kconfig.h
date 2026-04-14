/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.18.0                                                          */
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

/***[ DYNAMIC TASK CREATION **************************************************/
/* Enables/disables runtime task creation via kTaskSpawn(). */
#ifndef RK_CONF_DYNAMIC_TASK
#define RK_CONF_DYNAMIC_TASK (ON)
#endif

/***[ MAXIMUM NUMBER OF USER TASKS  ******************************************/
/*
Maximum number of user tasks supported by the kernel, including tasks to be
created after the scheduler starts (so-called "dynamic tasks")
If using the Application Logger facility, the Logger Task should be taken into
account.
 */
#ifndef RK_CONF_N_USRTASKS_MAX
#define RK_CONF_N_USRTASKS_MAX (4)
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

#define RK_CONF_CALLOUT_TIMER (ON)

/******************************************************************************/
/********* 3. INTER-TASK COMMUNICATION ****************************************/
/******************************************************************************/

/*** SHARED-STATE MECHANISMS ***/

/* SEMAPHORES (COUNTING/BINARY) */
#define RK_CONF_SEMAPHORE (ON)

/* MUTEX LOCK */
#define RK_CONF_MUTEX (ON)

/* SLEEP QUEUE */
#define RK_CONF_SLEEP_QUEUE (ON)

#if (RK_CONF_SLEEP_QUEUE == ON && RK_CONF_MUTEX == ON)
/* Condition Variable Model Helpers */
#define RK_CONF_CONDVAR (ON)
#endif


/*** MESSAGE-PASSING MECHANISMS ***/

/* MESSAGE QUEUE  */

#define RK_CONF_MESG_QUEUE (ON)

#if (RK_CONF_MESG_QUEUE == ON)

#define RK_CONF_MESG_QUEUE_SEND_CALLBACK (ON)

#endif /* RK_CONF_MESG_QUEUE */

/* CHANNELS */
#define RK_CONF_CHANNEL (ON)

/* MRM PROTOCOL */
#define RK_CONF_MRM (ON)

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
