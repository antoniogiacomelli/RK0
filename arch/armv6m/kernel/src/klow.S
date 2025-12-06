/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.2
 * Architecture     :   ARMv6m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/
 /*****************************************************************************
 *  COMPONENT         : LOW-LEVEL SCHEDULER
 *  PROVIDES TO       : ALL
 *  DEPENDS ON        : HARDWARE-DEPENDENT
 *  PROVIDES TO       : HIGH-LEVEL SCHEDULER
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
.equ STACK_GUARD, 0x0BADC0DE
.equ FAULT_STACKOVERVLOW, 0xFAFAFAFA
.equ STACKADDR, 16

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


USRSTARTUP:
    CPSID I             /* Disable interrupts */
    MOVS R0, #2         /* 0b10 = PSP, PRIV */
    MSR CONTROL, R0
    NOP
    LDR R1, =RK_gRunPtr
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
    LDR R1, =RK_gRunPtr
    LDR R2, [R1]
    LDR R0, [R2, #SP_OFFSET]      /* load stack pointer value */
    ADDS R0, #32                  /* adjust to match processor expected frame */
    MSR PSP, R0                   /* update PSP */
    NOP
    LDR R3, =0xFFFFFFFD
    STR R3, [R2, #LR_OFFSET]
    CPSIE I
    BX R3

.global SysTick_Handler
.thumb_func
SysTick_Handler:
    CPSID I
    PUSH {R0, R4}                /* R4 as working register */
    MOV R4, LR               /* MOV not MOVS for high register */
    BL kTickHandler
    CMP R0, #1
    BNE NOSWITCH
    /* fallthrough */
    LDR R0, =SCB_ICSR
    LDR R1, =ISCR_SETPSV
    LDR R2, [R0]
    ORRS R2, R1
    STR R2, [R0]            /* trigger pendsv  */
NOSWITCH:
    MOV LR, R4               /* restore LR from R4 */
    POP {R0, R4}                 /* restore R4 */
    CPSIE I
    BX LR

#if (__KDEF_STACKOVFLW == 1)
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
#endif

/* armv6m ctxt swtch */
.global PendSV_Handler
.thumb_func
PendSV_Handler:
    CPSID   I                  /* mask interrupts                */
    MRS     R0,  PSP           /* R0 = PSP  (points at HW frame) */

    NOP

    /* as there is no LDMIA! STMDB! we do this:                      */

    SUBS    R0,  #16
    STMIA   R0!, {R4-R7}       /* stack R4-R7; R0 += 16          */

    MOV     R4,  R8
    MOV     R5,  R9
    MOV     R6,  R10
    MOV     R7,  R11

    SUBS    R0,  #16
    STMIA   R0!, {R4-R7}       /* stack R8-R11; R0 += 16         */

    /* now point R0 back to bottom of software frame  */
    SUBS    R0,  #32           /* R0 = PSP – 32 (top) */
    LDR     R1, =RK_gRunPtr
    LDR     R2, [R1]           /* R2 = cur TCB                   */

    NOP

#if (__KDEF_STACKOVFLW == 1)
    CHECK_STACK_GUARD R2, SAVE
#endif
    SAVE:
    STR     R0, [R2, #SP_OFFSET]
    MOV     R3, LR
    STR     R3, [R2, #LR_OFFSET]

    /* call the beast  */
    BL      kSwtch    
    LDR     R0, =RK_gRunPtr
    LDR     R1, [R0]           /* R1 = new task */
#if (__KDEF_STACKOVFLW == 1)
    CHECK_STACK_GUARD R1, RESTORE
#endif
    RESTORE:
    LDR     R0, [R1, #SP_OFFSET]   /* R0 = saved PSP (bottom frame)*/
    /* pop HIGH regs R8-R11  */
    LDMIA   R0!, {R4-R7}    
    MOV     R8,  R4
    MOV     R9,  R5
    MOV     R10, R6
    MOV     R11, R7

    /*   pop LOW regs R4-R7  */
    LDMIA   R0!, {R4-R7}

    /*   restore PSP & LR   */
    MSR     PSP, R0
    
    NOP
    
    LDR     R3,  [R1, #LR_OFFSET]
    MOV     LR,  R3

    MOVS    R2, #RUNNING
    STR     R2, [R1, #STATUS_OFFSET]

#if (NDEBUG == 0)
    LDR     R2, [R1, #RUNCNTR_OFFSET]
    ADDS    R2, #1
    STR     R2, [R1, #RUNCNTR_OFFSET]
#endif
    
    CPSIE   I

    BX      LR
