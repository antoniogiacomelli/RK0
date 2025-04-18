#include <stdint.h>

#define STACK_TOP 0x20004000 /* End of RAM */

/* Forward declaration of the main function */
int main(void);

/* Forward declaration of the default handler */
void Default_Handler(void);

/* Declaration of the Reset handler */
void Reset_Handler(void);

/* Weak definition of all IRQ handlers */
void NMI_Handler(void)        __attribute__ ((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__ ((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__ ((weak, alias("Default_Handler")));

/* Vector table with the addresses of all interrupt handlers */
__attribute__ ((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    (void *)STACK_TOP,         /* Initial stack pointer */
    Reset_Handler,             /* Reset Handler */
    NMI_Handler,               /* NMI Handler */
    HardFault_Handler,         /* Hard Fault Handler */
    0,                         /* Reserved */
    0,                         /* Reserved */
    0,                         /* Reserved */
    0,                         /* Reserved */
    0,                         /* Reserved */
    0,                         /* Reserved */
    0,                         /* Reserved */
    SVC_Handler,               /* SVCall Handler */
    0,                         /* Reserved */
    0,                         /* Reserved */
    PendSV_Handler,            /* PendSV Handler */
    SysTick_Handler,           /* SysTick Handler */

    /* External Interrupts would be added here */
};

/* Default handler implementation */
void Default_Handler(void) {
    while(1);
}

/* Variables defined by the linker script */
extern uint32_t _etext;    /* End of .text section */
extern uint32_t _sdata;    /* Start of .data section */
extern uint32_t _edata;    /* End of .data section */
extern uint32_t _sbss;     /* Start of .bss section */
extern uint32_t _ebss;     /* End of .bss section */

/* Reset handler implementation */
void Reset_Handler(void) {
    uint32_t *pSrc, *pDest;

    /* Copy data section from flash to RAM */
    pSrc = &_etext;
    for(pDest = &_sdata; pDest < &_edata; ) {
        *pDest++ = *pSrc++;
    }
    
    /* Zero fill the bss section */
    for(pDest = &_sbss; pDest < &_ebss; ) {
        *pDest++ = 0;
    }
    
    /* Call the system initialization function */
    /* SystemInit(); */ /* Uncomment and implement if needed */
    
    /* Call the main function */
    main();
    
    /* If main returns, loop forever */
    while(1);
}

