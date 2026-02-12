/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.15                                              */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#include <qemu_uart.h>
/*
This file implements a simple put char (extended to put string) and use it
on the _write backend syscall so printf can be used.
For QEMU machines LM3S6965EVB (Texas Cortex M3) and 
MICROBIT (BBC Cortex-M0).
*/

#if defined(QEMU_MACHINE_MICROBIT)

static void uart_init_once(void)
{
    static unsigned char init_done;
    if (init_done)
        return;

    UART_TASKS_STOPRX = 1;
    UART_PSELTXD = MICROBIT_TX_PIN;
    UART_BAUDRATE = UART_BAUD_115200;
    UART_CONFIG = 0; 
    UART_ENABLE = UART_ENABLE_TX;

    UART_EVENTS_TXDRDY = 0;
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

int _write(int file, char const *ptr, int len)
{
    (void)file;
#if defined(QEMU_MACHINE_MICROBIT)
    /* Keep output atomic because this backend touches TX start/stop registers. */
    asm volatile("cpsid i");
#endif
    for (int i = 0; i < len; i++)
    {
        kPutc(ptr[i]);
    }
#if defined(QEMU_MACHINE_MICROBIT)
    asm volatile("cpsie i");
#endif
    return (len);
}

void kPuts(const char *str)
{
    while (*str)
    {
        kPutc(*str++);
    }
}
