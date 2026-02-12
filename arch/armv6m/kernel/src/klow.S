/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.15
 * Architecture     :   ARMv6-M (Cortex-M0)
 *
 ******************************************************************************/

.syntax unified
.thumb
.text
.align 2

/* task status values */
.equ READY,   0x10
.equ RUNNING, 0x20

/* armv6m reg addresses */
.equ SCB_ICSR,   0xE000ED04   
.equ STICK_CTRL, 0xE000E010   

/* armv6m reg values */
.equ ISCR_SETPSV, (1 << 28)
.equ ISCR_CLRPSV, (1 << 27)
.equ STICK_CLRP,  (1 << 30)
.equ STICK_ON,    0x0F
.equ STICK_OFF,   0x00

/* kernel specifics  */
.equ APP_START_UP_IMM,   0xAA
.equ FAULT_SVC,          0xFFFFFC72
.equ SP_OFFSET,          0
.equ STATUS_OFFSET,      4
.equ RUNCNTR_OFFSET,     8
.equ LR_OFFSET,          12
.equ STACK_GUARD,        0x0BADC0DE
.equ FAULT_STACKOVERVLOW,0xFAFAFAFA
.equ STACKADDR,          16

.global kApplicationInit
.thumb_func

.global kErrHandler  /* defined in kerr.c */
.thumb_func

.global kSwtch
.thumb_func

.global kTickHandler
.thumb_func

.global RK_gRunPtr


.global SVC_Handler
.thumb_func
.text
.align 4
SVC_Handler:

    MOVS    R0, #4
    MOV     R1, LR
    TST     R1, R0
    BNE     ERR

    /* Get stacked PC from MSP */
    MRS     R0, MSP
    LDR     R1, [R0, #24]    /* stacked PC */
    SUBS    R1, #2           /* SVC is 16-bit; go back 2 bytes */
    LDRB    R2, [R1, #0]     /* fetch SVC immediate */
    CMP     R2, #APP_START_UP_IMM
    BEQ     USRSTARTUP

ERR:
    LDR    R0, =FAULT_SVC
    BL      kErrHandler

.global USRSTARTUP
.thumb_func
.text
.align 2
USRSTARTUP:
    CPSID   I


    MOVS    R0, #2
    MSR     CONTROL, R0
    ISB

    /* Get current TCB */
    LDR     R1, =RK_gRunPtr
    LDR     R2, [R1]        /* R2 = TCB */

    MOVS    R3, #RUNNING
    STR     R3, [R2, #STATUS_OFFSET]
    MOVS    R3, #1
    STR     R3, [R2, #RUNCNTR_OFFSET]

    LDR     R0, [R2, #SP_OFFSET]

    /* Restore high regs R8–R11 */
    LDMIA   R0!, {R4-R7}
    MOV     R8,  R4
    MOV     R9,  R5
    MOV     R10, R6
    MOV     R11, R7

    /* Restore low regs R4–R7  */
    LDMIA   R0!, {R4-R7}

    MSR     PSP, R0

    LDR     R3, =0xFFFFFFFD
    STR     R3, [R2, #LR_OFFSET]

    DSB
    CPSIE   I
    ISB
    BX      R3

.global SysTick_Handler
.thumb_func
.text
.align 4
SysTick_Handler:
    PUSH {R0, R4}
    MOV  R4, LR

    BL   kTickHandler
    CMP  R0, #1
    BNE  NOSWTCH
    CPSID I
    LDR   R0, =SCB_ICSR
    LDR   R1, =ISCR_SETPSV     
    STR   R1, [R0]             
    CPSIE I
    NOSWTCH:
    MOV  LR, R4
    POP  {R0, R4}
    BX   LR


.if (__KDEF_STACKOVFLW == 1)
.macro  CHECK_STACK_GUARD tcbReg, successLabel
        LDR   R3, [\tcbReg, #STACKADDR]   /* R3 = tcb->stackAddr (bottom) */
        LDR   R4, [R3, #0]                /* R4 = stackAddr[0] */
        LDR   R3, =STACK_GUARD
        CMP   R3, R4
        BEQ   \successLabel

        LDR   R0, =FAULT_STACKOVERVLOW
        LDR   R4, =kErrHandler
        BX    R4
.endm
.endif
.global PendSV_Handler
.thumb_func
.text
.align 4
PendSV_Handler:

    CPSID   I
    MRS     R0, PSP

    SUBS    R0, #32


    /* save low regs R4–R7 at [sp+16,+20,+24,+28] */
    STR     R4, [R0, #16]
    STR     R5, [R0, #20]
    STR     R6, [R0, #24]
    STR     R7, [R0, #28]

    /* save  high regs R8–R11 via R4–R7 */
    MOV     R4, R8
    MOV     R5, R9
    MOV     R6, R10
    MOV     R7, R11
    STR     R4, [R0, #0]
    STR     R5, [R0, #4]
    STR     R6, [R0, #8]
    STR     R7, [R0, #12]

    LDR     R1, =RK_gRunPtr
    LDR     R2, [R1]

.if (__KDEF_STACKOVFLW == 1)
    CHECK_STACK_GUARD R2, SAVE
.endif

SAVE:
    STR     R0, [R2, #SP_OFFSET]
    MOV     R3, LR
    STR     R3, [R2, #LR_OFFSET]

    BL      kSwtch

    /* Load new TCB */
    LDR     R0, =RK_gRunPtr
    LDR     R1, [R0]           /* R1 = new TCB */

.if (__KDEF_STACKOVFLW == 1)
    CHECK_STACK_GUARD R1, RESTORE
.endif

 
RESTORE:

#ifndef NDEBUG 
    LDR     R2, [R1, #RUNCNTR_OFFSET]
    ADDS    R2, #1
    STR     R2, [R1, #RUNCNTR_OFFSET]
#endif

    MOVS    R2, #RUNNING
    STR     R2, [R1, #STATUS_OFFSET]

    LDR     R0, [R1, #SP_OFFSET]

    /* Restore high regs R8–R11 */
    LDMIA   R0!, {R4-R7}
    MOV     R8,  R4
    MOV     R9,  R5
    MOV     R10, R6
    MOV     R11, R7

    /* Restore low regs R4–R7  */
    LDMIA   R0!, {R4-R7}

    MSR     PSP, R0

    /* get LR from TCB */
    LDR     R3, [R1, #LR_OFFSET]
    MOV     LR, R3

    DSB
    CPSIE   I
    ISB
    BX      LR
