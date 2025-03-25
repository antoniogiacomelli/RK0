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

/* Include HAL, C libraries and
   GCC dependencies here
   then set K_CUSTOM_ENV to (1) in
   kconfig.h
 

 #define RK_SYSTEMCORECLOCK (SystemCoreClock)

 */


#define RK_CONF_PRINTF

#ifdef RK_CONF_PRINTF
/* extern data, as peripheral handlers declarations, etc*/
extern UART_HandleTypeDef huart2;
#endif

#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
