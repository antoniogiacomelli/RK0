/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.81.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_F401RE_H
#define RK_F401RE_H

#include <kcommondefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K_F401RE_REG32(addr) (*(volatile ULONG *)(addr))

#define K_F401RE_RCC_BASE (0x40023800UL)
#define K_F401RE_RCC_CR K_F401RE_REG32(K_F401RE_RCC_BASE + 0x00UL)
#define K_F401RE_RCC_PLLCFGR K_F401RE_REG32(K_F401RE_RCC_BASE + 0x04UL)
#define K_F401RE_RCC_CFGR K_F401RE_REG32(K_F401RE_RCC_BASE + 0x08UL)
#define K_F401RE_RCC_AHB1ENR K_F401RE_REG32(K_F401RE_RCC_BASE + 0x30UL)
#define K_F401RE_RCC_APB1ENR K_F401RE_REG32(K_F401RE_RCC_BASE + 0x40UL)

#define K_F401RE_RCC_CR_HSION (1UL << 0U)
#define K_F401RE_RCC_CR_HSIRDY (1UL << 1U)
#define K_F401RE_RCC_CR_PLLON (1UL << 24U)
#define K_F401RE_RCC_CR_PLLRDY (1UL << 25U)

#define K_F401RE_RCC_CFGR_SW_MASK (3UL << 0U)
#define K_F401RE_RCC_CFGR_SW_HSI (0UL << 0U)
#define K_F401RE_RCC_CFGR_SW_PLL (2UL << 0U)
#define K_F401RE_RCC_CFGR_SWS_MASK (3UL << 2U)
#define K_F401RE_RCC_CFGR_SWS_HSI (0UL << 2U)
#define K_F401RE_RCC_CFGR_SWS_PLL (2UL << 2U)
#define K_F401RE_RCC_CFGR_HPRE_MASK (0xFUL << 4U)
#define K_F401RE_RCC_CFGR_PPRE1_SHIFT (10U)
#define K_F401RE_RCC_CFGR_PPRE1_MASK \
    (7UL << K_F401RE_RCC_CFGR_PPRE1_SHIFT)
#define K_F401RE_RCC_CFGR_PPRE1_DIV2 \
    (4UL << K_F401RE_RCC_CFGR_PPRE1_SHIFT)
#define K_F401RE_RCC_CFGR_PPRE2_MASK (7UL << 13U)

#define K_F401RE_RCC_PLLCFGR_PLLM(value) ((ULONG)(value) << 0U)
#define K_F401RE_RCC_PLLCFGR_PLLN(value) ((ULONG)(value) << 6U)
#define K_F401RE_RCC_PLLCFGR_PLLP_DIV4 (1UL << 16U)
#define K_F401RE_RCC_PLLCFGR_PLLQ(value) ((ULONG)(value) << 24U)

#define K_F401RE_RCC_AHB1ENR_GPIOAEN (1UL << 0U)
#define K_F401RE_RCC_APB1ENR_USART2EN (1UL << 17U)

#define K_F401RE_FLASH_ACR K_F401RE_REG32(0x40023C00UL)
#define K_F401RE_FLASH_ACR_LATENCY_MASK (7UL)
#define K_F401RE_FLASH_ACR_LATENCY_2WS (2UL)
#define K_F401RE_FLASH_ACR_PRFTEN (1UL << 8U)
#define K_F401RE_FLASH_ACR_ICEN (1UL << 9U)
#define K_F401RE_FLASH_ACR_DCEN (1UL << 10U)

#define K_F401RE_GPIOA_BASE (0x40020000UL)
#define K_F401RE_GPIOA_MODER K_F401RE_REG32(K_F401RE_GPIOA_BASE + 0x00UL)
#define K_F401RE_GPIOA_OTYPER K_F401RE_REG32(K_F401RE_GPIOA_BASE + 0x04UL)
#define K_F401RE_GPIOA_OSPEEDR K_F401RE_REG32(K_F401RE_GPIOA_BASE + 0x08UL)
#define K_F401RE_GPIOA_PUPDR K_F401RE_REG32(K_F401RE_GPIOA_BASE + 0x0CUL)
#define K_F401RE_GPIOA_AFRL K_F401RE_REG32(K_F401RE_GPIOA_BASE + 0x20UL)

#define K_F401RE_USART2_BASE (0x40004400UL)
#define K_F401RE_USART2_SR K_F401RE_REG32(K_F401RE_USART2_BASE + 0x00UL)
#define K_F401RE_USART2_DR K_F401RE_REG32(K_F401RE_USART2_BASE + 0x04UL)
#define K_F401RE_USART2_BRR K_F401RE_REG32(K_F401RE_USART2_BASE + 0x08UL)
#define K_F401RE_USART2_CR1 K_F401RE_REG32(K_F401RE_USART2_BASE + 0x0CUL)
#define K_F401RE_USART2_SR_RXNE (1UL << 5U)
#define K_F401RE_USART2_SR_TXE (1UL << 7U)
#define K_F401RE_USART2_CR1_UE (1UL << 13U)
#define K_F401RE_USART2_CR1_TE (1UL << 3U)
#define K_F401RE_USART2_CR1_RE (1UL << 2U)

#define K_F401RE_HSI_HZ (16000000UL)
#define K_F401RE_SYSCLK_HZ (80000000UL)
#define K_F401RE_USART2_BAUD (115200UL)
#define K_F401RE_USART2_IRQN (38UL)

#ifdef __cplusplus
}
#endif

#endif /* RK_F401RE_H */
