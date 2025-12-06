/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.2
 * Architecture     :   ARMv6/7m
 *
 * This COMPONENT provides a simple logging interface.
 *
 ******************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#define CONF_LOGGER 1
#include <kapi.h>
#include <stdarg.h>

#if (CONF_LOGGER == 1)
#include <stdio.h>
#include <kstring.h>
#endif
VOID logInit(RK_PRIO priority);
VOID logPost(const char *fmt, ...);

#endif /* LOGGER_H */
