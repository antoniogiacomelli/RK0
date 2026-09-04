/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef APPUTILS_H
#define APPUTILS_H

#include <kconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

void kPutc(char const c);
void kPuts(const char *str);
#if (RK_CONF_TRACE == ON)
int kTraceUartGetc(char *chPtr);
void kTraceUartRxEnable(void);
#endif

#if defined(QEMU_MACHINE_MICROBIT)

#define NRF_UART_BASE      (0x40002000UL)
#define UART_TASKS_STARTRX (*(volatile unsigned long *)(NRF_UART_BASE + 0x000))
#define UART_TASKS_STOPRX  (*(volatile unsigned long *)(NRF_UART_BASE + 0x004))
#define UART_TASKS_STARTTX (*(volatile unsigned long *)(NRF_UART_BASE + 0x008))
#define UART_TASKS_STOPTX  (*(volatile unsigned long *)(NRF_UART_BASE + 0x00C))
#define UART_EVENTS_RXDRDY (*(volatile unsigned long *)(NRF_UART_BASE + 0x108))
#define UART_EVENTS_TXDRDY (*(volatile unsigned long *)(NRF_UART_BASE + 0x11C))
#define UART_INTENSET      (*(volatile unsigned long *)(NRF_UART_BASE + 0x304))
#define UART_INTENCLR      (*(volatile unsigned long *)(NRF_UART_BASE + 0x308))
#define UART_ENABLE        (*(volatile unsigned long *)(NRF_UART_BASE + 0x500))
#define UART_PSELRXD       (*(volatile unsigned long *)(NRF_UART_BASE + 0x514))
#define UART_PSELTXD       (*(volatile unsigned long *)(NRF_UART_BASE + 0x50C))
#define UART_RXD           (*(volatile unsigned long *)(NRF_UART_BASE + 0x518))
#define UART_BAUDRATE      (*(volatile unsigned long *)(NRF_UART_BASE + 0x524))
#define UART_CONFIG        (*(volatile unsigned long *)(NRF_UART_BASE + 0x56C))
#define UART_TXD           (*(volatile unsigned long *)(NRF_UART_BASE + 0x51C))
#define UART_ENABLE_TXRX   (4U)          /* value to enable UART */
#define UART_BAUD_115200   (0x01D7E000U) /* 115200 baud per nRF51 spec */
#define UART_INT_RXDRDY    (1UL << 2)
#define UART0_IRQN         (2U)
#define UART_PSEL_UNUSED   (0xFFFFFFFFU)
/* micro:bit v1 routes UART TX to P0.24 */
#define MICROBIT_TX_PIN (24U)
#define MICROBIT_RX_PIN (25U)

#endif /* QEMU_MACHINE_MICROBIT */

#ifdef __cplusplus
}
#endif

#endif
