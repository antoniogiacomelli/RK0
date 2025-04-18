#include <application.h>

/* keep stacks double-word aligned for ARMv7M */
INT stack1[STACKSIZE] __attribute__((aligned(8))); 
INT stack2[STACKSIZE] __attribute__((aligned(8)));
INT stack3[STACKSIZE] __attribute__((aligned(8)));

#define UART0_BASE      0x40002000
#define UART0_PSELTXD   (*(volatile uint32_t *)(UART0_BASE + 0x50C))
#define UART0_ENABLE    (*(volatile uint32_t *)(UART0_BASE + 0x500))
#define UART0_BAUDRATE  (*(volatile uint32_t *)(UART0_BASE + 0x524))
#define UART0_STARTTX   (*(volatile uint32_t *)(UART0_BASE + 0x000))

void uart_init(void) {
    UART0_PSELTXD = 24;             // GPIO pin 24 (matches micro:bit TX pin in QEMU)
    UART0_BAUDRATE = 0x01D7E000;    // 115200 baud
    UART0_ENABLE = 4;               // Enable UART (value = 0x4 per spec)
    UART0_STARTTX = 1;              // Start transmission
}
void uart_putchar(char c) {
    volatile uint32_t *TXD = (volatile uint32_t *)(UART0_BASE + 0x508);
    *TXD = c;
}

void kPuts(const char *s) {
    while (*s) uart_putchar(*s++);
}

RK_MBOX mbox1;
RK_MBOX mbox2;


VOID kApplicationInit(VOID)
{

    kMboxInit(&mbox1, NULL);
    kMboxInit(&mbox2, NULL);
}

VOID Task1(VOID* args)
{
    RK_UNUSEARGS
    UINT *recvPtr = NULL;
	while (1)
	{
        kPuts("Task 1 is running...\n\r");
		kMboxPend(&mbox1, (VOID**)(&recvPtr), RK_WAIT_FOREVER);
                
	}
}
VOID Task2(VOID* args)
{
    RK_UNUSEARGS
    UINT mesg = 0xAABBCCD;
	while (1)
	{
        kPuts("Task 2 is running...\n\r");
		kMboxPost(&mbox1, &mesg, RK_WAIT_FOREVER);

    }
}
VOID Task3(VOID* args)
{
    RK_UNUSEARGS
	while (1)
	{
        kPuts("Task 3 running...\n\r");
        kYield();
    }
}
