/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.83.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#include <kcoredefs.h>

#if defined(STM32F030x8) || defined(RK_MCU_F030R8)
#include <kf030r8.h>
#define RK_ARMV6M_F030R8 (1U)
#else
#define RK_ARMV6M_F030R8 (0U)
#endif

#if defined(QEMU_MACHINE_MICROBIT) && (RK_CONF_SYSCORECLK == 0)
#error "QEMU builds cannot use RK_CONF_SYSCORECLK=0; set RK_CONF_SYSCORECLK in kconfig.h or pass QEMU_SYSCORECLK"
#endif

#if RK_ARMV6M_F030R8
#if (RK_CONF_SYSCORECLK == 0)
#define RK_SYSCORECLK_EFFECTIVE K_F030R8_SYSCLK_HZ
#else
#define RK_SYSCORECLK_EFFECTIVE RK_CONF_SYSCORECLK
#endif
unsigned long RK_gSysCoreClock = RK_SYSCORECLK_EFFECTIVE;
#elif (RK_CONF_SYSCORECLK == 0)
/* CMSIS-Core exports SystemCoreClock when RK_CONF_SYSCORECLK is zero */
extern unsigned long int SystemCoreClock;
unsigned long RK_gSysCoreClock = 0;
#else
unsigned long RK_gSysCoreClock = RK_CONF_SYSCORECLK;
#endif

#if RK_ARMV6M_F030R8
#if (RK_SYSCORECLK_EFFECTIVE != K_F030R8_HSI_HZ) &&                         \
    (RK_SYSCORECLK_EFFECTIVE != K_F030R8_SYSCLK_HZ)
#error "STM32F030R8 supports RK_CONF_SYSCORECLK=0UL, 8000000UL, or 48000000UL"
#endif

static void kCoreBoardClockInit_(void)
{
    K_F030R8_RCC_CR |= K_F030R8_RCC_CR_HSION;
    while ((K_F030R8_RCC_CR & K_F030R8_RCC_CR_HSIRDY) == 0UL)
    {
    }

    K_F030R8_RCC_CFGR =
        (K_F030R8_RCC_CFGR & ~K_F030R8_RCC_CFGR_SW_MASK) |
        K_F030R8_RCC_CFGR_SW_HSI;
    while ((K_F030R8_RCC_CFGR & K_F030R8_RCC_CFGR_SWS_MASK) !=
           K_F030R8_RCC_CFGR_SWS_HSI)
    {
    }

    K_F030R8_RCC_CR &= ~K_F030R8_RCC_CR_PLLON;
    while ((K_F030R8_RCC_CR & K_F030R8_RCC_CR_PLLRDY) != 0UL)
    {
    }

    K_F030R8_RCC_CFGR &=
        ~(K_F030R8_RCC_CFGR_HPRE_MASK | K_F030R8_RCC_CFGR_PPRE_MASK |
          K_F030R8_RCC_CFGR_PLLSRC | K_F030R8_RCC_CFGR_PLLMUL_MASK);

#if (RK_SYSCORECLK_EFFECTIVE == K_F030R8_SYSCLK_HZ)
    K_F030R8_FLASH_ACR =
        K_F030R8_FLASH_ACR_PRFTBE | K_F030R8_FLASH_ACR_LATENCY;
    K_F030R8_RCC_CFGR |= K_F030R8_RCC_CFGR_PLLMUL(12UL);
    K_F030R8_RCC_CR |= K_F030R8_RCC_CR_PLLON;
    while ((K_F030R8_RCC_CR & K_F030R8_RCC_CR_PLLRDY) == 0UL)
    {
    }
    K_F030R8_RCC_CFGR =
        (K_F030R8_RCC_CFGR & ~K_F030R8_RCC_CFGR_SW_MASK) |
        K_F030R8_RCC_CFGR_SW_PLL;
    while ((K_F030R8_RCC_CFGR & K_F030R8_RCC_CFGR_SWS_MASK) !=
           K_F030R8_RCC_CFGR_SWS_PLL)
    {
    }
#else
    K_F030R8_FLASH_ACR = K_F030R8_FLASH_ACR_PRFTBE;
#endif
}
#endif

#ifndef RK_CONF_SYSTICK_DIV
#error "SYSTICK INTERVAL NOT DEFINED"
#else
unsigned long RK_gSyTickDiv = RK_CONF_SYSTICK_DIV;
#endif

static inline unsigned kCoreSysTickConfig_(unsigned ticks)
{
    /* check if number of ticks is valid (24-bit reload) */
    if ((ticks - 1U) > 0xFFFFFFUL)
    {
        return (0xFFFFFFFF);
    }

#if (RK_CONF_SYSCORECLK == 0) && !RK_ARMV6M_F030R8
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
#if RK_ARMV6M_F030R8
    kCoreBoardClockInit_();
#endif
    unsigned long refClk =
#if (RK_CONF_SYSCORECLK == 0) && !RK_ARMV6M_F030R8
        (RK_gSysCoreClock ? RK_gSysCoreClock : SystemCoreClock);
#else
        RK_gSysCoreClock;
#endif

    kCoreSysTickConfig_(refClk / RK_CONF_SYSTICK_DIV);
    kCoreSetInterruptPriority_(RK_CORE_SVC_IRQN, 0x01);
    kCoreSetInterruptPriority_(RK_CORE_SYSTICK_IRQN, 0x02);
    kCoreSetInterruptPriority_(RK_CORE_PENDSV_IRQN, 0x03);
}
