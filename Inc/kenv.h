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

/*******************************************
 Include libraries you might use, C standard libs,
 BSPs, etc., then set CUSTOM_ENV to (1) in
 kconfig.h.
 ******************************************/
#include <stddef.h>
#include <stdint.h>
#include <cmsis_gcc.h>
#include <khal.h>

/*********************************************************
The Kernel assumes CMSIS-like SystemCoreClock variable is
defined. This is used to configure SysTick.
Define it and then uncomment the following Macro.
- For QEMU, value is hardwired to 50MHz
******************************************************/
#define RK_SYSTEMCORECLOCK (50000000)
 

#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
