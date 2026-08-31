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
For QEMU machines LM3S6965EVB (Texas Cortex M3) and
MICROBIT (BBC Cortex-M0) and for STM32F103RB board
*/

#if ((RK_CONF_TRACE == ON) && \
     (defined(QEMU_MACHINE_LM3S6965EVB) || defined(QEMU_MACHINE_MICROBIT)))
#if defined(QEMU_MACHINE_MICROBIT)
#define TRACE_UART_RX_BUF_SIZE 32U
#else
#define TRACE_UART_RX_BUF_SIZE 64U
#endif
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

#if defined(STM32F103xB) || defined(RK_MCU_F103RB)
#define STM32F103_USART2_BASE (0x40004400UL)
#define STM32F103_USART2_SR (*(volatile unsigned long *)(STM32F103_USART2_BASE + 0x00UL))
#define STM32F103_USART2_DR (*(volatile unsigned long *)(STM32F103_USART2_BASE + 0x04UL))
#define STM32F103_USART2_BRR (*(volatile unsigned long *)(STM32F103_USART2_BASE + 0x08UL))
#define STM32F103_USART2_CR1 (*(volatile unsigned long *)(STM32F103_USART2_BASE + 0x0CUL))
#define STM32F103_USART_SR_RXNE (1UL << 5)
#define STM32F103_USART_SR_TXE (1UL << 7)
#define STM32F103_USART_CR1_RE (1UL << 2)
#define STM32F103_USART_CR1_TE (1UL << 3)
#define STM32F103_USART_CR1_UE (1UL << 13)
#define STM32F103_USART2_BAUD (115200UL)

#define STM32F103_RCC_BASE (0x40021000UL)
#define STM32F103_RCC_APB2ENR (*(volatile unsigned long *)(STM32F103_RCC_BASE + 0x18UL))
#define STM32F103_RCC_APB1ENR (*(volatile unsigned long *)(STM32F103_RCC_BASE + 0x1CUL))
#define STM32F103_RCC_APB2ENR_AFIOEN (1UL << 0)
#define STM32F103_RCC_APB2ENR_IOPAEN (1UL << 2)
#define STM32F103_RCC_APB1ENR_USART2EN (1UL << 17)

#define STM32F103_GPIOA_BASE (0x40010800UL)
#define STM32F103_GPIOA_CRL (*(volatile unsigned long *)(STM32F103_GPIOA_BASE + 0x00UL))
#define STM32F103_GPIOA_ODR (*(volatile unsigned long *)(STM32F103_GPIOA_BASE + 0x0CUL))

static void stm32f103rb_uart_init_once(void)
{
    static unsigned char init_done;
    volatile unsigned long fence;
    unsigned long core_clock;

    if (init_done)
    {
        return;
    }

    STM32F103_RCC_APB2ENR |= STM32F103_RCC_APB2ENR_AFIOEN |
                             STM32F103_RCC_APB2ENR_IOPAEN;
    STM32F103_RCC_APB1ENR |= STM32F103_RCC_APB1ENR_USART2EN;
    fence = STM32F103_RCC_APB2ENR;
    fence = STM32F103_RCC_APB1ENR;
    (void)fence;

    /* NUCLEO-F103RB exposes USART2 on PA2/PA3 through the ST-LINK VCP. */
    STM32F103_GPIOA_CRL &= ~((0xFUL << (2U * 4U)) |
                             (0xFUL << (3U * 4U)));
    STM32F103_GPIOA_CRL |= ((0xBUL << (2U * 4U)) |  /* PA2: AF push-pull */
                            (0x4UL << (3U * 4U)));  /* PA3: floating input */
    STM32F103_GPIOA_ODR |= (1UL << 3U);

    core_clock = RK_gSysCoreClock;
    if (core_clock == 0UL)
    {
        core_clock = RK_CONF_SYSCORECLK;
    }
    if (core_clock == 0UL)
    {
        core_clock = 8000000UL;
    }

    STM32F103_USART2_CR1 = 0UL;
    STM32F103_USART2_BRR =
        (core_clock + (STM32F103_USART2_BAUD / 2UL)) /
        STM32F103_USART2_BAUD;
    STM32F103_USART2_CR1 = STM32F103_USART_CR1_UE |
                           STM32F103_USART_CR1_TE |
                           STM32F103_USART_CR1_RE;
    init_done = 1U;
}
#endif

#if defined(QEMU_MACHINE_LM3S6965EVB)
void kPutc(char const c)
{
    while (UART0_FR & UART0_FR_TXFF)
        ;
    UART0_DR = c;
}
#elif defined(QEMU_MACHINE_MICROBIT)
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
#elif defined(STM32F103xB) || defined(RK_MCU_F103RB)
void kPutc(char const c)
{
    stm32f103rb_uart_init_once();
    while ((STM32F103_USART2_SR & STM32F103_USART_SR_TXE) == 0UL)
        ;
    STM32F103_USART2_DR = (unsigned long)((unsigned char)c);
}
#else
void kPutc(char const c) { (void)c; }
#endif

#if (RK_CONF_TRACE == ON)
#if defined(QEMU_MACHINE_LM3S6965EVB)
int kTraceUartGetc(char *chPtr)
{
    if (chPtr == 0)
    {
        return (0);
    }

    if (traceUartRxPop_(chPtr) != 0)
    {
        return (1);
    }

    if (UART0_FR & UART0_FR_RXFE)
    {
        return (0);
    }

    *chPtr = (char)(UART0_DR & 0xFFU);
    return (1);
}

void kTraceUartRxEnable(void)
{
    UART0_ICR = UART0_RXIC | UART0_RTIC;
    UART0_IMSC |= UART0_RXIM | UART0_RTIM;
    traceUartNvicEnable_(UART0_IRQN);
}

void UART0_Handler(void)
{
    while ((UART0_FR & UART0_FR_RXFE) == 0U)
    {
        traceUartRxPush_((char)(UART0_DR & 0xFFU));
    }

    UART0_ICR = UART0_RXIC | UART0_RTIC;
    kTraceInputSignalFromISR();
}
#elif defined(QEMU_MACHINE_MICROBIT)
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
#elif defined(STM32F103xB) || defined(RK_MCU_F103RB)
int kTraceUartGetc(char *chPtr)
{
    if (chPtr == 0)
    {
        return (0);
    }

    stm32f103rb_uart_init_once();
    if ((STM32F103_USART2_SR & STM32F103_USART_SR_RXNE) == 0UL)
    {
        return (0);
    }

    *chPtr = (char)(STM32F103_USART2_DR & 0xFFUL);
    return (1);
}

void kTraceUartRxEnable(void)
{
    stm32f103rb_uart_init_once();
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
