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


#include <khal.h>
#include <stdio.h>
#include <stm32f0xx_hal.h>
#include <cmsis_gcc.h>
#include <string.h>

#ifdef __CMSIS_VERSION_H
#define RK_SYSTEMCORECLOCK (SystemCoreClock)
#endif


#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
