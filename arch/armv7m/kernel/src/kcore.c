/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.80.1                                                         */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#include <kcoredefs.h>

#if defined(STM32F103xB) || defined(RK_MCU_F103RB)
#define RK_ARMV7M_F103RB (1U)
#else
#define RK_ARMV7M_F103RB (0U)
#endif

#if defined(QEMU_MACHINE_LM3S6965EVB)
#define RK_ARMV7M_QEMU (1U)
#else
#define RK_ARMV7M_QEMU (0U)
#endif

#if RK_ARMV7M_QEMU && (RK_CONF_SYSCORECLK == 0UL)
#error "QEMU builds cannot use RK_CONF_SYSCORECLK=0; set RK_CONF_SYSCORECLK in kconfig.h or pass QEMU_SYSCORECLK"
#endif

#if RK_ARMV7M_F103RB
#define STM32F103_HSI_HZ (8000000UL)
#define STM32F103_SYSCLK_MAX_HZ (72000000UL)
#if (RK_CONF_SYSCORECLK == 0UL)
#define RK_SYSCORECLK_EFFECTIVE STM32F103_SYSCLK_MAX_HZ
#else
#define RK_SYSCORECLK_EFFECTIVE RK_CONF_SYSCORECLK
#endif
#else
#define RK_SYSCORECLK_EFFECTIVE RK_CONF_SYSCORECLK
#endif

#if RK_ARMV7M_F103RB
unsigned long RK_gSysCoreClock = RK_SYSCORECLK_EFFECTIVE;
#elif (RK_CONF_SYSCORECLK == 0UL)
/* this is the CMSIS-Core variable for the clock freq */
extern unsigned long int SystemCoreClock;
unsigned long RK_gSysCoreClock = 0;
#else
unsigned long RK_gSysCoreClock = RK_SYSCORECLK_EFFECTIVE;
#endif

#ifdef RK_CONF_SYSTICK_DIV
unsigned long RK_gSyTickDiv = RK_CONF_SYSTICK_DIV;
#else
#error "RK_CONF_SYSTICK_DIV not defined"
#endif

#ifndef __NVIC_PRIO_BITS
#define RK_NVIC_PRIO_BITS (4U)
#else
#define RK_NVIC_PRIO_BITS (__NVIC_PRIO_BITS)
#endif

#define RK_NVIC_PRIO_ENCODE(priority)                                         \
    ((unsigned char)((((priority) << (8U - RK_NVIC_PRIO_BITS))) & 0xFFU))

#if RK_ARMV7M_F103RB
#define STM32F103_FLASH_BASE (0x40022000UL)
#define STM32F103_FLASH_ACR                                                   \
    (*(volatile unsigned long *)(STM32F103_FLASH_BASE + 0x00UL))
#define STM32F103_FLASH_ACR_LATENCY_MASK (7UL << 0U)
#define STM32F103_FLASH_ACR_LATENCY_2WS (2UL << 0U)
#define STM32F103_FLASH_ACR_PRFTBE (1UL << 4U)

#define STM32F103_RCC_BASE (0x40021000UL)
#define STM32F103_RCC_CR                                                      \
    (*(volatile unsigned long *)(STM32F103_RCC_BASE + 0x00UL))
#define STM32F103_RCC_CFGR                                                    \
    (*(volatile unsigned long *)(STM32F103_RCC_BASE + 0x04UL))

#define STM32F103_RCC_CR_HSION (1UL << 0U)
#define STM32F103_RCC_CR_HSIRDY (1UL << 1U)
#define STM32F103_RCC_CR_HSEON (1UL << 16U)
#define STM32F103_RCC_CR_HSERDY (1UL << 17U)
#define STM32F103_RCC_CR_HSEBYP (1UL << 18U)
#define STM32F103_RCC_CR_PLLON (1UL << 24U)
#define STM32F103_RCC_CR_PLLRDY (1UL << 25U)

#define STM32F103_RCC_CFGR_SW_MASK (3UL << 0U)
#define STM32F103_RCC_CFGR_SW_HSI (0UL << 0U)
#define STM32F103_RCC_CFGR_SW_HSE (1UL << 0U)
#define STM32F103_RCC_CFGR_SW_PLL (2UL << 0U)
#define STM32F103_RCC_CFGR_SWS_MASK (3UL << 2U)
#define STM32F103_RCC_CFGR_SWS_HSI (0UL << 2U)
#define STM32F103_RCC_CFGR_SWS_HSE (1UL << 2U)
#define STM32F103_RCC_CFGR_SWS_PLL (2UL << 2U)
#define STM32F103_RCC_CFGR_HPRE_MASK (0xFUL << 4U)
#define STM32F103_RCC_CFGR_PPRE1_MASK (7UL << 8U)
#define STM32F103_RCC_CFGR_PPRE1_DIV2 (4UL << 8U)
#define STM32F103_RCC_CFGR_PPRE2_MASK (7UL << 11U)
#define STM32F103_RCC_CFGR_ADCPRE_MASK (3UL << 14U)
#define STM32F103_RCC_CFGR_ADCPRE_DIV6 (2UL << 14U)
#define STM32F103_RCC_CFGR_PLLSRC_HSE (1UL << 16U)
#define STM32F103_RCC_CFGR_PLLXTPRE (1UL << 17U)
#define STM32F103_RCC_CFGR_PLLMUL_MASK (0xFUL << 18U)
#define STM32F103_RCC_CFGR_PLLMUL(mult) (((unsigned long)((mult) - 2U)) << 18U)
#ifndef RK_CONF_STM32F103_HSECLK
#define RK_CONF_STM32F103_HSECLK (8000000UL)
#endif

