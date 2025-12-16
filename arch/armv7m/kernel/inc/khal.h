/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.2
 * Architecture     :   ARMv7m
 *
 * Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>
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

/*******************************************************************************
 *
 * This is a minimal HAL for ARMv7M CPU cores. It implements only the control
 * registers the kernel uses (SYSTICK, NVIC, SCB).
 * Some core low-level manipulations use CMSIS GCC.
 ******************************************************************************/

#ifndef RK_HAL_H
#define RK_HAL_H


extern unsigned long RK_gSyTickDiv;
extern unsigned long RK_gSysCoreClock;
extern unsigned long RK_gSysTickInterval;

/* Common types needed across all platforms */
typedef struct
{
    volatile unsigned CPUID;/* CPU ID Base Register */
    volatile unsigned ICSR;/* Interrupt Control and State Register */
    volatile unsigned VTOR;/* Vector Table Offset Register */
    volatile unsigned AIRCR;/* Application Interrupt and Reset Control Register */
    volatile unsigned SCR;/* System Control Register */
    volatile unsigned CCR;/* Configuration Control Register */
    volatile unsigned char SHP[12];/* System Handlers Priority Registers */
    volatile unsigned SHCSR;/* System Handler Control and State Register */
    volatile unsigned CFSR;/* Configurable Fault Status Register */
    volatile unsigned HFSR;/* HardFault Status Register */
    volatile unsigned DFSR;/* Debug Fault Status Register */
    volatile unsigned MMFAR;/* MemManage Fault Address Register */
    volatile unsigned BFAR;/* BusFault Address Register */
    volatile unsigned AFSR;/* Auxiliary Fault Status Register */
} RK_CORE_SCB_TYPE;

typedef struct
{
    volatile unsigned CTRL;/* Control Register */
    volatile unsigned LOAD;/* Reload Value Register */
    volatile unsigned VAL;/* Current Value Register */
    volatile unsigned CALIB;/* Calibration Register */
} RK_CORE_SYSTICK_TYPE;

typedef struct
{
    volatile unsigned ISER[8];/* Interrupt Set Enable Register */
    unsigned RESERVED0[24];
    volatile unsigned ICER[8];/* Interrupt Clear Enable Register */
    unsigned RESERVED1[24];
    volatile unsigned ISPR[8];/* Interrupt Set Pending Register */
    unsigned RESERVED2[24];
    volatile unsigned ICPR[8];/* Interrupt Clear Pending Register */
    unsigned RESERVED3[24];
    volatile unsigned IABR[8];/* Interrupt Active Bit Register */
    unsigned RESERVED4[56];
    volatile unsigned char IP[240];/* Interrupt Priority Register */
    unsigned RESERVED5[644];
    volatile unsigned STIR;/* Software Trigger Interrupt Register */
} RK_CORE_NVIC_TYPE;

/* Platform-specific memory map for core peripherals */
#define RK_CORE_SCB_BASE            (0xE000ED00UL)
#define RK_CORE_SYSTICK_BASE        (0xE000E010UL)
#define RK_CORE_NVIC_BASE           (0xE000E100UL)

#define RK_CORE_SCB                 ((RK_CORE_SCB_TYPE *)RK_CORE_SCB_BASE)
#define RK_CORE_SYSTICK             ((RK_CORE_SYSTICK_TYPE *)RK_CORE_SYSTICK_BASE)
#define RK_CORE_NVIC                ((RK_CORE_NVIC_TYPE *)RK_CORE_NVIC_BASE)

#define RK_CORE_SVC_IRQN                 ((int)-5)
#define RK_CORE_DEBUGMON_IRQN            ((int)-4)
#define RK_CORE_PENDSV_IRQN              ((int)-2)
#define RK_CORE_SYSTICK_IRQN             ((int)-1)



/* System reset */
void kCoreSystemReset( void);

/* Set priority grouping */
unsigned kCoreSetPriorityGrouping( unsigned priorityGroup);

/* Get priority grouping */
unsigned kCoreGetPriorityGrouping( void);

/* Enable fault exceptions */
void kCoreEnableFaults( void);

/* Set interrupt priority */
void kCoreSetInterruptPriority( int IRQn, unsigned priority);

/* Get interrupt priority */
unsigned kCoreGetInterruptPriority( int IRQn);

/* Enable interrupt */
void kCoreEnableInterrupt( int IRQn);

/* Disable interrupt */
void kCoreDisableInterrupt( int IRQn);

/* Clear pending interrupt */
void kCoreClearPendingInterrupt( int IRQn);

/* Set pending interrupt */
void kCoreSetPendingInterrupt( int IRQn);

/* Get pending interrupt status */
unsigned kCoreGetPendingInterrupt( int IRQn);

/* Configure and enable SysTickCore */
unsigned kCoreSysTickConfig( unsigned ticks);

/* Get current SysTickCore value */
unsigned kCoreGetSysTickValue( void);

/* Enable SysTickCore */
void kCoreEnableSysTick( void);

/* Disable SysTickCore */
void kCoreDisableSysTick( void);

void kCoreInit(void);

#endif
