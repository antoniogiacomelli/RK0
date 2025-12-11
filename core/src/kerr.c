/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.4                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/*******************************************************************************
 * 	COMPONENT    : ERROR CHECKER
 * 	DEPENDS ON   : LOW-LEVEL SCHEDULER, TIMER
 * 	PROVIDES TO  : ALL
 *  PUBLIC API	 : N/A
 ******************************************************************************/

 #define RK_SOURCE_CODE

#include <kerr.h>

/*** Compile time errors ****/
#if defined(__ARM_ARCH_7EM__) /* Cortex-M4 / M7 */
#define ARCH_CM_7EM 1
#elif defined(__ARM_ARCH_7M__) /* Cortex-M3       */
#define ARCH_CM_7M 1
#elif defined(__ARM_ARCH_6M__) /* Cortex-M0/M0+/ */
#define ARCH_CM_6M 1
#else
#error "Unsupported Cortex-M architecture—check your -mcpu/-march"
#endif

#ifndef __GNUC__
#error "You need GCC as your compiler!"
#endif

#ifndef __CMSIS_GCC_H
#error "You need CMSIS-GCC !"
#endif

#ifndef RK_CONF_MINIMAL_VER
#error "Missing RK0 version"
#endif

#if (RK_CONF_MIN_PRIO > 31)
#error "Invalid minimal effective priority. (Max numerical value: 31)"
#endif

#if defined(QEMU_MACHINE)
#if (RK_CONF_SYSCORECLK == 0UL)
#error "Invalid RK_CONF_SYSCORECLK for QEMU. Can't be 0."
#endif
#endif

/******************************************************************************
 * ERROR HANDLING
 ******************************************************************************/
#if (RK_CONF_FAULT == ON)
volatile RK_FAULT RK_gFaultID = 0;
volatile struct traceItem RK_gTraceInfo = {0};

void kErrHandler(RK_FAULT fault) /* generic error handler */
{

    RK_gTraceInfo.code = fault;
    RK_gFaultID = fault;
    if (RK_gRunPtr)
    {
        RK_gTraceInfo.task = RK_gRunPtr->taskName;
        RK_gTraceInfo.sp = (UINT)RK_gRunPtr->sp;
        RK_gTraceInfo.taskID = (BYTE)RK_gRunPtr->pid;
    }
    else
    {
        RK_gTraceInfo.task = 0;
        RK_gTraceInfo.sp = 0;
    }

    register unsigned lr_value;
    __asm volatile("mov %0, lr" : "=r"(lr_value));
    RK_gTraceInfo.lr = lr_value;
    RK_gTraceInfo.tick = kTickGet();
    #if (DEBUG_CONF_PRINT_ERRORS == 1)
    RK_CR_AREA
    RK_CR_ENTER
    printf("FATAL: %04x\n\r", RK_gFaultID);
    printf("TASK: %s\n\r", (RK_gTraceInfo.task != 0) ? RK_gTraceInfo.task : "UNKOWN");
    RK_CR_EXIT
    #endif
    RK_ABORT
}
#else
void kErrHandler(RK_FAULT fault)
{
    (void)fault;
    return;
}
#endif
