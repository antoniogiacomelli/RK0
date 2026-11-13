/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.92.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#include <kcommondefs.h>
#include <kconfig.h>

#ifndef CONF_LOGGER
#define CONF_LOGGER 1
#endif


#if (CONF_LOGGER == 1)
#if defined(QEMU_MACHINE_MICROBIT)
#define LOGLEN 48         /* Max length of a single log message */
#define LOGPOOLSIZ 4      /* Fixed-pool records and pointer-ring capacity */
#define LOG_STACKSIZE 160 /* Size of the stack. */
#else
#define LOGLEN 64         /* Max length of a single log message */
#define LOGPOOLSIZ 12     /* Fixed-pool records and pointer-ring capacity */
#define LOG_STACKSIZE 256 /* Size of the stack. */
#endif

/* Print "E" when the record pool is exhausted. */
#ifndef CONF_LOG_ERROR
#define CONF_LOG_ERROR (ON)
#endif
#endif

/* Available even when logging is compiled out. */
#define LOG_LEVEL_MSG           0
#define LOG_LEVEL_FAULT         1

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize once before normal logging. Producers never wait for ring space;
 * full-ring records are dropped. Task events wake the dedicated consumer.
 */
VOID logInit(RK_PRIO priority);
VOID logEnqueue(UINT level, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));

#define logPost(...)  logEnqueue(LOG_LEVEL_MSG, __VA_ARGS__)
#define logError(...) logEnqueue(LOG_LEVEL_FAULT,  __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
