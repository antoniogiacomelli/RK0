/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef APPUTILS_H
#define APPUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

void kPutc(char const c);
void kPuts(const char *str);
int kTraceUartGetc(char *chPtr);
void kTraceUartRxEnable(void);

#if defined(QEMU_MACHINE_LM3S6965EVB)

#ifndef UART0_BASE
#define UART0_BASE 0x4000C000
#define UART0_DR  (*(volatile unsigned *)(UART0_BASE + 0x00)) /* Data register */
#define UART0_FR  (*(volatile unsigned *)(UART0_BASE + 0x18)) /* Fifo register */
#define UART0_IMSC (*(volatile unsigned *)(UART0_BASE + 0x38)) /* Interrupt mask */
#define UART0_ICR (*(volatile unsigned *)(UART0_BASE + 0x44)) /* Interrupt clear */
#define UART0_FR_TXFF (1U << 5)   /* FIFO Full */
#define UART0_FR_RXFE (1U << 4)   /* RX FIFO Empty */
#define UART0_RXIM (1U << 4)
#define UART0_RTIM (1U << 6)
#define UART0_RXIC (1U << 4)
#define UART0_RTIC (1U << 6)
#define UART0_IRQN (5U)
#endif

#elif defined(QEMU_MACHINE_MICROBIT)

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

#endif

#ifdef __cplusplus
}
#endif

#endif
