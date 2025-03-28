/******************************************************************************
 *
 * RK0 - Real-Time Kernel '0'
 * Version  :   V0.4.0
 * Target   :   ARMv7m
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

#if !(CUSTOM_ENV)
#error "You need to custom your application environment and change CUSTOM_ENV\
         to (1)"
#endif

#ifndef __GNUC__
#   error "You need GCC as your compiler!"
#endif

#ifndef RK_CONF_MINIMAL_VER
#error "Missing K0BA version"
#endif

#if (RK_CONF_MIN_PRIO > 31)
#	error "Invalid minimal effective priority. (Max numerical value: 31)"
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
