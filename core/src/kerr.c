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

/*******************************************************************************
 * 	COMPONENT    : ERROR CHECKER
 * 	DEPENDS ON   : LOW-LEVEL SCHEDULER, TIMER
 * 	PROVIDES TO  : ALL
 *  PUBLIC API	 : N/A
 ******************************************************************************/

 
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

_Static_assert(sizeof(VOID *) == 4, "Platform not supported");
_Static_assert(sizeof(ULONG) == sizeof(VOID *), "Platform not supported");
_Static_assert(sizeof(ULONG) == sizeof(LONG), "Platform not supported");
_Static_assert(sizeof(ULONG) == sizeof(INT), "Platform not supported");
_Static_assert(sizeof(ULONG) == sizeof(UINT), "Platform not supported");

#define RT_SILENCE_CHAR_MSG
#if (defined(__GNUC__) && !defined(RT_SILENCE_CHAR_MSG))
#if RT_CHAR_IS_SIGNED
#pragma message "*** NOTE: plain char is SIGNED ****"
#else
#pragma message "*** NOTE: plain char is UNSIGNED ***"
#endif
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
#define kprintf printf
/******************************************************************************
 * ERROR HANDLING
 ******************************************************************************/
#if (RK_CONF_FAULT == ON)
/* this works better on real hardware with a debugger  */
/* on QEMU or running with no debugger connected,      */
/* an assert(0) might be a better option               */
void abort(void)
{
    while (1)
        ;
}
volatile RK_FAULT faultID = 0;
volatile struct traceItem traceInfo = {0};

void kErrHandler(RK_FAULT fault) /* generic error handler */
{

    traceInfo.code = fault;
    faultID = fault;
    if (runPtr)
    {
        traceInfo.task = runPtr->taskName;
        traceInfo.sp = *((RK_STACK *)runPtr);
        traceInfo.taskID = (BYTE)runPtr->pid;
    }
    else
    {
        traceInfo.task = 0;
        traceInfo.sp = 0;
    }

    register unsigned lr_value;
    __asm volatile("mov %0, lr" : "=r"(lr_value));
    traceInfo.lr = lr_value;
    traceInfo.tick = kTickGet();
    #if defined(DEBUG_CONF_PRINT_ERRORS)
    kprintf("FATAL: %d\n\r", faultID);
    kprintf("TASK: %s\n\r", (traceInfo.task != 0) ? traceInfo.task : "UNKOWN");
    #endif
    abort();
}
#else
void kErrHandler(RK_FAULT fault)
{
    (void)fault;
    return;
}
#endif
