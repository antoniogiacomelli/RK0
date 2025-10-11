/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_CONFIG_H
#define RK_CONFIG_H

#define ON   1U
#define OFF  0U

/******************************************************************************/
/********* 1. TASKS AND SCHEDULER *********************************************/
/******************************************************************************/

/*** [ • SYSTEM TASKS STACK SIZE (WORDS) **************************************/
/******************************************************************************/
/* !!! IMPORTANT !!!                                                          */
/*                                                                            */
/* This configuration is exposed so the system programmer can adjust          */
/* the IdleTask stack size to support any hook.                               */
/*                                                                            */
/* The Timer Handler stack size must be adjusted to support                   */
/* Application Timers callouts.                                               */
/*                                                                            */
/* System Tasks are in core/ksystasks.c.                                      */
/*                                                                            */
/* (1 Word = 4 bytes)                                                         */
/*                                                                            */
/* (!) Keep it aligned to a double-word (8-byte) boundary.                    */
/******************************************************************************/
#define RK_CONF_IDLE_STACKSIZE              (64)        /* Words */
#define RK_CONF_TIMHANDLER_STACKSIZE        (128)       /* Words */

/***[• USER-DEFINED TASKS (NUMBER) ********************************************/
#define RK_CONF_N_USRTASKS                  (5)

/***[• MINIMAL EFFECTIVE PRIORITY (HIGHEST PRIORITY NUMBER)  ******************/
#define RK_CONF_MIN_PRIO                    (5)

/***[• SYSTEM CORE CLOCK AND KERNEL TICK **************************************/
/* If using CMSIS you can set this value to 0, so it will fallback to */ 
/* the standard CMSIS SystemCoreClock. (!NOT VALID FOR QEMU!)         */
#define RK_CONF_SYSCORECLK                  (5000000UL)

/* This will set the tick as 1/RK_SYSTICK_DIV millisec                     */
/* 1000 -> 1 ms Tick, 500 -> 2 ms Tick, 100 -> 10ms Tick, and so forth     */
#define RK_CONF_SYSTICK_DIV                 (100UL)

/******************************************************************************/
/********* 2. APPLICATION TIMER  **********************************************/
/******************************************************************************/

#define RK_CONF_CALLOUT_TIMER                    (ON)

/******************************************************************************/
/********* 3. INTER-TASK COMMUNICATION ****************************************/
/******************************************************************************/

#define RK_CONF_SLEEP_QUEUE                      (ON)

#define RK_CONF_SEMAPHORE                        (ON)

#define RK_CONF_MUTEX                            (ON)

#define RK_CONF_MESG_QUEUE                       (ON)
#if (RK_CONF_MESG_QUEUE == ON)
#define RK_CONF_MESG_QUEUE_NOTIFY                (ON)
#define RK_CONF_PORTS                            (ON)
#endif

#define RK_CONF_MRM                              (ON)

/******************************************************************************/
/********* 4. ERROR CHECKING    ***********************************************/
/******************************************************************************/
/* The kernel can return error codes (RK_CONF_ERR_CHECK) plus also halting    */
/* execution (RK_CONF_FAULT) upon faulty operations request, such as a        */
/* blocking call within an ISR.                                               */
/* Note that an unsuccessful return value is not synonymous with error.       */
/* An unsuccesful 'try' post to a full RK_MAILBOX or a 'signal' to a empty    */
/* RK_SLEEP_QUEUE, for instance  are well-defined operations,  that do not.   */
/* lead to system failure.                                                    */
/* SUCCESSFUL operations return 0. Unsuccesful are > 0. Errors are < 0.       */

#if !defined(NDEBUG)
#define RK_CONF_ERR_CHECK                    (ON)
#if (RK_CONF_ERR_CHECK == ON)
#define RK_CONF_FAULT                        (ON)
#endif
#endif

/******************************************************************************/
/********* 5. OTHERS  *********************************************************/
/******************************************************************************/
/* This accumulates the number of ticks in the IDLE TASK on the global
idleTicks */
#define RK_CONF_IDLE_TICK_COUNT              (ON)

#endif /* KCONFIG_H */
