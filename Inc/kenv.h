/******************************************************************************
 *
 * RK0 - Real-Time Kernel '0'
 * Version  :   V0.4.0
 * Target   :   ARMv7m
 *
 * Copyright (c) 2025 Antonio Giacomelli
 ******************************************************************************/

#ifndef RK_ENV_H
#define RK_ENV_H

#ifdef __cplusplus
extern "C" {
#endif

/* include CMSIS-Core, CMSIS-GCC 
and any other libraries you might use
such as HALs - then set CUSTOM_ENV to (1) in
kconfig.h. */
#include <khal.h>
#include <stddef.h>
#include <stdint.h>
#include <cmsis_gcc.h>
 
 #define RK_SYSTEMCORECLOCK
 

#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
