
#include <application.h>
/*
This file implements a simple put char (extended to put string) and use it
on the _write backend syscall so printf can be used. Note this implementation
is assuming the QEMU machine (Texas stellaris cortex m3) register map.
*/

#if (QEMU_MACHINE == lm3s6965evb)

void kPutc(char const c)
{
    while (UART0_FR & UART0_FR_TXFF)
        ;
    UART0_DR = c;
}

void kPuts(const char *str)
{
    while (*str)
    {
        kPutc(*str++);
    }
}

/* !! NOTE THE IRQ DISABLE IN _WRITE !! */

int _write(int file, char const *ptr, int len)
{
    RK_CR_AREA
    RK_CR_ENTER
    (void)file;
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++)
    {
        kPutc(*ptr++);
    }
    RK_CR_EXIT
    return (len);
}
#else
void kPutc(char const c)
{
    (void)c;
    return;
}

void kPuts(const char *str)
{
    (void)str;
    return;
}
#endif
