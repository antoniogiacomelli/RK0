/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.16
 * Architecture     :   ARMv7m
 *
 * Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>
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
 * 
 ******************************************************************************/

#ifndef RK_HAL_H
#define RK_HAL_H

#ifdef __cplusplus
extern "C" {
#endif


extern unsigned long RK_gSyTickDiv;
extern unsigned long RK_gSysCoreClock;
extern unsigned long RK_gSysTickInterval;


/* Platform-specific memory map for core peripherals */
#define RK_CORE_SCB_BASE            (0xE000ED00UL)
#define RK_CORE_SYSTICK_BASE        (0xE000E010UL)
#define RK_CORE_NVIC_BASE           (0xE000E100UL)

#define RK_CORE_SVC_IRQN                 ((int)-5)
#define RK_CORE_DEBUGMON_IRQN            ((int)-4)
#define RK_CORE_PENDSV_IRQN              ((int)-2)
#define RK_CORE_SYSTICK_IRQN             ((int)-1)


void kCoreInit(void);

#ifdef __cplusplus
}
#endif

#endif
