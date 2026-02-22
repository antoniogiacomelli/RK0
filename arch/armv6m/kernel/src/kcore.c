/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#include <kcoredefs.h>

#if (RK_CONF_SYSCORECLK == 0)
/* CMSIS-Core exports SystemCoreClock when RK_CONF_SYSCORECLK is zero */
extern unsigned long int SystemCoreClock;
unsigned long RK_gSysCoreClock = 0;
#else
unsigned long RK_gSysCoreClock = RK_CONF_SYSCORECLK;
#endif

#ifdef RK_CONF_SYSTICK_DIV
unsigned long RK_gSyTickDiv = RK_CONF_SYSTICK_DIV;
#else
unsigned long RK_gSyTickDiv = 0;
#endif

static inline unsigned kCoreSysTickConfig_(unsigned ticks)
{
    /* check if number of ticks is valid (24-bit reload) */
    if ((ticks - 1U) > 0xFFFFFFUL)
    {
        return (0xFFFFFFFF);
    }

#if (RK_CONF_SYSCORECLK == 0)
    if (RK_gSysCoreClock == 0)
    {
        RK_gSysCoreClock = SystemCoreClock;
    }
#endif

    /* Set reload register */
    RK_REG_SYSTICK_LOAD = (ticks - 1U);
    /* Reset the SysTick counter */
    RK_REG_SYSTICK_VAL = 0;
    /* keep interrupt disabled; clock source = core */
    RK_REG_SYSTICK_CTRL = 0x06;

#ifndef RK_CONF_SYSTICK_DIV
    RK_gSysTickInterval = (ticks * 1000UL) / (RK_gSysCoreClock);
#else
    RK_gSysTickInterval = 1000UL / RK_CONF_SYSTICK_DIV;
#endif

    return (0);
}

static inline void kCoreSetInterruptPriority_(int IRQn, unsigned priority)
{
    /* ARMv6-M supports 4 priority levels (bits 7:6) */
    unsigned char prio = (unsigned char)((priority & 0x3U) << 6);

    if (IRQn < 0)
    {
        /* system handler priorities start at SCB->SHP[0] (offset 0x18) */
        volatile unsigned char *shp = (volatile unsigned char *)(RK_CORE_SCB_BASE + 0x18);
        unsigned offset = (((unsigned)IRQn) & 0xFU) - 4U;
        shp[offset] = prio;
    }
    else if (IRQn < 32)
    {
        /* external interrupts: NVIC IP bytes start at 0xE000E400 */
        volatile unsigned char *ip = (volatile unsigned char *)(RK_CORE_NVIC_BASE + 0x300);
        ip[IRQn] = prio;
    }
}

void kCoreInit(void)
{
    unsigned long refClk =
#if (RK_CONF_SYSCORECLK == 0)
        (RK_gSysCoreClock ? RK_gSysCoreClock : SystemCoreClock);
#else
        RK_gSysCoreClock;
#endif

    kCoreSysTickConfig_(refClk / RK_CONF_SYSTICK_DIV);
    kCoreSetInterruptPriority_(RK_CORE_SVC_IRQN, 0x01);
    kCoreSetInterruptPriority_(RK_CORE_SYSTICK_IRQN, 0x02);
    kCoreSetInterruptPriority_(RK_CORE_PENDSV_IRQN, 0x03);
}
