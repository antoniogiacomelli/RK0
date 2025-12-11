
#include <application.h>
/*
This file implements a simple put char (extended to put string) and use it
on the _write backend syscall so printf can be used.

*/

#ifdef QEMU_MACHINE_LM3S6965EVB

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

int _write(int file, char const *ptr, int len)
{
    (void)file;
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++)
    {
        kPutc(*ptr++);
    }
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