#ifndef RK_CONF_STM32F103_HSE_BYPASS
#define RK_CONF_STM32F103_HSE_BYPASS (ON)
#endif

#ifndef RK_CONF_STM32F103_HSE_DIV2
#define RK_CONF_STM32F103_HSE_DIV2 (OFF)
#endif

#if !RK_CONFIG_BOOL_VALID(RK_CONF_STM32F103_HSE_BYPASS)
#error "RK_CONF_STM32F103_HSE_BYPASS must be ON or OFF"
#endif

#if !RK_CONFIG_BOOL_VALID(RK_CONF_STM32F103_HSE_DIV2)
#error "RK_CONF_STM32F103_HSE_DIV2 must be ON or OFF"
#endif

#if (RK_SYSCORECLK_EFFECTIVE > STM32F103_SYSCLK_MAX_HZ)
#error "STM32F103 SYSCLK must not exceed 72000000UL"
#endif

#if (RK_CONF_STM32F103_HSECLK == 0UL)
#error "RK_CONF_STM32F103_HSECLK must be greater than zero"
#endif

#define RK_STM32F103_USE_HSI                                                  \
    (RK_SYSCORECLK_EFFECTIVE == STM32F103_HSI_HZ)
#define RK_STM32F103_USE_HSE                                                  \
    ((RK_SYSCORECLK_EFFECTIVE == RK_CONF_STM32F103_HSECLK) &&                 \
     !RK_STM32F103_USE_HSI)
#define RK_STM32F103_USE_PLL                                                  \
    (!RK_STM32F103_USE_HSI && !RK_STM32F103_USE_HSE)

#if (RK_CONF_STM32F103_HSE_DIV2 == ON)
#define RK_STM32F103_PLL_INPUT_HZ (RK_CONF_STM32F103_HSECLK / 2UL)
#else
#define RK_STM32F103_PLL_INPUT_HZ (RK_CONF_STM32F103_HSECLK)
#endif

#if RK_STM32F103_USE_PLL
#if (RK_STM32F103_PLL_INPUT_HZ == 0UL)
#error "STM32F103 PLL input must be greater than zero"
#endif
#if ((RK_SYSCORECLK_EFFECTIVE % RK_STM32F103_PLL_INPUT_HZ) != 0UL)
#error "STM32F103 cannot derive RK_CONF_SYSCORECLK exactly from configured PLL input"
#endif
#define RK_STM32F103_PLLMUL                                                   \
    (RK_SYSCORECLK_EFFECTIVE / RK_STM32F103_PLL_INPUT_HZ)
#if (RK_STM32F103_PLLMUL < 2UL) || (RK_STM32F103_PLLMUL > 16UL)
#error "STM32F103 derived PLL multiplier must be in the x2..x16 range"
#endif
#endif

