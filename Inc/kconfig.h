/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version              :   V0.4.0
 * Architecture         :   ARMv7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 * www.kernel0.org
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  •••••••••••••••••• KERNEL CONFIGURATION OPTIONS •••••••••••••••••
 *
 *  ••• Critical Definitions •••
 *
 *  •   Number of User-defined tasks
 *  •   System Tick Interval
 *  •   Minimal effective priority (maximum number)
 *
 *
 ******************************************************************************/

#ifndef RK_CONFIG_H
#define RK_CONFIG_H
#include <kdefs.h>
/******************************************************************************/

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

/*** [ • SYSTEM TICK PERIOD (PARAMETER) ***************************************/

/*
 * You can configure the TICK period by defining RK_CONF_TICK_PERIOD to
 * (SystemCoreClock/N) where  where 1/N is the frequency for a given
 * period: e.g., for 1ms -> N=1000, 5ms -> N=200.
 *
 **/

#define RK_CONF_TICK_PERIOD  (RK_TICK_5MS) /* PRE-DEFINED OPTIONS: */
                                                // RK_TICK_1MS 
                                                // RK_TICK_5MS 
                                                // RK_TICK_10MS
                                             

/***[• USER-DEFINED TASKS (NUMBER) ********************************************/

#define RK_CONF_N_USRTASKS    	            (3)

/***[• MINIMAL EFFECTIVE PRIORITY (HIGHEST PRIORITY NUMBER)  ******************/

#define RK_CONF_MIN_PRIO	           	    (1)

/***[• TIME-SLICE SCHEDULING (ON/OFF) *****************************************/

#define RK_CONF_SCH_TSLICE			        (OFF)


/******************************************************************************/
/********* 2. APPLICATION TIMER  **********************************************/
/******************************************************************************/
#define RK_CONF_CALLOUT_TIMER				(OFF)


/******************************************************************************/
/********* 3. SYNCHRONISATION  ************************************************/
/******************************************************************************/
                                                    
/***[• TASK PRIVATE BINARY SEMAPHORE ******************************************/

#define RK_CONF_BIN_SEMA                     (ON)

/***[• COUNTER SEMAPHORES *****************************************************/

#define RK_CONF_SEMA                         (OFF)

#if (RK_CONF_SEMA==ON)

#endif

/***[• MUTEX SEMAPHORES *******************************************************/

#define RK_CONF_MUTEX                        (OFF)

#if (RK_CONF_MUTEX==ON)

/*-- CONFIG: PRIORITY INHERITANCE     -*/

#define RK_CONF_MUTEX_PRIO_INH		         (OFF)

#endif

/***[• GENERIC EVENTS (SLEEP/WAKE/SIGNAL) *************************************/

#define RK_CONF_EVENT                        (OFF)


/******************************************************************************/
/********* 3. MESSAGE-PASSING  ************************************************/
/******************************************************************************/

/***[• MAILBOX ****************************************************************/

#define RK_CONF_MBOX	       	             (ON)

#if(RK_CONF_MBOX==ON)

/*-- CONFIG: OPTIONAL FUNCTIONS       -*/
#define RK_CONF_FUNC_MBOX_ISFULL		     (ON)
#define RK_CONF_FUNC_MBOX_PEEK		         (ON)
#define RK_CONF_FUNC_MBOX_POSTOVW		     (ON)

#endif

/***[• MAIL QUEUE  ************************************************************/

#define RK_CONF_QUEUE					     (OFF)

#if(RK_CONF_QUEUE==ON)

/*-- CONFIG: OPTIONAL FUNCTIONS  -*/
#define RK_CONF_FUNC_QUEUE_ISFULL			 (ON)
#define RK_CONF_FUNC_QUEUE_PEEK			     (ON)
#define RK_CONF_FUNC_QUEUE_QUERY	       	 (ON)
#define RK_CONF_FUNC_QUEUE_JAM			     (ON)
#endif

/***[• STREAM QUEUE ***********************************************************/

#define RK_CONF_STREAM			   	         (OFF)

#if (RK_CONF_STREAM == ON)

/*-- CONFIG: OPTIONAL FUNCTIONS  -*/
#define RK_CONF_FUNC_STREAM_JAM			     (ON)
#define RK_CONF_FUNC_STREAM_PEEK			 (ON)
#define RK_CONF_FUNC_STREAM_QUERY	    	 (ON)
#define RK_CONF_FUNC_STREAM_RESET			 (ON)

#endif

/***[• MOST-RECENT MESSAGE BUFFERS ********************************************/

#define RK_CONF_MRM                          (OFF)


/******************************************************************************/
/********* 4. OTHERS  *********************************************************/
/******************************************************************************/

#define RK_CONF_IDLE_TICK_COUNT              (ON)

#endif /* KCONFIG_H */
