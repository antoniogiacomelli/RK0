/******************************************************************************
 *
 * RK0 - Real-Time Kernel '0'
 * Version  :   V0.4.0
 * Arch     :   ARMv6/7-M
 *
 * Copyright (c) 2025 Antonio Giacomelli
 ******************************************************************************/

/******************************************************************************
 *
 * 	Module          : Compile Time Checker / Error Handler
 * 	Depends on      : Environment
 * 	Provides to     : All
 *  Public API      : No
 *
 ******************************************************************************/

#include "kexecutive.h"

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

#ifndef RK_SYSTEMCORECLOCK
#	error	"Undefined SystemCoreClock global value."
#endif

/******************************************************************************
 * ERROR HANDLING
 ******************************************************************************/
volatile RK_FAULT faultID = 0;

/*police line do not cross*/
void kErrHandler( RK_FAULT fault)/* generic error handler */
{
/*TODO: before using   */
/*#ifdef NDEBUG, guarantee these faults are returning correctly  */
    faultID = fault;
    asm volatile ("cpsid i" : : : "memory");
    while (1)
        ;
}
