/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv7m
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
 *    Low-level System Tick and Context Switching (PendSV). Note the context-
 *    switching will only keep the extended frame when using a co-processor
 *    FPU if the pre-processor symbol __FPU_PRESENT=1 is (-D)efined. 
 *     
 *****************************************************************************/
 
.syntax unified /* thumb2 */
.text
.align 4

/* task status values */
.equ READY,   0x10
.equ RUNNING, 0x20

/* armv7m reg addresses */
.equ SCB_ICSR,   0xE000ED04 /* base icsr */
.equ STICK_CTRL, 0xE000E010 /* systick csr */

/* arvm7m reg values */
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

.global __getReadyPrio
.type __getReadyPrio, %function
.thumb_func
__getReadyPrio:
CLZ  R12, R0
MOV  R0, R12
NEG  R0, R0
ADD  R0, #31
BX LR


.global kApplicationInit
.type kApplicationInit, %function
.thumb_func

.global kErrHandler  /* defined in kerr.c */
.type kErrHandler, %function
.thumb_func

.global SVC_Handler
.type SVC_Handler, %function
.thumb_func
SVC_Handler:       /* we start-up from msp               */
    TST LR, #4     /* for MSP, LR here shall end with a x9               */
    BNE ERR
    MRS R12, MSP
    LDR R1, [R12, #24] /* <-  the instruction that brought us here       */
    LDRB R12, [R1,#-2] /* <- walk two bytes back on code memory,
                             get the immediate      */
    CMP R12, #APP_START_UP_IMM
    BEQ USRSTARTUP
    ERR:
    MOV R0, #FAULT_SVC
    BL  kErrHandler

/* start user application */
.global USRSTARTUP
.type USRSTARTUP, %function
.thumb_func
 USRSTARTUP:                  /* get camera-ready:                        */
    CPSID I
    MOVS R0, #2               /* 0b10 = PSP, PRIV                         */
    MSR CONTROL, R0
    DSB
    LDR R1, =runPtr
    LDR R2, [R1, #SP_OFFSET]
    MOVS R12, #RUNNING
    STR R12, [R2, #STATUS_OFFSET]
    MOVS R12, #0x01
    STR R12, [R2, #RUNCNTR_OFFSET]
    LDR R2, [R2, #SP_OFFSET] /* base address == &(runPtr->sp)              */
    LDMIA R2!, {R4-R11}      /* 'POP' R4-R11 as    assembled on kInitTcb_  */
    MSR PSP, R2              /* update PSP after 'popping '                */
    MOV LR, #0xFFFFFFFD      /* set LR to indicate we choose PSP           */
    DSB                      /* yelling to data bus */
    CPSIE I                  /* red lights on */
    ISB                      /* exorcising the ins pipe */
    BX LR                    /* play                    */

.global SysTick_Handler
.type SysTick_Handler, %function
.thumb_func
SysTick_Handler:
    CPSID I
    PUSH {R0, LR}         /* push LR, R0 for alignment */
    BL kTickHandler       /* always run kTickHandler, result in R0 */
    CMP R0, #1            /* check if context switch is needed */
    BEQ SWITCH
    /* fallthrough */
    POP {R0, LR}
    DSB
    CPSIE I
    ISB
    BX LR
SWITCH:                   /* defer ctxt swtch */
    POP {R0, LR}
    LDR R0, =SCB_ICSR
    MOV R1, #ISCR_SETPSV
    STR R1, [R0]
    DSB
    CPSIE I
    ISB
    BX LR

/* ctxt swtch */
.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func
PendSV_Handler:
    CPSID I
    MRS R12, PSP
.if (__FPU_PRESENT==1) /* compiler (-D)irective */
    TST LR, #0x10          /* LR & 0b10000 ? */
    BNE SKIPSAVEVFP        /* not zero, (Z = 0) no FPU context */
    VSTMDB R12!, {S16-S31} /* zero */
    SKIPSAVEVFP:
.endif

    STMDB R12!, {R4-R11}

    LDR R0, =runPtr
    LDR R1, [R0]
    STR R12, [R1, #SP_OFFSET]     /* save PSP */
    STR LR, [R1, #LR_OFFSET]     /* save LR  */

    /* call scheduler */
    BL kSchSwtch

    /* load new task context */
    LDR R0, =runPtr
    LDR R1, [R0]

    /* update run counter and status */
    LDR R3, [R1, #RUNCNTR_OFFSET]
    ADD R3, #1
    STR R3, [R1, #RUNCNTR_OFFSET]

    MOV R12, #RUNNING
    STR R12, [R1, #STATUS_OFFSET]

    /* saved LR at TCB */
    LDR LR, [R1, #LR_OFFSET]


   /*  saved PSP value */
    LDR R2, [R1, #SP_OFFSET]

    LDMIA R2!, {R4-R11}

.if (__FPU_PRESENT==1)

    TST LR, #0x10
    BNE SKIPRESTOREVFP

   /* restore registers with FPU state  */

    VLDMIA R2!, {S16-S31}
    SKIPRESTOREVFP:

.endif

    MSR PSP, R2

    DSB
    CPSIE I
    ISB
    BX LR
    
