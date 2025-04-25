#pragma GCC diagnostic ignored "-Wpedantic" 

/* startup_microbit.c – C startup + vector table for micro:bit (nRF51822) */

#include <stdint.h>

/* Linker-generated symbols */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
void SystemInit(void);
int  main(void);

/* Default handler: loops forever */
void Default_Handler(void) { while (1) {} }

/* Core exception handlers */
void Reset_Handler(void);
void NMI_Handler         (void) __attribute__ ((weak, alias("Default_Handler")));
void HardFault_Handler   (void) __attribute__ ((weak, alias("Default_Handler")));
void SVC_Handler         (void) __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler      (void) __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));

/* Peripheral IRQ handlers (weak aliases) */
void POWER_CLOCK_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void RADIO_IRQHandler        (void) __attribute__ ((weak, alias("Default_Handler")));
void UART0_IRQHandler        (void) __attribute__ ((weak, alias("Default_Handler")));
void SPI0_TWI0_IRQHandler    (void) __attribute__ ((weak, alias("Default_Handler")));
void SPI1_TWI1_IRQHandler    (void) __attribute__ ((weak, alias("Default_Handler")));
void GPIOTE_IRQHandler       (void) __attribute__ ((weak, alias("Default_Handler")));
void ADC_IRQHandler          (void) __attribute__ ((weak, alias("Default_Handler")));
void TIMER0_IRQHandler       (void) __attribute__ ((weak, alias("Default_Handler")));
void TIMER1_IRQHandler       (void) __attribute__ ((weak, alias("Default_Handler")));
void TIMER2_IRQHandler       (void) __attribute__ ((weak, alias("Default_Handler")));

/* Vector table placed at start of FLASH */
__attribute__ ((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack), /* Initial SP */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0, /* MemManage */
    0, /* BusFault */
    0, /* UsageFault */
    0, /* Reserved */
    0, /* Reserved */
    0, /* Reserved */
    0, /* Reserved */
    SVC_Handler,
    0, /* DebugMon */
    0, /* Reserved */
    PendSV_Handler,
    SysTick_Handler,
    /* Peripheral interrupts */
    POWER_CLOCK_IRQHandler,
    RADIO_IRQHandler,
    UART0_IRQHandler,
    SPI0_TWI0_IRQHandler,
    SPI1_TWI1_IRQHandler,
    GPIOTE_IRQHandler,
    ADC_IRQHandler,
    TIMER0_IRQHandler,
    TIMER1_IRQHandler,
    TIMER2_IRQHandler
};

/* Reset handler: copy data, zero bss, call SystemInit() and main() */
void Reset_Handler(void)
{
    uint32_t *src, *dst;

    /* Copy .data from flash to RAM */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero-fill .bss */
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }


    /* Enter application */
    main();

    /* If main() returns, loop forever */
    while (1) {}
}
