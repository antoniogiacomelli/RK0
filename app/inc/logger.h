/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.14
 * Architecture     :   ARMv6/7m
 *
 * This COMPONENT provides a simple logging interface.
 *
 * Usage:
 *  
 *  logPost(formatted string as for a printf);
 *      Prints timestamp and message.
 * 
 *  logError(formated string as for printf); 
 *      Prints timestamp and messsage. Abort execution.
 * 
 *  If calling logError when there are no available buffers in
 *  for printing the execution will be aborted with a standard
 *  message.
 * 
 * The logger facility is designed as an Active Object using a Message Queue
 * and a Partition Pool. 
 * 
 * The standard log structure has three fields:
 * 
 * { time, string, level };
 *
 * You can refactor it as for your needs.
 * 
 * The logger task must be set as the lowest priority user task. 
 * 
 * It blocks waiting for messages, and when has a chance to run dequeues 
 * as many buffers as it can returning it to the partition pool.
 * 
 * a logPost is NON BLOCKING. It will not block waiting for a buffer.
 * Not enough buffers can have many symptons the most common is missed prints
 * what can be hard to detect if messages are all equal but changing only a 
 * parameter. 
 * 
 * 
 *
 ******************************************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include <kcommondefs.h>
#include <kconfig.h>

#define CONF_LOGGER 1 /* Turn logger on/off */

#if (CONF_LOGGER == 1)
#define LOGLEN 64    /* Max length of a single log message */
#define LOGPOOLSIZ 16 /* Number of log message buffers  */


#define LOG_STACKSIZE 256 /* Size of the stack. */

/* used by logPost and logError */
#define LOG_LEVEL_MSG           0
#define LOG_LEVEL_FAULT         1


#if (RK_CONF_MESG_QUEUE == OFF)
#error "Need RK_CONF_MESG_QUEUE enabled for logger facility"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

VOID logInit(RK_PRIO priority);
VOID logEnqueue(UINT level, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));

#define logPost(...)  logEnqueue(LOG_LEVEL_MSG, __VA_ARGS__)
#define logError(...) logEnqueue(LOG_LEVEL_FAULT,  __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
