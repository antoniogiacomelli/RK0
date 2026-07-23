/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.0                                                           */
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
MICROBIT (BBC Cortex-M0).
*/

#define TRACE_UART_RX_BUF_SIZE 64U
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
#else
void kPutc(char const c) { (void)c; }
#endif

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
