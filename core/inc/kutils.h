#ifndef K_UTILS_H
#define K_UTILS_H

#if (QEMU_MACHINE == lm3s6965evb)
#include <stdio.h>
#ifndef UART0_BASE
#define UART0_BASE 0x4000C000
#define UART0_DR  (*(volatile unsigned *)(UART0_BASE + 0x00)) /* Data register */
#define UART0_FR  (*(volatile unsigned *)(UART0_BASE + 0x18)) /* Fifo register */
#define UART0_FR_TXFF (1U << 5)   /* FIFO Full */
#endif

#endif

void kPutc(char const c);
void kPuts(const char *str);

#endif