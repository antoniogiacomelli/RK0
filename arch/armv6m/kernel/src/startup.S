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

.syntax unified
.cpu cortex-m0
.thumb

    .extern _estack
    .extern _sidata
    .extern _sdata
    .extern _edata
    .extern _sbss
    .extern _ebss

    .extern main             /* C entry point */

    .extern SVC_Handler
    .extern PendSV_Handler
    .extern SysTick_Handler
    .extern UART0_Handler
    .extern USART2_IRQHandler

    .thumb_func
Default_Handler:
    b Default_Handler

    .thumb_func
HardFault_Handler:
    b HardFault_Handler

    .thumb_func
NMI_Handler:
    b Default_Handler

/* Vector table */

    .section .isr_vector, "a", %progbits
    .align 2
    .globl g_pfnVectors
g_pfnVectors:
    .word _estack            /*  0: Initial MSP */
    .word Reset_Handler      /*  1: Reset */
    .word NMI_Handler        /*  2: NMI */
    .word HardFault_Handler  /*  3: HardFault */
    .word Default_Handler    /*  4: Reserved */
    .word Default_Handler    /*  5: Reserved */
    .word Default_Handler    /*  6: Reserved */
    .word Default_Handler    /*  7: Reserved */
    .word Default_Handler    /*  8: Reserved */
    .word Default_Handler    /*  9: Reserved */
    .word Default_Handler    /* 10: Reserved */
    .word SVC_Handler        /* 11: SVCall */
    .word Default_Handler    /* 12: Reserved */
    .word Default_Handler    /* 13: Reserved */
    .word PendSV_Handler     /* 14: PendSV */
    .word SysTick_Handler    /* 15: SysTick */

#if defined(STM32F030x8) || defined(RK_MCU_F030R8)
    .word Default_Handler    /* IRQ  0: WWDG */
    .word 0                  /* IRQ  1: Reserved */
    .word Default_Handler    /* IRQ  2: RTC */
    .word Default_Handler    /* IRQ  3: FLASH */
    .word Default_Handler    /* IRQ  4: RCC */
    .word Default_Handler    /* IRQ  5: EXTI0_1 */
    .word Default_Handler    /* IRQ  6: EXTI2_3 */
    .word Default_Handler    /* IRQ  7: EXTI4_15 */
    .word 0                  /* IRQ  8: Reserved */
    .word Default_Handler    /* IRQ  9: DMA1_Channel1 */
    .word Default_Handler    /* IRQ 10: DMA1_Channel2_3 */
    .word Default_Handler    /* IRQ 11: DMA1_Channel4_5 */
    .word Default_Handler    /* IRQ 12: ADC1 */
    .word Default_Handler    /* IRQ 13: TIM1_BRK_UP_TRG_COM */
    .word Default_Handler    /* IRQ 14: TIM1_CC */
    .word 0                  /* IRQ 15: Reserved */
    .word Default_Handler    /* IRQ 16: TIM3 */
    .word Default_Handler    /* IRQ 17: TIM6 */
    .word 0                  /* IRQ 18: Reserved */
    .word Default_Handler    /* IRQ 19: TIM14 */
    .word Default_Handler    /* IRQ 20: TIM15 */
    .word Default_Handler    /* IRQ 21: TIM16 */
    .word Default_Handler    /* IRQ 22: TIM17 */
    .word Default_Handler    /* IRQ 23: I2C1 */
    .word Default_Handler    /* IRQ 24: I2C2 */
    .word Default_Handler    /* IRQ 25: SPI1 */
    .word Default_Handler    /* IRQ 26: SPI2 */
    .word Default_Handler    /* IRQ 27: USART1 */
    .word USART2_IRQHandler  /* IRQ 28: USART2 */
    .word 0                  /* IRQ 29: Reserved */
    .word 0                  /* IRQ 30: Reserved */
    .word 0                  /* IRQ 31: Reserved */
#else
    .word Default_Handler    /* IRQ 0: POWER_CLOCK */
    .word Default_Handler    /* IRQ 1: RADIO */
    .word UART0_Handler      /* IRQ 2: UART0 */
    .rept 29
        .word Default_Handler
    .endr
#endif

/* Reset Handler */

    .text
    .thumb
    .align 2
    .globl Reset_Handler
    .thumb_func
Reset_Handler:
    /* MSP already loaded from vector[0] */

    /* Copy .data from FLASH to RAM: [_sidata] → [_sdata, _edata) */
    ldr r0, =_sidata      /* src */
    ldr r1, =_sdata       /* dst */
    ldr r2, =_edata       /* end dst */

1:  cmp r1, r2
    bcc 2f
    b   3f
2:  ldr r3, [r0]          /* r3 = *src */
    str r3, [r1]          /* *dst = r3 */
    adds r0, #4           /* src += 4 */
    adds r1, #4           /* dst += 4 */
    b   1b

3:
    /* Zero .bss: [_sbss, _ebss) */
    ldr r0, =_sbss        /* dst */
    ldr r1, =_ebss        /* end */
    movs r2, #0           /* value = 0 */

4:  cmp r0, r1
    bcc 5f
    b   6f
5:  str r2, [r0]          /* *dst = 0 */
    adds r0, #4           /* dst += 4 */
    b   4b

6:
    /* Optional: set CONTROL etc. if you want.
       RK0 will later switch to PSP in SVC/USRSTARTUP anyway. */

    /* Jump to C entry point */
    bl  main

    /* If main ever returns, just spin */
7:  b   7b

/* Weak aliases (can be overridden in C if you want custom handlers) */
    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak SysTick_Handler
    .weak PendSV_Handler
    .weak SVC_Handler
    .weak UART0_Handler
    .thumb_set UART0_Handler, Default_Handler

    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
