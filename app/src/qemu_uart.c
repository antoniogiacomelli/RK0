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


#include <qemu_uart.h>
#include <kapi.h>
#include <ktrace.h>
/*
This file implements a simple put char (extended to put string) and use it
on the _write backend syscall so printf can be used.
For QEMU micro:bit and STM32F030R8 board.
*/

#if ((RK_CONF_TRACE == ON) &&                                                \
     defined(QEMU_MACHINE_MICROBIT))
#define TRACE_UART_RX_BUF_SIZE 32U
#define TRACE_UART_RX_BUF_MASK (TRACE_UART_RX_BUF_SIZE - 1U)
#define NVIC_ISER0 (*(volatile unsigned long *)0xE000E100UL)
#define NVIC_ICPR0 (*(volatile unsigned long *)0xE000E280UL)

static volatile unsigned traceUartRxHead;
static volatile unsigned traceUartRxTail;
static char traceUartRxBuf[TRACE_UART_RX_BUF_SIZE];

static void traceUartRxPush_(char const ch)
{
    unsigned const head = traceUartRxHead;
    unsigned const next = (head + 1U) & TRACE_UART_RX_BUF_MASK;

    if (next != traceUartRxTail)
    {
        traceUartRxBuf[head] = ch;
        traceUartRxHead = next;
    }
}

static int traceUartRxPop_(char *const chPtr)
{
    int ret = 0;
    RK_CR_AREA
    RK_CR_ENTER

    if (traceUartRxTail != traceUartRxHead)
    {
        unsigned const tail = traceUartRxTail;
        *chPtr = traceUartRxBuf[tail];
        traceUartRxTail = (tail + 1U) & TRACE_UART_RX_BUF_MASK;
        ret = 1;
    }

    RK_CR_EXIT
    return (ret);
}

static void traceUartNvicEnable_(unsigned const irqNum)
{
    NVIC_ICPR0 = (1UL << irqNum);
    NVIC_ISER0 = (1UL << irqNum);
}
#endif

#if defined(QEMU_MACHINE_MICROBIT)

static void uart_init_once(void)
{
    static unsigned char init_done;
    if (init_done)
        return;

    UART_TASKS_STOPRX = 1;
    UART_PSELRXD = MICROBIT_RX_PIN;
    UART_PSELTXD = MICROBIT_TX_PIN;
    UART_BAUDRATE = UART_BAUD_115200;
    UART_CONFIG = 0;
    UART_ENABLE = UART_ENABLE_TXRX;

    UART_EVENTS_RXDRDY = 0;
    UART_EVENTS_TXDRDY = 0;
    UART_TASKS_STARTRX = 1;
    init_done = 1;
}
#endif

#if defined(STM32F030x8) || defined(RK_MCU_F030R8)
#define STM32F030_RCC_BASE (0x40021000UL)
#define STM32F030_RCC_AHBENR                                                \
    (*(volatile unsigned long *)(STM32F030_RCC_BASE + 0x14UL))
#define STM32F030_RCC_APB1ENR                                               \
    (*(volatile unsigned long *)(STM32F030_RCC_BASE + 0x1CUL))
#define STM32F030_RCC_AHBENR_GPIOAEN (1UL << 17U)
#define STM32F030_RCC_APB1ENR_USART2EN (1UL << 17U)

#define STM32F030_GPIOA_BASE (0x48000000UL)
#define STM32F030_GPIOA_MODER                                               \
    (*(volatile unsigned long *)(STM32F030_GPIOA_BASE + 0x00UL))
#define STM32F030_GPIOA_OSPEEDR                                             \
    (*(volatile unsigned long *)(STM32F030_GPIOA_BASE + 0x08UL))
#define STM32F030_GPIOA_PUPDR                                               \
    (*(volatile unsigned long *)(STM32F030_GPIOA_BASE + 0x0CUL))
#define STM32F030_GPIOA_AFRL                                                \
    (*(volatile unsigned long *)(STM32F030_GPIOA_BASE + 0x20UL))

#define STM32F030_USART2_BASE (0x40004400UL)
#define STM32F030_USART2_CR1                                                \
    (*(volatile unsigned long *)(STM32F030_USART2_BASE + 0x00UL))
#define STM32F030_USART2_BRR                                                \
    (*(volatile unsigned long *)(STM32F030_USART2_BASE + 0x0CUL))
#define STM32F030_USART2_ISR                                                \
    (*(volatile unsigned long *)(STM32F030_USART2_BASE + 0x1CUL))
#define STM32F030_USART2_RDR                                                \
    (*(volatile unsigned long *)(STM32F030_USART2_BASE + 0x24UL))
#define STM32F030_USART2_TDR                                                \
    (*(volatile unsigned long *)(STM32F030_USART2_BASE + 0x28UL))
#define STM32F030_USART_ISR_RXNE (1UL << 5U)
#define STM32F030_USART_ISR_TXE (1UL << 7U)
#define STM32F030_USART_CR1_UE (1UL << 0U)
#define STM32F030_USART_CR1_RE (1UL << 2U)
#define STM32F030_USART_CR1_TE (1UL << 3U)
#define STM32F030_USART2_BAUD (115200UL)

