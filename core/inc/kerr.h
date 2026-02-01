/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.10                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#ifndef RK_ERR_H
#define RK_ERR_H

#include <kenv.h>
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile RK_FAULT RK_gFaultID;
struct traceItem 
{
    RK_FAULT code;
    RK_TICK tick;
    UINT sp;
    CHAR* task;
    BYTE taskID;
    UINT lr;
}K_ALIGN(4);

VOID kPanic(const char* fileName, const int line,const char* fmt, ...)
__attribute__((format(printf, 3, 4)));
#define K_PANIC(...) \
do { \
    kPanic(__FILE__, __LINE__, __VA_ARGS__); \
} while(0)

__attribute__((section(".noinit")))
extern volatile struct traceItem RK_gTraceInfo;
VOID kErrHandler(RK_FAULT);

#ifdef __cplusplus
}
#endif
#endif /* RK_ERR_H*/
