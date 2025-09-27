/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.0
 * Architecture     :   ARMv6/7m
 *
 * This COMPONENT provides a simple logging interface.
 *
 ******************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H
#define RK_CONF_LOGGER (ON)
#if (RK_CONF_LOGGER == ON)
#include <kapi.h>
#include <stdio.h>
#include <stdarg.h>
#include <kstring.h>


VOID logInit(VOID);
/* this is not the same as kprintf in linux. */
VOID kprintf(const char *fmt, ...);
/* neither this */
VOID logPost(const char *fmt, ...);
#else
#define logInit(x) do { ; } while(0) 
#define kprintf do { ; } while(0) 
#define logPosT do { ; } while(0) 
#endif
#endif /* LOGGER_H */
