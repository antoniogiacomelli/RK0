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
 *  		It should be compatible with and M3, M4, M7 cores.
 */

#include <khal.h>
#include <kconfig.h>

unsigned kCoreSetPriorityGrouping(unsigned priorityGroup)
{
    RK_CORE_SCB->AIRCR = (0x5FAUL << 16) | (priorityGroup & 0x07UL) << 8;
    return (RK_CORE_SCB->AIRCR & (0x07UL << 8));
}

unsigned kCoreGetPriorityGrouping(void)
{
    return ((RK_CORE_SCB->AIRCR & (0x07UL << 8)) >> 8);
}

void kCoreEnableFaults(void)
{
    RK_CORE_SCB->SHCSR |= 0x00070000; /* Enable Usage, Bus, and Memory Fault */
}

void kCoreSetInterruptPriority(int IRQn, unsigned priority)
{
    if (IRQn < 0)
    {
        /* System handler priority */
        RK_CORE_SCB->SHP[(((unsigned)IRQn) & 0xF) - 4] =
            (unsigned char)((priority << 4) & 0xFF);
    }
    else
    {
        /* IRQ priority */
        RK_CORE_NVIC->IP[IRQn] = (unsigned char)((priority << 4) & 0xFF);
    }
}

unsigned kCoreGetInterruptPriority(int IRQn)
{
    if (IRQn < 0)
    {
        /* System handler priority */
        return (RK_CORE_SCB->SHP[(((unsigned)IRQn) & 0xF) - 4] >> 4);
    }
    else
    {
        /* IRQ priority */
        return (RK_CORE_NVIC->IP[IRQn] >> 4);
    }
}

void kCoreEnableInterrupt(int IRQn)
{
    if (IRQn >= 0)
    {
        RK_CORE_NVIC->ISER[IRQn >> 5] = (1UL << (IRQn & 0x1F));
    }
}

void kCoreDisableInterrupt(int IRQn)
{
    if (IRQn >= 0)
    {
        RK_CORE_NVIC->ICER[IRQn >> 5] = (1UL << (IRQn & 0x1F));
    }
}

void kCoreClearPendingInterrupt(int IRQn)
{
    if (IRQn >= 0)
    {
        RK_CORE_NVIC->ICPR[IRQn >> 5] = (1UL << (IRQn & 0x1F));
    }
}

void kCoreSetPendingInterrupt(int IRQn)
{
    if (IRQn >= 0)
    {
        RK_CORE_NVIC->ISPR[IRQn >> 5] = (1UL << (IRQn & 0x1F));
    }
}

unsigned kCoreGetPendingInterrupt(int IRQn)
{
    if (IRQn >= 0)
    {
        return (((RK_CORE_NVIC->ISPR[IRQn >> 5] & (1UL << (IRQn & 0x1F))) != 0) ? 1U : 0U);
    }
    else
    {
        return 0U;
    }
}

/*
 * SysTickCore Functions
 */
unsigned long RK_gSysCoreClock = RK_CONF_SYSCORECLK;
#ifdef RK_CONF_SYSTICK_DIV
unsigned long RK_gSyTickDiv = RK_CONF_SYSTICK_DIV;
#else
unsigned long RK_gSyTickDiv = 0;
#endif

extern unsigned long int SystemCoreClock;

unsigned kCoreSysTickConfig(unsigned ticks)
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
    RK_CORE_SYSTICK->LOAD = (ticks - 1);

    /* Reset the SysTick counter */
    RK_CORE_SYSTICK->VAL = 0;

    RK_CORE_SYSTICK->CTRL = 0x06; /* keep interrupt disabled */

#ifndef RK_CONF_SYSTICK_DIV

    RK_gSysTickInterval = (ticks * 1000UL) / (RK_gSysCoreClock);

#else

    RK_gSysTickInterval = 1000UL / RK_CONF_SYSTICK_DIV;

#endif

    return (0);
}

unsigned kCoreGetSysTickValue(void)
{
    return (RK_CORE_SYSTICK->VAL);
}

void kCoreEnableSysTick(void)
{
    RK_CORE_SYSTICK->CTRL |= 0x01;
}

void kCoreDisableSysTick(void)
{
    RK_CORE_SYSTICK->CTRL &= (unsigned)~0x01;
}

void kCoreInit(void)
{
#if (RK_CONF_SYSCORECLK == 0)

    if (RK_gSysCoreClock == 0)
    {
        kCoreSysTickConfig(SystemCoreClock / RK_CONF_SYSTICK_DIV);
    }
    else
    {
        kCoreSysTickConfig(RK_gSysCoreClock / RK_CONF_SYSTICK_DIV);
    }

#else
    kCoreSysTickConfig(RK_gSysCoreClock / RK_CONF_SYSTICK_DIV);
#endif
    kCoreSetInterruptPriority(RK_CORE_SVC_IRQN, 0x06);
    kCoreSetInterruptPriority(RK_CORE_SYSTICK_IRQN, 0x07);
    kCoreSetInterruptPriority(RK_CORE_PENDSV_IRQN, 0x07);
}