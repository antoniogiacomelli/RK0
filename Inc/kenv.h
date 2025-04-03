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

#define RK_CORE_QEMU

/***************************************************
 KEEP the includes below, and add your custom 
 includes.
 **************************************************/
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
#ifdef RK_CORE_QEMU

#define RK_SYSTEMCORECLOCK (50000000)
#define RK_CORE_INIT() \
do { \
kCoreSysTickConfig( RK_CONF_TICK_PERIOD); \
RK_TICK_DIS \
kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x07); \
kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x08); \
kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x0A); \
} while(0)

#endif

#ifdef __cplusplus
}
#endif
#endif /*ENV_H*/
