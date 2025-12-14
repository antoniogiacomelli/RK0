/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.1
 * Architecture     :   ARMv6/7m
 *
 * This COMPONENT provides a simple logging interface.
 *
 ******************************************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include <kcommondefs.h>   

#define CONF_LOGGER 1


#ifdef __cplusplus
extern "C" {
#endif

VOID logInit(RK_PRIO priority);
VOID logPost(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */