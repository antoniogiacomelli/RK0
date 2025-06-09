/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
 * Architecture     :   ARMv7m
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

#include <kdefs.h>

#define ON     (1)
#define OFF    (0)

/******************************************************************************/
/********* 1. TASKS AND SCHEDULER *********************************************/
/******************************************************************************/

/*** [ • SYSTEM TASKS STACK SIZE (WORDS) **************************************/

/*
 * This configuration is exposed so the system programmer might adjust
 * the IdleTask stack size to support any hook. The Timer Handler stack size
 * must be adjusted to support Application Timers callouts.
 * (1 Word = 4 bytes)
 *
 * (!) Keep it aligned to a double-word (8-byte) boundary.
 **/

#define RK_CONF_IDLE_STACKSIZE      	    	(64) /* Words */
#define RK_CONF_TIMHANDLER_STACKSIZE  		    (64) /* Words */

/***[• USER-DEFINED TASKS (NUMBER) ********************************************/

#define RK_CONF_N_USRTASKS    	            (1)

/***[• MINIMAL EFFECTIVE PRIORITY (HIGHEST PRIORITY NUMBER)  ******************/

#define RK_CONF_MIN_PRIO	           	    (1)

/******************************************************************************/
/********* 2. APPLICATION TIMER  **********************************************/
/******************************************************************************/
#define RK_CONF_CALLOUT_TIMER				(ON)


/******************************************************************************/
/********* 3. SYNCHRONISATION  ************************************************/
/******************************************************************************/
                                                    
/***[• COUNTER SEMAPHORES *****************************************************/

#define RK_CONF_SEMA                         (ON)


/***[• MUTEX SEMAPHORES *******************************************************/

#define RK_CONF_MUTEX                        (ON)

/***[• GENERIC EVENTS (SLEEP/WAKE/SIGNAL) *************************************/

#define RK_CONF_EVENT                        (ON)

/******************************************************************************/
/********* 3. MESSAGE-PASSING  ************************************************/
/******************************************************************************/

/***[• MAILBOX ****************************************************************/

#define RK_CONF_MBOX	       	             (ON)

#if(RK_CONF_MBOX==ON)

/*-- CONFIG: OPTIONAL FUNCTIONS       -*/
#define RK_CONF_FUNC_MBOX_QUERY  		     (ON)
#define RK_CONF_FUNC_MBOX_PEEK		         (ON)
#define RK_CONF_FUNC_MBOX_POSTOVW		     (ON)

#endif

/***[• MAIL QUEUE  ************************************************************/

#define RK_CONF_QUEUE					     (ON)

#if(RK_CONF_QUEUE==ON)

/*-- CONFIG: OPTIONAL FUNCTIONS  -*/
#define RK_CONF_FUNC_QUEUE_PEEK			     (ON)
#define RK_CONF_FUNC_QUEUE_QUERY	       	 (ON)
#define RK_CONF_FUNC_QUEUE_JAM			     (ON)
#endif

/***[• STREAM QUEUE ***********************************************************/

#define RK_CONF_STREAM			   	         (ON)

#if (RK_CONF_STREAM == ON)

/*-- CONFIG: OPTIONAL FUNCTIONS  -*/
#define RK_CONF_FUNC_STREAM_JAM			     (ON)
#define RK_CONF_FUNC_STREAM_PEEK			 (ON)
#define RK_CONF_FUNC_STREAM_QUERY	    	 (ON)

#endif

/***[• MOST-RECENT MESSAGE BUFFERS ********************************************/

#define RK_CONF_MRM                          (ON)


/******************************************************************************/
/********* 4. OTHERS  *********************************************************/
/******************************************************************************/

/* Count the accumulated number of ticks within the IdleTask */
/* This is useful to compare with the total number of ticks  */
/* since the scheduler has tarted */
#define RK_CONF_IDLE_TICK_COUNT              (ON)

/* Check for fatal errors. If disabled, the kernel calls will not */
/* verify if inputs are sane. Useful to disable after */
/* the system has been tested, as it saves ROM */

#define RK_CONF_FAULT_CHECK                  (ON)


#endif /* KCONFIG_H */
