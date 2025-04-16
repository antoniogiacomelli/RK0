/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv6-M (Cortex-M0)
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 *
 ******************************************************************************/
 /*****************************************************************************
 *  Module            : Low-level Scheduler/Start-Up.
 *  Provides to       : High-level Scheduler / Main application.
 *  Public API        : No
 *
 *  o In this unit:
 *                Low-level kernel routines.
 *    This assembly unit handles scheduler start-up (SVC Handler), 
 *    Low-level System Tick and Context Switching (PendSV).
 *
 *
 *****************************************************************************/
 
.syntax unified
.thumb
.text
.align 2

/* task status values */
.equ READY,   0x10
.equ RUNNING, 0x20

/* armv6m reg addresses */
.equ SCB_ICSR,   0xE000ED04 /* base icsr */
.equ STICK_CTRL, 0xE000E010 /* systick csr */

/* armv6m reg values */
.equ ISCR_SETPSV, (1 << 28)
.equ ISCR_CLRPSV, (1 << 27)
.equ STICK_CLRP, (1 << 30)
.equ STICK_ON,  0x0F
.equ STICK_OFF, 0x00

/* kernel specifics  */
.equ APP_START_UP_IMM,   0xAA
.equ USR_CTXT_SWTCH,     0xC5
.equ FAULT_SVC, 0xFF
.equ SP_OFFSET, 0
.equ STATUS_OFFSET, 4
.equ RUNCNTR_OFFSET, 8
.equ LR_OFFSET, 12


.global kApplicationInit
.thumb_func

.global kErrHandler  /* defined in kerr.c */
.thumb_func

.global SVC_Handler
.thumb_func
SVC_Handler:       /* we start-up from msp */
    MOVS R0, #4
    MOV R1, LR
    TST R0, R1
    BNE ERR
    MRS R0, MSP
    LDR R1, [R0, #24]
    SUBS R1, #2
    LDRB R2, [R1, #0]
    CMP R2, #APP_START_UP_IMM
    BEQ USRSTARTUP
    ERR:
    MOVS R0, #FAULT_SVC
    BL  kErrHandler

/* start user application */
.global USRSTARTUP
.thumb_func
USRSTARTUP:
    CPSID I             /* Disable interrupts */
    MOVS R0, #2         /* 0b10 = PSP, PRIV */
    MSR CONTROL, R0
    LDR R1, =runPtr
    LDR R2, [R1]
    MOVS R3, #RUNNING
    STR R3, [R2, #STATUS_OFFSET]
    MOVS R3, #0x01
    STR R3, [R2, #RUNCNTR_OFFSET]
    LDR R3, [R2, #SP_OFFSET]
    /* only low registers can be directly loaded */
    ADDS R3, #32                  /* advance to R4-R7 position */
    LDM R3!, {R4-R7}              /* load R4-R7 */
    SUBS R3, #48                  /* go back to get R8-R11 */
    LDR R0, [R3, #0]
    LDR R1, [R3, #4]
    LDR R2, [R3, #8]
    LDR R3, [R3, #12]
    MOV R8, R0                    /* move to high registers */
    MOV R9, R1
    MOV R10, R2
    MOV R11, R3
    /* get the stack pointer for the main context */
    LDR R1, =runPtr
    LDR R2, [R1]
    LDR R0, [R2, #SP_OFFSET]      /* load stack pointer value */
    ADDS R0, #32                  /* adjust to match processor expected frame */
    MSR PSP, R0                   /* update PSP */
    LDR R0, =0xFFFFFFFD
    CPSIE I
    BX R0



.global SysTick_Handler
.thumb_func
SysTick_Handler:
    CPSID I
    PUSH {R4}                /* R4 as working register */
    MOV R4, LR               /* MOV not MOVS for high register */
                             /* dont forget to pop lr (and r4!)*/
    BL kTickHandler
    CMP R0, #1
    BNE NOSWITCH
    LDR R0, =SCB_ICSR
    LDR R1, =ISCR_SETPSV
    LDR R2, [R0]
    ORRS R2, R1
    STR R2, [R0]            /* trigger pendsv  */
NOSWITCH:
    MOV LR, R4               /* restore LR from R4 */
    POP {R4}                 /* restore R4 */
    CPSIE I
    BX LR

/* armv6m ctxt swtch */
.global PendSV_Handler
.thumb_func
PendSV_Handler:
    CPSID I
    /* save context */
    MRS R0, PSP
    MOV R1, R8               /* copy R8-R11 to low registers we can work */
    MOV R2, R9
    MOV R3, R10
    MOV R4, R11
   /* SUBS + STMIA equals STMDB with writeback */
    SUBS R0, #16             /* create space for R8-R11 */
    STMIA R0!, {R1-R4}       /* push R8-R11 */
    SUBS R0, #16             /* R0 back to start of high reg space */
    /* push r4-r7 */
    SUBS R0, #16
    STMIA R0!, {R4-R7}
    SUBS R0, #16
    LDR R1, =runPtr
    LDR R2, [R1]
    STR R0, [R2, #SP_OFFSET]
    MOV R3, LR
    STR R3, [R2, #LR_OFFSET]
    /* call the beast */
    BL kSchSwtch
    LDR R0, =runPtr
    LDR R1, [R0]
    MOVS R2, #RUNNING
    STR R2, [R1, #STATUS_OFFSET]
    LDR R2, [R1, #RUNCNTR_OFFSET]
    ADDS R2, #1
    STR R2, [R1, #RUNCNTR_OFFSET]
    LDR R3, [R1, #LR_OFFSET]
    MOV LR, R3
    LDR R0, [R1, #SP_OFFSET]
    /* pop R4-R7  */
    LDMIA R0!, {R4-R7}
    /* r8-r11 */
    LDMIA R0!, {R1-R4}       /* load values into R1-R4 with writeback */
    MOV R8, R1               /* move to high registers */
    MOV R9, R2
    MOV R10, R3
    MOV R11, R4
    MSR PSP, R0
    CPSIE I
    BX LR
