/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.1                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#pragma GCC diagnostic ignored "-Wpedantic"
/* see comment on vector table definition */

#include <stdint.h>

/* Forward declaration of the system exception handlers */
void Reset_Handler(void);
void Default_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void);
void DebugMon_Handler(void);
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
void USART2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
/* External definitions */
extern uint32_t _sidata;     /* Start address of the initialisation values of the .data section */
extern uint32_t _sdata;      /* Start address of the .data section */
extern uint32_t _edata;      /* End address of the .data section */
extern uint32_t _sbss;       /* Start address of the .bss section */
extern uint32_t _ebss;       /* End address of the .bss section */
extern uint32_t _estack;     /* alias for __stack */
/* Forward declaration for the main function */
extern int main(void);
/* The Vector Table */
__attribute__ ((section(".isr_vector")))
void (* const g_pfnVectors[])(void) =
{
    /* Core system exceptions */
   /* pedantic warning is ignored on start-up because of this cast of a function pointer to generic pointer ; workarounds are too cumbersome */
   (void*)&_estack,         /* The initial stack pointer */
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

#if defined(STM32F103xB) || defined(RK_MCU_F103RB)
    /* STM32F103RB external interrupts */
    Default_Handler,             /* IRQ 0: WWDG */
    Default_Handler,             /* IRQ 1: PVD */
    Default_Handler,             /* IRQ 2: TAMPER */
    Default_Handler,             /* IRQ 3: RTC */
    Default_Handler,             /* IRQ 4: FLASH */
    Default_Handler,             /* IRQ 5: RCC */
    Default_Handler,             /* IRQ 6: EXTI0 */
    Default_Handler,             /* IRQ 7: EXTI1 */
    Default_Handler,             /* IRQ 8: EXTI2 */
    Default_Handler,             /* IRQ 9: EXTI3 */
    Default_Handler,             /* IRQ 10: EXTI4 */
    Default_Handler,             /* IRQ 11: DMA1 channel 1 */
    Default_Handler,             /* IRQ 12: DMA1 channel 2 */
    Default_Handler,             /* IRQ 13: DMA1 channel 3 */
    Default_Handler,             /* IRQ 14: DMA1 channel 4 */
    Default_Handler,             /* IRQ 15: DMA1 channel 5 */
    Default_Handler,             /* IRQ 16: DMA1 channel 6 */
    Default_Handler,             /* IRQ 17: DMA1 channel 7 */
    Default_Handler,             /* IRQ 18: ADC1/ADC2 */
    Default_Handler,             /* IRQ 19: USB HP/CAN TX */
    Default_Handler,             /* IRQ 20: USB LP/CAN RX0 */
    Default_Handler,             /* IRQ 21: CAN RX1 */
    Default_Handler,             /* IRQ 22: CAN SCE */
    Default_Handler,             /* IRQ 23: EXTI9_5 */
    Default_Handler,             /* IRQ 24: TIM1 break */
    Default_Handler,             /* IRQ 25: TIM1 update */
    Default_Handler,             /* IRQ 26: TIM1 trigger/commutation */
    Default_Handler,             /* IRQ 27: TIM1 capture compare */
    Default_Handler,             /* IRQ 28: TIM2 */
    Default_Handler,             /* IRQ 29: TIM3 */
    Default_Handler,             /* IRQ 30: TIM4 */
    Default_Handler,             /* IRQ 31: I2C1 event */
    Default_Handler,             /* IRQ 32: I2C1 error */
    Default_Handler,             /* IRQ 33: I2C2 event */
    Default_Handler,             /* IRQ 34: I2C2 error */
    Default_Handler,             /* IRQ 35: SPI1 */
    Default_Handler,             /* IRQ 36: SPI2 */
    Default_Handler,             /* IRQ 37: USART1 */
    USART2_IRQHandler,           /* IRQ 38: USART2 */
    Default_Handler,             /* IRQ 39: USART3 */
    Default_Handler,             /* IRQ 40: EXTI15_10 */
    Default_Handler,             /* IRQ 41: RTC alarm */
    Default_Handler,             /* IRQ 42: USB wakeup */
#else
    /* QEMU lm3s6965evb external interrupts */
    GPIO_Handler,                /* IRQ 0: GPIO */
    Default_Handler,             /* IRQ 1 */
    Default_Handler,             /* IRQ 2 */
    Default_Handler,             /* IRQ 3 */
    Default_Handler,             /* IRQ 4 */
    UART0_Handler,               /* IRQ 5: UART0 */
    UART1_Handler,               /* IRQ 6: UART1 */
    SSI_Handler,                 /* IRQ 7: SSI */
    I2C_Handler,                 /* IRQ 8: I2C */
    PWM_Handler,                 /* IRQ 9: PWM */
    Default_Handler,             /* IRQ 10 */
    Default_Handler,             /* IRQ 11 */
    Default_Handler,             /* IRQ 12 */
    Default_Handler,             /* IRQ 13 */
    ADC_Handler,                 /* IRQ 14: ADC */
#endif
};


/**
 * @brief  System initialisation function
 */
void SystemInit(void) {
#if defined(STM32F103xB) || defined(RK_MCU_F103RB)
    *(volatile uint32_t *)0xE000ED08UL = (uint32_t)(uintptr_t)g_pfnVectors;
#endif
    /* RK0 will handle the system initialisation */
}

/**
 * @brief  Reset handler
 */
void Reset_Handler(void)
{
    uint32_t const *pSrc;
    uint32_t *pDest;

    /* Copy the data segment initialisers from flash to SRAM */
    pSrc = &_sidata;
    pDest = &_sdata;

    /* cppcheck-suppress comparePointers */
    while (pDest < &_edata)
    {
        *pDest++ = *pSrc++;
    }

    /* Zero fill the bss segment */
    /* cppcheck-suppress comparePointers */
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

void DebugMon_Handler(void)
{
    while(1)
    ;
}
