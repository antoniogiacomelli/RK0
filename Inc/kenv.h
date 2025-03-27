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

#include <stm32f4xx_hal.h>
#include <cmsis_gcc.h>
#include <stdio.h>
#include <string.h>
#define RK_SYSTEMCORECLOCK (SystemCoreClock)

#define RK_CONF_PRINTF

#ifdef RK_CONF_PRINTF
/* extern data, as peripheral handlers declarations, etc*/
extern UART_HandleTypeDef huart2;
#endif

#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
