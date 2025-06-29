/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.4
 * Architecture     :   ARMv6/7-M
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

#define RK_CONF_N_USRTASKS                  (3)

/***[• MINIMAL EFFECTIVE PRIORITY (HIGHEST PRIORITY NUMBER)  ******************/

#define RK_CONF_MIN_PRIO                    (3)

/***[• SYSTEM CORE CLOCK AND KERNEL TICK **************************************/

/* If using CMSIS you can set this value to 0, so it will fallback to */ 
/* the standard CMSIS SystemCoreClock */
#define RK_CONF_SYSCORECLK                   (50000000UL)

/* This will set the tick as 1/RK_SYSTICK_DIV millisec                     */
/* 1000 -> 1 ms Tick, 500 -> 2 ms Tick, 100 -> 10ms Tick, and so forth      */
#define RK_CONF_SYSTICK_DIV                  (1000)

/******************************************************************************/
/********* 2. APPLICATION TIMER  **********************************************/
/******************************************************************************/

#define RK_CONF_CALLOUT_TIMER                 (ON)

/******************************************************************************/
/********* 3. SYNCHRONISATION  ************************************************/
/******************************************************************************/

/***[• SLEEP-WAKE EVENTS (CONDITION VARIABLES) ********************************/

#define RK_CONF_EVENT                         (ON)

/***[• SEMAPHORES (COUNTING/BINARY) *******************************************/

#define RK_CONF_SEMA                          (ON)

/***[• MUTEX SEMAPHORES *******************************************************/

#define RK_CONF_MUTEX                         (ON)

/******************************************************************************/
/********* 3. MESSAGE-PASSING  ************************************************/
/******************************************************************************/

/***[• MAILBOX ****************************************************************/

#define RK_CONF_MBOX                          (ON)

#if (RK_CONF_MBOX == ON)

/*-- CONFIG: OPTIONAL FUNCTIONS       -*/
#define RK_CONF_FUNC_MBOX_QUERY               (ON)
#define RK_CONF_FUNC_MBOX_PEEK                (ON)
#define RK_CONF_FUNC_MBOX_POSTOVW             (ON)

#endif

/***[• MAIL QUEUE  ************************************************************/

#define RK_CONF_QUEUE                          (ON)

#if (RK_CONF_QUEUE == ON)

/*-- CONFIG: OPTIONAL FUNCTIONS  -*/
#define RK_CONF_FUNC_QUEUE_PEEK                (ON)
#define RK_CONF_FUNC_QUEUE_QUERY               (ON)
#define RK_CONF_FUNC_QUEUE_JAM                 (ON)
#endif

/***[• STREAM QUEUE ***********************************************************/

#define RK_CONF_STREAM                         (ON)

#if (RK_CONF_STREAM == ON)

/*-- CONFIG: OPTIONAL FUNCTIONS  -*/
#define RK_CONF_FUNC_STREAM_JAM                (ON)
#define RK_CONF_FUNC_STREAM_PEEK               (ON)
#define RK_CONF_FUNC_STREAM_QUERY              (ON)

#endif

/***[• MOST-RECENT MESSAGE BUFFERS ********************************************/

#define RK_CONF_MRM                            (ON)

/******************************************************************************/
/********* 4. ERROR CHECKING    ***********************************************/
/******************************************************************************/
/* The kernel can return error codes (RK_CONF_ERR) plus also halting          */
/* execution (RK_CONF_FAULT) upon faulty operations request, such as a        */
/* blocking call within an ISR.                                               */
/* Note that a unsuccessful return value is not synonymous with error.        */
/* An unsuccesful 'try' post to a full RK_MBOX or a 'signal' to an empty      */
/* RK_EVENT, for instance  are well-defined operations,  that do not lead     */
/* to system failure.                                                         */
/* SUCCESSFUL operations return 0. Unsuccesful are > 0. Errors are < 0.       */

#if !defined(NDEBUG)
#define RK_CONF_ERR_CHECK                    (ON)
#if (RK_CONF_ERR_CHECK == ON)
#define RK_CONF_FAULT                        (ON)
/* print error code, line and source file on stderr */
#define RK_CONF_PRINT_ERR                    (ON)
#endif
#endif

/******************************************************************************/
/********* 5. OTHERS  *********************************************************/
/******************************************************************************/
/* This accumulates the number of ticks in the IDLE TASK on the global
idleTicks */
#define RK_CONF_IDLE_TICK_COUNT                  (ON)

#endif /* KCONFIG_H */