static void stm32f030r8_uart_init_once(void)
{
    static unsigned char init_done;
    volatile unsigned long fence;
    unsigned long core_clock;

    if (init_done != 0U)
    {
        return;
    }

    STM32F030_RCC_AHBENR |= STM32F030_RCC_AHBENR_GPIOAEN;
    STM32F030_RCC_APB1ENR |= STM32F030_RCC_APB1ENR_USART2EN;
    fence = STM32F030_RCC_AHBENR;
    fence = STM32F030_RCC_APB1ENR;
    (void)fence;

    /* NUCLEO-F030R8 exposes USART2 on PA2/PA3 through the ST-LINK VCP. */
    STM32F030_GPIOA_MODER &= ~((3UL << (2U * 2U)) |
                               (3UL << (3U * 2U)));
    STM32F030_GPIOA_MODER |= ((2UL << (2U * 2U)) |
                              (2UL << (3U * 2U)));
    STM32F030_GPIOA_AFRL &= ~((0xFUL << (2U * 4U)) |
                              (0xFUL << (3U * 4U)));
    STM32F030_GPIOA_AFRL |= ((1UL << (2U * 4U)) |
                             (1UL << (3U * 4U)));
    STM32F030_GPIOA_OSPEEDR |= ((3UL << (2U * 2U)) |
                                (3UL << (3U * 2U)));
    STM32F030_GPIOA_PUPDR &= ~((3UL << (2U * 2U)) |
                               (3UL << (3U * 2U)));
    STM32F030_GPIOA_PUPDR |= (1UL << (3U * 2U));

    core_clock = RK_gSysCoreClock;
    if (core_clock == 0UL)
    {
        core_clock = RK_CONF_SYSCORECLK;
    }
    if (core_clock == 0UL)
    {
        core_clock = 8000000UL;
    }

    STM32F030_USART2_CR1 = 0UL;
    STM32F030_USART2_BRR =
        (core_clock + (STM32F030_USART2_BAUD / 2UL)) /
        STM32F030_USART2_BAUD;
    STM32F030_USART2_CR1 = STM32F030_USART_CR1_UE |
                           STM32F030_USART_CR1_TE |
                           STM32F030_USART_CR1_RE;
    init_done = 1U;
}
#endif

#if defined(QEMU_MACHINE_MICROBIT)
void kPutc(char const c)
{
    uart_init_once();
    UART_TASKS_STARTTX = 1;
    UART_TXD = (unsigned long)c;
    while (UART_EVENTS_TXDRDY == 0)
        ;
    UART_EVENTS_TXDRDY = 0;
    UART_TASKS_STOPTX = 1;
}
#elif defined(STM32F030x8) || defined(RK_MCU_F030R8)
void kPutc(char const c)
{
    stm32f030r8_uart_init_once();
    while ((STM32F030_USART2_ISR & STM32F030_USART_ISR_TXE) == 0UL)
        ;
    STM32F030_USART2_TDR = (unsigned long)((unsigned char)c);
}
#else
void kPutc(char const c) { (void)c; }
#endif

#if (RK_CONF_TRACE == ON)
#if defined(QEMU_MACHINE_MICROBIT)
int kTraceUartGetc(char *chPtr)
{
    if (chPtr == 0)
    {
        return (0);
    }

    uart_init_once();
    if (traceUartRxPop_(chPtr) != 0)
    {
        return (1);
    }

    if (UART_EVENTS_RXDRDY == 0)
    {
        return (0);
    }

    *chPtr = (char)(UART_RXD & 0xFFUL);
    UART_EVENTS_RXDRDY = 0;
    return (1);
}

void kTraceUartRxEnable(void)
{
    uart_init_once();
    UART_EVENTS_RXDRDY = 0;
    UART_INTENSET = UART_INT_RXDRDY;
    traceUartNvicEnable_(UART0_IRQN);
}

void UART0_Handler(void)
{
    if (UART_EVENTS_RXDRDY != 0UL)
    {
        traceUartRxPush_((char)(UART_RXD & 0xFFUL));
        UART_EVENTS_RXDRDY = 0;
        kTraceInputSignalFromISR();
    }
}
#elif defined(STM32F030x8) || defined(RK_MCU_F030R8)
int kTraceUartGetc(char *chPtr)
{
    if (chPtr == 0)
    {
        return (0);
    }

    stm32f030r8_uart_init_once();
    if ((STM32F030_USART2_ISR & STM32F030_USART_ISR_RXNE) == 0UL)
    {
        return (0);
    }

    *chPtr = (char)(STM32F030_USART2_RDR & 0xFFUL);
    return (1);
}

void kTraceUartRxEnable(void)
{
    stm32f030r8_uart_init_once();
}
#else
int kTraceUartGetc(char *chPtr)
{
    (void)chPtr;
    return (0);
}

void kTraceUartRxEnable(void)
{
}
#endif
#endif /* RK_CONF_TRACE */

int _write(int file, char const *ptr, int len)
{
    (void)file;
    /* ! NOTE the IRQs are disabled */
    asm volatile("cpsid i");
    for (int i = 0; i < len; i++)
    {
        kPutc(ptr[i]);
    }
    asm volatile("cpsie i");
    return (len);
}

void kPuts(const char *str)
{
    while (*str)
    {
        kPutc(*str++);
    }
}
