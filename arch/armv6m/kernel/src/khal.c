
/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.10                                               */
/** ARCHITECTURE     :   ARMv6m                                               */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/ /*
  This is intended as a minimal CPU HAL for ARMv6-M (Cortex-M0)
  handling NVIC, SCB, SysTick.
  */

#include <khal.h>
#include <kconfig.h>

unsigned kCoreSetPriorityGrouping(unsigned priorityGroup)
{
    /* M0 doesn't support priority grouping, ignore parameter */
    (void)priorityGroup;
    return (1);
}

unsigned kCoreGetPriorityGrouping(void)
{
    /* N/S */
    return (1);
}

void kCoreEnableFaults(void)
{
    /*N/S*/
}

void kCoreSetInterruptPriority(int IRQn, unsigned priority)
{
    /* supports 4 priority levels (2 bits) */

    priority = (priority & 0x3) << 6;

    if (IRQn < 0)
    {
        {
            RK_CORE_SCB->SHP[(((unsigned)IRQn) & 0xF) - 4] =
                (unsigned char)priority;
        }
    }
    else
    {
        /* 32 external interrupts */
        if (IRQn < 32)
        {
            RK_CORE_NVIC->IP[IRQn] = (unsigned char)priority;
        }
    }
}

unsigned kCoreGetInterruptPriority(int IRQn)
{
    if (IRQn < 0)
    {
        /* System handler priority */
        if (IRQn == RK_CORE_SVC_IRQN || IRQn == RK_CORE_PENDSV_IRQN || IRQn == RK_CORE_SYSTICK_IRQN)
        {
            return (RK_CORE_SCB->SHP[(((unsigned)IRQn) & 0xF) - 4] >> 6);
        }
        return (0);
    }
    else
    {
        /* IRQ priority */
        if (IRQn < 32)
        {
            return (RK_CORE_NVIC->IP[IRQn] >> 6);
        }
        return (0);
    }
}

void kCoreEnableInterrupt(int IRQn)
{
    if (IRQn >= 0 && IRQn < 32)
    {
        RK_CORE_NVIC->ISER[0] = (1UL << IRQn);
    }
}

void kCoreDisableInterrupt(int IRQn)
{
    if (IRQn >= 0 && IRQn < 32)
    {
        RK_CORE_NVIC->ICER[0] = (1UL << IRQn);
    }
}

void kCoreClearPendingInterrupt(int IRQn)
{
    if (IRQn >= 0 && IRQn < 32)
    {
        RK_CORE_NVIC->ICPR[0] = (1UL << IRQn);
    }
}

void kCoreSetPendingInterrupt(int IRQn)
{
    if (IRQn >= 0 && IRQn < 32)
    {
        RK_CORE_NVIC->ISPR[0] = (1UL << IRQn);
    }
}

unsigned kCoreGetPendingInterrupt(int IRQn)
{
    if (IRQn >= 0 && IRQn < 32)
    {
        return (((RK_CORE_NVIC->ISPR[0] & (1UL << IRQn)) != 0) ? 1U : 0U);
    }
    else
    {
        return (0U);
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
    /*  check if number of ticks is valid */
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
    kCoreSetInterruptPriority(RK_CORE_SVC_IRQN, 0x01);
    kCoreSetInterruptPriority(RK_CORE_SYSTICK_IRQN, 0x02);
    kCoreSetInterruptPriority(RK_CORE_PENDSV_IRQN, 0x03);
}
