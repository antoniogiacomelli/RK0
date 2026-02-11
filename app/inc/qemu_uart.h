#ifndef APPUTILS_H
#define APPUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

void kPutc(char const c);
void kPuts(const char *str);

#if defined(QEMU_MACHINE_LM3S6965EVB)

#ifndef UART0_BASE
#define UART0_BASE 0x4000C000
#define UART0_DR  (*(volatile unsigned *)(UART0_BASE + 0x00)) /* Data register */
#define UART0_FR  (*(volatile unsigned *)(UART0_BASE + 0x18)) /* Fifo register */
#define UART0_FR_TXFF (1U << 5)   /* FIFO Full */
#endif

#elif defined(QEMU_MACHINE_MICROBIT)

#define NRF_UART_BASE      (0x40002000UL)
#define UART_TASKS_STARTRX (*(volatile unsigned long *)(NRF_UART_BASE + 0x000))
#define UART_TASKS_STOPRX  (*(volatile unsigned long *)(NRF_UART_BASE + 0x004))
#define UART_TASKS_STARTTX (*(volatile unsigned long *)(NRF_UART_BASE + 0x008))
#define UART_TASKS_STOPTX  (*(volatile unsigned long *)(NRF_UART_BASE + 0x00C))
#define UART_EVENTS_TXDRDY (*(volatile unsigned long *)(NRF_UART_BASE + 0x11C))
#define UART_ENABLE        (*(volatile unsigned long *)(NRF_UART_BASE + 0x500))
#define UART_PSELTXD       (*(volatile unsigned long *)(NRF_UART_BASE + 0x50C))
#define UART_BAUDRATE      (*(volatile unsigned long *)(NRF_UART_BASE + 0x524))
#define UART_CONFIG        (*(volatile unsigned long *)(NRF_UART_BASE + 0x56C))
#define UART_TXD           (*(volatile unsigned long *)(NRF_UART_BASE + 0x51C))
#define UART_ENABLE_TX     (4U)          /* value to enable UART */
#define UART_BAUD_115200   (0x01D7E000U) /* 115200 baud per nRF51 spec */
#define UART_PSEL_UNUSED   (0xFFFFFFFFU)
/* micro:bit v1 routes UART TX to P0.24 */
#define MICROBIT_TX_PIN (24U)

#endif

#ifdef __cplusplus
}
#endif

#endif
