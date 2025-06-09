/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 	Module          : Compile Time Checker / Error Handler
 * 	Depends on      : Environment
 * 	Provides to     : All
 *  Public API      : No
 *
 ******************************************************************************/

#include "kservices.h"

/*** Compile time errors */
#if   defined(__ARM_ARCH_7EM__)        /* Cortex-M4 / M7 */
#  define ARCH_CM_7EM   1
#elif defined(__ARM_ARCH_7M__)         /* Cortex-M3       */
#  define ARCH_CM_7M    1
#elif defined(__ARM_ARCH_6M__)         /* Cortex-M0/M0+/ */
#  define ARCH_CM_6M    1
#else
#  error "Unsupported Cortex-M architecture—check your -mcpu/-march"
#endif

#ifndef __GNUC__
#   error "You need GCC as your compiler!"
#endif

#ifndef __CMSIS_GCC_H
#   error "You need CMSIS-GCC !"
#endif

#ifndef RK_CONF_MINIMAL_VER
#error "Missing RK0 version"
#endif

#if (RK_CONF_MIN_PRIO > 31)
#	error "Invalid minimal effective priority. (Max numerical value: 31)"
#endif

/******************************************************************************
 * ERROR HANDLING
 ******************************************************************************/

void abort(void)
{
    __disable_irq();
    while(1)
    ;
}
 volatile RK_FAULT faultID = 0;
volatile struct traceItem traceInfo = {0};
/*police line do not cross*/
void kErrHandler( RK_FAULT fault)/* generic error handler */
{
    traceInfo.code = fault;
    faultID = fault;
     if (runPtr) 
     {
        traceInfo.task = runPtr->taskName;
        traceInfo.sp = *((int*)runPtr); 
        traceInfo.taskID = (BYTE) runPtr->pid;
    } 
    else 
    {
        traceInfo.task = 0;
        traceInfo.sp = 0;
    }

    register unsigned lr_value;
    __asm volatile ("mov %0, lr" : "=r"(lr_value));
    traceInfo.lr = lr_value;
    traceInfo.tick = kTickGet();
    assert(0);
}

