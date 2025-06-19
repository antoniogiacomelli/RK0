
#include <application.h>


#if (QEMU_MACHINE == lm3s6965evb)

void kPutc(char const c) 
{
	while (UART0_FR & UART0_FR_TXFF)
        ;
    UART0_DR = c;
}

void kPuts(const char *str) 
{
    while(*str) 
    {
        kPutc(*str++);
    }
    
 }
 int _write(int file, char const *ptr, int len)
{
  (void)file;
  int DataIdx;

  kDisableIRQ();
  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    kPutc(*ptr++);
  }
  kEnableIRQ();
  return len;
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
