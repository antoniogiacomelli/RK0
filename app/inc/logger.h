/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
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
