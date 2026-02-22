/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.0                                                           */
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

    /* Fill remaining IRQ vectors with default handler for now */
    .rept 32
        .word Default_Handler
    .endr

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
