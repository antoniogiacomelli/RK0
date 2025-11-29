/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.1                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

/******************************************************************************
 * 
 *  COMPONENT         : LOW-LEVEL SCHEDULER
 *  PROVIDES TO       : ALL
 *  DEPENDS ON        : HARDWARE-DEPENDENT
 *  PROVIDES TO       : HIGH-LEVEL SCHEDULER
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
.equ STACK_GUARD, 0x0BADC0DE
.equ FAULT_STACKOVERVLOW, 0xFAFAFAFA
.equ SP_OFFSET, 0
.equ STATUS_OFFSET, 4
.equ RUNCNTR_OFFSET, 8
.equ LR_OFFSET, 12
.equ STACKADDR, 16



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
    TST LR, #4     /* for MSP, LR rightmost nibble is x9 (0b1001) */
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
    LDR R1, =RK_gRunPtr
    LDR R2, [R1, #SP_OFFSET]
    MOVS R12, #RUNNING
    STR R12, [R2, #STATUS_OFFSET]
    MOVS R12, #0x01
    STR R12, [R2, #RUNCNTR_OFFSET]
    LDR R2, [R2, #SP_OFFSET] /* base address == &(RK_gRunPtr->sp)              */
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
    CPSIE I
    BX LR
SWITCH:                   /* defer ctxt swtch */
    LDR R0, =SCB_ICSR
    MOV R1, #ISCR_SETPSV
    STR R1, [R0]
    DSB
    POP {R0, LR}
    CPSIE I
    BX LR


.if (__KDEF_STACKOVFLW == 1)
.macro  CHECK_STACK_GUARD tcbReg, successLabel
        LDR   R2, [\tcbReg, #STACKADDR]   /* R2 = &stackAddr */
        LDR   R3, [R2]                     /* R3 = *(stackBottom) */
        LDR   R2, =STACK_GUARD            /* R2 = expected canary */
        CMP   R2, R3
        BEQ   \successLabel
        /*error: fallthrough */
        LDR   R0, =FAULT_STACKOVERVLOW
        LDR   R4, =kErrHandler
        BX    R4
.endm
.endif
/* ctxt swtch */
.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func
PendSV_Handler:
    CPSID I
    MRS R12, PSP
.if (__FPU_PRESENT == 1)
    TST LR, #0x10          /* LR & 0b10000 ? */
    BNE SKIPSAVEVFP        /* not zero, (Z = 0) no FPU context */
    VSTMDB R12!, {S16-S31} /* zero */
    SKIPSAVEVFP:
.endif

    STMDB R12!, {R4-R11}
    LDR R0, =RK_gRunPtr
    LDR R1, [R0]
#if (__KDEF_STACKOVFLW == 1)
    /* check for stack overflow */
    CHECK_STACK_GUARD R1, SAVE
#endif
    SAVE:
    STR R12, [R1, #SP_OFFSET]     /* save PSP */
    STR LR, [R1, #LR_OFFSET]     /* save LR  */
    /* call scheduler */
    BL kSwtch
    /* load new task context */
    LDR R0, =RK_gRunPtr
     LDR R1, [R0]
#if (__KDEF_STACKOVFLW == 1)
    CHECK_STACK_GUARD R1, RESTORE 
#endif
    RESTORE:
    /* update run counter and status */
#if (NDEBUG==0)
    LDR R3, [R1, #RUNCNTR_OFFSET]
    ADD R3, #1
    STR R3, [R1, #RUNCNTR_OFFSET]
#endif
    MOV R12, #RUNNING
    STR R12, [R1, #STATUS_OFFSET]

    /* get saved LR at TCB */
    LDR LR, [R1, #LR_OFFSET]

    /*  get saved PSP value */
    LDR R2, [R1, #SP_OFFSET]
    LDMIA R2!, {R4-R11}

.if (__FPU_PRESENT == 1)

    TST LR, #0x10
    BNE SKIPRESTOREVFP
/* restore registers with FPU state  */
    VLDMIA R2!, {S16-S31}
    SKIPRESTOREVFP:
.endif
 /* assign PSP */
    MSR PSP, R2
    DSB
    CPSIE I
    ISB
    BX LR