static void kCoreBoardClockInit_(void)
{
    STM32F103_RCC_CR |= STM32F103_RCC_CR_HSION;
    while ((STM32F103_RCC_CR & STM32F103_RCC_CR_HSIRDY) == 0UL)
    {
    }

    if ((STM32F103_RCC_CFGR & STM32F103_RCC_CFGR_SWS_MASK) !=
        STM32F103_RCC_CFGR_SWS_HSI)
    {
        STM32F103_RCC_CFGR =
            (STM32F103_RCC_CFGR & ~STM32F103_RCC_CFGR_SW_MASK) |
            STM32F103_RCC_CFGR_SW_HSI;
        while ((STM32F103_RCC_CFGR & STM32F103_RCC_CFGR_SWS_MASK) !=
               STM32F103_RCC_CFGR_SWS_HSI)
        {
        }
    }

    STM32F103_RCC_CR &= ~STM32F103_RCC_CR_PLLON;
    while ((STM32F103_RCC_CR & STM32F103_RCC_CR_PLLRDY) != 0UL)
    {
    }

#if RK_STM32F103_USE_HSI
    STM32F103_RCC_CR &= ~STM32F103_RCC_CR_HSEON;
    while ((STM32F103_RCC_CR & STM32F103_RCC_CR_HSERDY) != 0UL)
    {
    }
#else

#if (RK_CONF_STM32F103_HSE_BYPASS == ON)
    STM32F103_RCC_CR |= STM32F103_RCC_CR_HSEBYP;
#else
    STM32F103_RCC_CR &= ~STM32F103_RCC_CR_HSEBYP;
#endif

    STM32F103_RCC_CR |= STM32F103_RCC_CR_HSEON;
    while ((STM32F103_RCC_CR & STM32F103_RCC_CR_HSERDY) == 0UL)
    {
    }
#endif

    STM32F103_FLASH_ACR =
        (STM32F103_FLASH_ACR & ~STM32F103_FLASH_ACR_LATENCY_MASK) |
        STM32F103_FLASH_ACR_PRFTBE | STM32F103_FLASH_ACR_LATENCY_2WS;

    STM32F103_RCC_CFGR &=
        ~(STM32F103_RCC_CFGR_HPRE_MASK | STM32F103_RCC_CFGR_PPRE1_MASK |
          STM32F103_RCC_CFGR_PPRE2_MASK | STM32F103_RCC_CFGR_ADCPRE_MASK |
          STM32F103_RCC_CFGR_PLLSRC_HSE | STM32F103_RCC_CFGR_PLLXTPRE |
          STM32F103_RCC_CFGR_PLLMUL_MASK | STM32F103_RCC_CFGR_SW_MASK);

    STM32F103_RCC_CFGR |= STM32F103_RCC_CFGR_ADCPRE_DIV6;

#if (RK_SYSCORECLK_EFFECTIVE > 36000000UL)
    STM32F103_RCC_CFGR |= STM32F103_RCC_CFGR_PPRE1_DIV2;
#endif

#if RK_STM32F103_USE_HSE
    STM32F103_RCC_CFGR =
        (STM32F103_RCC_CFGR & ~STM32F103_RCC_CFGR_SW_MASK) |
        STM32F103_RCC_CFGR_SW_HSE;
    while ((STM32F103_RCC_CFGR & STM32F103_RCC_CFGR_SWS_MASK) !=
           STM32F103_RCC_CFGR_SWS_HSE)
    {
    }
#elif RK_STM32F103_USE_PLL
    STM32F103_RCC_CFGR |= STM32F103_RCC_CFGR_PLLSRC_HSE |
                          STM32F103_RCC_CFGR_PLLMUL(RK_STM32F103_PLLMUL);

#if (RK_CONF_STM32F103_HSE_DIV2 == ON)
    STM32F103_RCC_CFGR |= STM32F103_RCC_CFGR_PLLXTPRE;
#endif

    STM32F103_RCC_CR |= STM32F103_RCC_CR_PLLON;
    while ((STM32F103_RCC_CR & STM32F103_RCC_CR_PLLRDY) == 0UL)
    {
    }

    STM32F103_RCC_CFGR =
        (STM32F103_RCC_CFGR & ~STM32F103_RCC_CFGR_SW_MASK) |
        STM32F103_RCC_CFGR_SW_PLL;
    while ((STM32F103_RCC_CFGR & STM32F103_RCC_CFGR_SWS_MASK) !=
           STM32F103_RCC_CFGR_SWS_PLL)
    {
    }
#else
    while ((STM32F103_RCC_CFGR & STM32F103_RCC_CFGR_SWS_MASK) !=
           STM32F103_RCC_CFGR_SWS_HSI)
    {
    }
#endif

    RK_gSysCoreClock = RK_SYSCORECLK_EFFECTIVE;
}
#endif

static inline unsigned kCoreSysTickConfig_(unsigned ticks)
{
    /* CheckCore if number of ticks is valid */
    if ((ticks - 1) > 0xFFFFFFUL) /*24-bit max*/
    {
        return (0xFFFFFFFF);
    }
#if (RK_CONF_SYSCORECLK == 0UL) && !RK_ARMV7M_F103RB

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


#define SCB_SHP          (volatile unsigned char*)(0xE000ED18)
#define NVIC_IP          (volatile unsigned char*)(0xE000E400)

static inline
void kCoreSetInterruptPriority_(int IRQn, unsigned priority)
{
    if (IRQn < 0)
    {
        unsigned long offset = ((unsigned long)IRQn & 0xF) - 4;
        /* System handler priority */
        *(SCB_SHP + offset) = RK_NVIC_PRIO_ENCODE(priority);
    }
    else
    {
        unsigned long offset = (unsigned long)IRQn;

        /* IRQ priority */
        *(NVIC_IP + offset) = RK_NVIC_PRIO_ENCODE(priority);
    }
}


void kCoreInit(void)
{
#if RK_ARMV7M_F103RB
    kCoreBoardClockInit_();
#endif

    kCoreSysTickConfig_(RK_gSysCoreClock / RK_CONF_SYSTICK_DIV);
    kCoreSetInterruptPriority_(RK_CORE_SVC_IRQN, 0x05);
    kCoreSetInterruptPriority_(RK_CORE_SYSTICK_IRQN, 0x06);
    kCoreSetInterruptPriority_(RK_CORE_PENDSV_IRQN, 0x07);
}
