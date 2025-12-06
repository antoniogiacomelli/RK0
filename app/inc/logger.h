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
#define CONF_LOGGER (ON)
#if (CONF_LOGGER == ON)
#include <kapi.h>
#include <stdio.h>
#include <stdarg.h>
#include <kstring.h>


VOID logInit(RK_PRIO priority);
VOID logPost(const char *fmt, ...);
#else
#define logInit(x) do { ; } while(0) 
#define logPost do { ; } while(0) 
#endif
#endif /* LOGGER_H */
