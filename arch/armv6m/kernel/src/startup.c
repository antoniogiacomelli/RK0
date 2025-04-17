#include <stdint.h>

/* Symbols defined in linker script */
extern uint32_t _sidata; // Flash address for .data section
extern uint32_t _sdata;  // Start of .data in RAM
extern uint32_t _edata;  // End of .data in RAM
extern uint32_t _sbss;   // Start of .bss
extern uint32_t _ebss;   // End of .bss
extern uint32_t _estack; // Top of stack

extern int main(void);
void Reset_Handler(void);

/* Optional user-provided hooks */
void SystemInit(void)        __attribute__((weak));
void SystemInit(void)        {}

void __libc_init_array(void) __attribute__((weak));
void __libc_init_array(void) {}

#define WEAK_ALIAS(x) __attribute__((weak, alias(#x)))

/* Default handlers */
void NMI_Handler(void)          WEAK_ALIAS(Default_Handler);
void HardFault_Handler(void)    WEAK_ALIAS(Default_Handler);
void SVC_Handler(void)          WEAK_ALIAS(Default_Handler);
void PendSV_Handler(void)       WEAK_ALIAS(Default_Handler);
void SysTick_Handler(void)      WEAK_ALIAS(Default_Handler);

/* Interrupt vector table */
__attribute__((section(".isr_vector")))
const void *VectorTable[] = {
    &_estack,           // Initial stack pointer
    Reset_Handler,      // Reset
    NMI_Handler,        // NMI
    HardFault_Handler,  // HardFault
    0, 0, 0, 0, 0, 0, 0,// Reserved
    SVC_Handler,        // SVCall
    0, 0,               // Reserved
    PendSV_Handler,     // PendSV
    SysTick_Handler     // SysTick
    // Add IRQs here as needed
};

/* Reset Handler */
void Reset_Handler(void)
{
    uint32_t *src, *dst;

    // Copy .data from FLASH to RAM
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata)
        *dst++ = *src++;

    // Zero-initialize .bss
    dst = &_sbss;
    while (dst < &_ebss)
        *dst++ = 0;

    SystemInit();          // CMSIS-style hook (optional)
    main();                // Your main application

    while (1);             // Trap in case main returns
}

/* Fallback interrupt handler */
void Default_Handler(void)
{


    while (1);
}
