/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.1
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>
 *
 *
 ******************************************************************************/

/**
 * @file khal.c
 * @brief 	 This is intended as a minimal CPU HAL for ARMv7M
 * 			 handling NVIC, SCB, SysTick.
 *  		 It should be compatible with and M3, M4, M7 cores.
 *           You can map it to any CMSIS-Core.
 */

#include <kdefs.h>

unsigned long RK_gSysCoreClock = RK_CONF_SYSCORECLK;
#ifdef RK_CONF_SYSTICK_DIV
unsigned long RK_gSyTickDiv = RK_CONF_SYSTICK_DIV;
#else
unsigned long RK_gSyTickDiv = 0;
#endif

extern unsigned long int SystemCoreClock;

static inline unsigned kCoreSysTickConfig_(unsigned ticks)
{
    /* CheckCore if number of ticks is valid */
    if ((ticks - 1) > 0xFFFFFFUL) /*24-bit max*/
    {
        return (0xFFFFFFFF);
    }
#if (RK_CONF_SYSCORECLK == 0)

    if (RK_gSysCoreClock == 0)
        RK_gSysCoreClock = SystemCoreClock;

#endif
    /* Set reload register */
    RK_REG_SYSTICK_LOAD = (ticks - 1);

    /* Reset the SysTick counter */
    RK_REG_SYSTICK_VAL = 0;

    RK_REG_SYSTICK_CTRL = 0x06; /* keep interrupt disabled */

#ifndef RK_CONF_SYSTICK_DIV

    RK_gSysTickInterval = (ticks * 1000UL) / (RK_gSysCoreClock);

#else

    RK_gSysTickInterval = 1000UL / RK_CONF_SYSTICK_DIV;

#endif

    return (0);
}

 
#define SCB_SHP          (volatile unsigned long*)(0xE000ED18)
#define NVIC_IP          (volatile unsigned long*)(0xE000E400)

static inline
void kCoreSetInterruptPriority_(int IRQn, unsigned priority)
{
    if (IRQn < 0)
    {
        unsigned long offset = ((unsigned long)IRQn & 0xF) - 4;
        /* System handler priority */
        *(SCB_SHP + offset) =
            (unsigned char)((priority << 4) & 0xFF);
    }
    else
    {
         unsigned long offset = ((unsigned long)IRQn & 0xF) - 4;

        /* IRQ priority */
        *(NVIC_IP + offset)  = (unsigned char)((priority << 4) & 0xFF);
    }
}


void kCoreInit(void)
{
#if (RK_CONF_SYSCORECLK == 0)

    if (RK_gSysCoreClock == 0)
    {
        kCoreSysTickConfig_(SystemCoreClock / RK_CONF_SYSTICK_DIV);
    }
    else
    {
        kCoreSysTickConfig_(RK_gSysCoreClock / RK_CONF_SYSTICK_DIV);
    }

#else
    kCoreSysTickConfig_(RK_gSysCoreClock / RK_CONF_SYSTICK_DIV);
#endif
    kCoreSetInterruptPriority_(RK_CORE_SVC_IRQN, 0x05);
    kCoreSetInterruptPriority_(RK_CORE_SYSTICK_IRQN, 0x06);
    kCoreSetInterruptPriority_(RK_CORE_PENDSV_IRQN, 0x07);
}