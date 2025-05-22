/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
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
volatile RK_FAULT faultID = 0;
volatile struct faultRecord faultInfo = {0};
/*police line do not cross*/
void kErrHandler( RK_FAULT fault)/* generic error handler */
{
    faultInfo.code = fault;

     if (runPtr) 
     {
        faultInfo.task = (unsigned)runPtr;
        faultInfo.sp = *((int*)runPtr); 
    } 
    else 
    {
        faultInfo.task = 0;
        faultInfo.sp = 0;
    }

    register unsigned lr_value;
    __asm volatile ("mov %0, lr" : "=r"(lr_value));
    faultInfo.lr = lr_value;
    assert(0);
}
