/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.1
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

/*******************************************************************************
 *
 * This is a minimal HAL for ARMv6-M (Cortex-M0) CPU cores. It implements only the control
 * registers the kernel uses (SYSTICK, NVIC, SCB).
 * Note: ARMv6-M has a reduced set of features compared to ARMv7-M.
 ******************************************************************************/

#ifndef KHALCORE_H
#define KHALCORE_H

/* Common types needed across all platforms */
typedef struct
{
    volatile unsigned CPUID;/* CPU ID Base Register */
    volatile unsigned ICSR;/* Interrupt Control and State Register */
    volatile unsigned RESERVED0;
    volatile unsigned AIRCR;/* Application Interrupt and Reset Control Register */
    volatile unsigned SCR;/* System Control Register */
    volatile unsigned CCR;/* Configuration Control Register */
    volatile unsigned RESERVED1;
    volatile unsigned char SHP[2];/* System Handlers Priority Registers (ARMv6-M has only 2) */
    volatile unsigned SHCSR;/* System Handler Control and State Register */
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
    volatile unsigned ISER[1];/* Interrupt Set Enable Register (ARMv6-M has only 1) */
    unsigned RESERVED0[31];
    volatile unsigned ICER[1];/* Interrupt Clear Enable Register (ARMv6-M has only 1) */
    unsigned RESERVED1[31];
    volatile unsigned ISPR[1];/* Interrupt Set Pending Register (ARMv6-M has only 1) */
    unsigned RESERVED2[31];
    volatile unsigned ICPR[1];/* Interrupt Clear Pending Register (ARMv6-M has only 1) */
    unsigned RESERVED3[31];
    unsigned RESERVED4[64];
    volatile unsigned char IP[32];/* Interrupt Priority Register (ARMv6-M has limited priorities) */
} RK_CORE_NVIC_TYPE;

/* Platform-specific memory map for core peripherals */
#define RK_CORE_SCB_BASE            (0xE000ED00UL)
#define RK_CORE_SYSTICK_BASE        (0xE000E010UL)
#define RK_CORE_NVIC_BASE           (0xE000E100UL)

#define RK_CORE_SCB                 ((RK_CORE_SCB_TYPE *)RK_CORE_SCB_BASE)
#define RK_CORE_SYSTICK             ((RK_CORE_SYSTICK_TYPE *)RK_CORE_SYSTICK_BASE)
#define RK_CORE_NVIC                ((RK_CORE_NVIC_TYPE *)RK_CORE_NVIC_BASE)

#define RK_CORE_SVC_IRQN                 ((int)-5)
#define RK_CORE_PENDSV_IRQN              ((int)-2)
#define RK_CORE_SYSTICK_IRQN             ((int)-1)


void kCoreSystemReset(void);

unsigned kCoreSetPriorityGrouping(unsigned priorityGroup);

unsigned kCoreGetPriorityGrouping(void);

void kCoreEnableFaults(void);

void kCoreSetInterruptPriority(int IRQn, unsigned priority);

unsigned kCoreGetInterruptPriority(int IRQn);

void kCoreEnableInterrupt(int IRQn);

void kCoreDisableInterrupt(int IRQn);

void kCoreClearPendingInterrupt(int IRQn);

void kCoreSetPendingInterrupt(int IRQn);

unsigned kCoreGetPendingInterrupt(int IRQn);

unsigned kCoreSysTickConfig(unsigned ticks);

unsigned kCoreGetSysTickValue(void);

void kCoreEnableSysTick(void);

void kCoreDisableSysTick(void);

#endif
