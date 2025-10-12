/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
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

#if (QEMU_MACHINE == lm3s6965evb)
#ifndef NDEBUG
#define DEBUG_CONF_PRINT_ERRORS
#include <stdio.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern volatile RK_FAULT faultID;
struct traceItem 
{
    RK_FAULT code;
    RK_TICK tick;
    UINT sp;
    CHAR* task;
    BYTE taskID;
    UINT lr;
}K_ALIGN(4);

__attribute__((section(".noinit")))
extern volatile struct traceItem traceInfo;
VOID kErrHandler(RK_FAULT);
#ifdef __cplusplus
}
#endif
#endif /* RK_ERR_H*/
