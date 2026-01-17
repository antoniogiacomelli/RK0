/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.5
 * Architecture     :   ARMv6/7m
 *
 * This COMPONENT provides a simple logging interface.
 *
 * 
 * The logger facility is an Active Object
 * It blocks on a queue waiting for data to print
 * Every logPost is non-blocking send of data to the
 * event queue of the logger task.
 * It grabs a buffer from the pool, writes and enqueue without blocking
 * Logger task runs at lowest priority
 * It blocks waiting for messages and when gets to run will dequeues
 * as much messages it can until blocking or being preempted.
 * 
 ******************************************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include <kcommondefs.h>   

#define CONF_LOGGER 1 /* Turn logger on/off */
#define LOGLEN 64    /* Max length of a single log message */
#define LOGPOOLSIZ 16 /* Number of log message buffers  */
                      /* a low number can cause messages that 
                         are missed, information that seems wrong
                         when messages are equal but changes only
                         a field... */

#define LOG_STACKSIZE 256 /* Size of the stack. Remember this is a printf.*/


#ifdef __cplusplus
extern "C" {
#endif

VOID logInit(RK_PRIO priority);
VOID logPost(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */