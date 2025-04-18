#include <stdint.h>

/* Forward declaration of the system exception handlers */
void Reset_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void);
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void);
void SysTick_Handler(void);

/* Forward declaration of standard peripheral interrupt handlers */
void GPIO_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UART0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UART1_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SSI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void I2C_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PWM_Handler(void) __attribute__((weak, alias("Default_Handler")));
void ADC_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* Forward declaration for the main function */
extern int main(void);

/* The Vector Table */
__attribute__ ((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = 
{
    /* Core system exceptions */
    (void *)0x20010000,          /* The initial stack pointer */
    Reset_Handler,               /* The reset handler */
    NMI_Handler,                 /* The NMI handler */
    HardFault_Handler,           /* The hard fault handler */
    MemManage_Handler,           /* The MPU fault handler */
    BusFault_Handler,            /* The bus fault handler */
    UsageFault_Handler,          /* The usage fault handler */
    0,                           /* Reserved */
    0,                           /* Reserved */
    0,                           /* Reserved */
    0,                           /* Reserved */
    SVC_Handler,                 /* SVCall handler */
    DebugMon_Handler,            /* Debug monitor handler */
    0,                           /* Reserved */
    PendSV_Handler,              /* The PendSV handler */
    SysTick_Handler,             /* The SysTick handler */

    /* External interrupts */
    GPIO_Handler,                /* GPIO */
    UART0_Handler,               /* UART0 */
    UART1_Handler,               /* UART1 */
    SSI_Handler,                 /* SSI */
    I2C_Handler,                 /* I2C */
    PWM_Handler,                 /* PWM */
    ADC_Handler,                 /* ADC */
};

/* External definitions */
extern uint32_t _sidata;     /* Start address of the initialisation values of the .data section */
extern uint32_t _sdata;      /* Start address of the .data section */
extern uint32_t _edata;      /* End address of the .data section */
extern uint32_t _sbss;       /* Start address of the .bss section */
extern uint32_t _ebss;       /* End address of the .bss section */

/**
 * @brief  System initialisation function
 */
void SystemInit(void) {
    /* RK0 will handle the system initialisation */
}

/**
 * @brief  Reset handler 
 */
void Reset_Handler(void) 
{
    uint32_t *pSrc, *pDest;

    /* Copy the data segment initialisers from flash to SRAM */
    pSrc = &_sidata;
    pDest = &_sdata;
    
    while (pDest < &_edata) 
    {
        *pDest++ = *pSrc++;
    }

    /* Zero fill the bss segment */
    for (pDest = &_sbss; pDest < &_ebss; pDest++) 
    {
        *pDest = 0;
    }

    /* Call system initialisation function */
    SystemInit();

    /* Call the application's entry point */
    main();

    /* Loop forever if main() returns */
    while (1);
}

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.
 */
void Default_Handler(void) 
{
    /* Go into an infinite loop */
    while (1);
}
