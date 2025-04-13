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

#include <stm32f4xx.h>
#include <khal.h>
#include <stdio.h>
#include <stddef.h>
#include <cmsis_gcc.h>


#ifdef __CMSIS_VERSION_H
#define RK_SYSTEMCORECLOCK (SystemCoreClock)
#endif

#define RK_CONF_PRINTF

#ifdef RK_CONF_PRINTF
#define RK_CONF_PRINTF
#ifdef RK_CONF_PRINTF
#define kprintf(...) \
            do \
            { \
                __disable_irq();\
                printf(__VA_ARGS__); \
                __enable_irq(); \
            } while(0U)

#else
#define kprintf(...) (void)0
#endif

#endif

#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
