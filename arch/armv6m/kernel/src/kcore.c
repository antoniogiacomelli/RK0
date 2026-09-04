/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.1                                                           */
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

#if (defined(STM32F030x8) || defined(RK_MCU_F030R8)) &&                    \
    (RK_CONF_SYSCORECLK != 0UL)
#define STM32F030_FLASH_BASE (0x40022000UL)
#define STM32F030_FLASH_ACR                                                \
    (*(volatile unsigned long *)(STM32F030_FLASH_BASE + 0x00UL))
#define STM32F030_FLASH_ACR_LATENCY (1UL << 0U)
#define STM32F030_FLASH_ACR_PRFTBE (1UL << 4U)

#define STM32F030_RCC_BASE (0x40021000UL)
#define STM32F030_RCC_CR                                                  \
    (*(volatile unsigned long *)(STM32F030_RCC_BASE + 0x00UL))
#define STM32F030_RCC_CFGR                                                \
    (*(volatile unsigned long *)(STM32F030_RCC_BASE + 0x04UL))

#define STM32F030_RCC_CR_HSION (1UL << 0U)
#define STM32F030_RCC_CR_HSIRDY (1UL << 1U)
#define STM32F030_RCC_CR_PLLON (1UL << 24U)
#define STM32F030_RCC_CR_PLLRDY (1UL << 25U)

#define STM32F030_RCC_CFGR_SW_MASK (3UL << 0U)
#define STM32F030_RCC_CFGR_SW_HSI (0UL << 0U)
#define STM32F030_RCC_CFGR_SW_PLL (2UL << 0U)
#define STM32F030_RCC_CFGR_SWS_MASK (3UL << 2U)
#define STM32F030_RCC_CFGR_SWS_HSI (0UL << 2U)
#define STM32F030_RCC_CFGR_SWS_PLL (2UL << 2U)
#define STM32F030_RCC_CFGR_HPRE_MASK (0xFUL << 4U)
#define STM32F030_RCC_CFGR_PPRE_MASK (7UL << 8U)
#define STM32F030_RCC_CFGR_PLLSRC_MASK (1UL << 16U)
#define STM32F030_RCC_CFGR_PLLMUL_MASK (0xFUL << 18U)
#define STM32F030_RCC_CFGR_PLLMUL12 (10UL << 18U)

#if (RK_CONF_SYSCORECLK != 48000000UL)
#error "STM32F030R8 RK0-owned clock setup expects RK_CONF_SYSCORECLK=48000000UL"
#endif

static void kCoreBoardClockInit_(void)
{
    STM32F030_RCC_CR |= STM32F030_RCC_CR_HSION;
    while ((STM32F030_RCC_CR & STM32F030_RCC_CR_HSIRDY) == 0UL)
    {
    }

    STM32F030_FLASH_ACR |= STM32F030_FLASH_ACR_LATENCY |
                           STM32F030_FLASH_ACR_PRFTBE;

    if ((STM32F030_RCC_CFGR & STM32F030_RCC_CFGR_SWS_MASK) ==
        STM32F030_RCC_CFGR_SWS_PLL)
    {
        STM32F030_RCC_CFGR =
            (STM32F030_RCC_CFGR & ~STM32F030_RCC_CFGR_SW_MASK) |
            STM32F030_RCC_CFGR_SW_HSI;
        while ((STM32F030_RCC_CFGR & STM32F030_RCC_CFGR_SWS_MASK) !=
               STM32F030_RCC_CFGR_SWS_HSI)
        {
        }
    }

    STM32F030_RCC_CR &= ~STM32F030_RCC_CR_PLLON;
    while ((STM32F030_RCC_CR & STM32F030_RCC_CR_PLLRDY) != 0UL)
    {
    }

    STM32F030_RCC_CFGR &= ~(STM32F030_RCC_CFGR_HPRE_MASK |
                            STM32F030_RCC_CFGR_PPRE_MASK |
                            STM32F030_RCC_CFGR_PLLSRC_MASK |
                            STM32F030_RCC_CFGR_PLLMUL_MASK);
    STM32F030_RCC_CFGR |= STM32F030_RCC_CFGR_PLLMUL12;

    STM32F030_RCC_CR |= STM32F030_RCC_CR_PLLON;
    while ((STM32F030_RCC_CR & STM32F030_RCC_CR_PLLRDY) == 0UL)
    {
    }

    STM32F030_RCC_CFGR =
        (STM32F030_RCC_CFGR & ~STM32F030_RCC_CFGR_SW_MASK) |
        STM32F030_RCC_CFGR_SW_PLL;
    while ((STM32F030_RCC_CFGR & STM32F030_RCC_CFGR_SWS_MASK) !=
           STM32F030_RCC_CFGR_SWS_PLL)
    {
    }

    RK_gSysCoreClock = 48000000UL;
}
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
#if (defined(STM32F030x8) || defined(RK_MCU_F030R8)) &&                    \
    (RK_CONF_SYSCORECLK != 0UL)
    /* RK_CONF_SYSCORECLK==0 keeps the CMSIS SystemCoreClock fallback path. */
    kCoreBoardClockInit_();
#endif

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
