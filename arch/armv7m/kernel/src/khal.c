/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv6/7m
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

/******************************************************************************
 *
 *  Module          : HAL
 *  Depends on      : -
 *  Provides to     : ALL
 *  Public API      : NO
 *
 *****************************************************************************/

/*

 This is intended as a minimal CPU HAL for ARMv7M
 handling NVIC, SCB, SysTick.
 It should be compatible with and M3, M4, M7 cores.

 */

#include <khal.h>

unsigned kCoreSetPriorityGrouping( unsigned priorityGroup)
{
	unsigned regVal;
	unsigned prioGroupTmp = (priorityGroup & 0x07UL);

	regVal = RK_CORE_SCB->AIRCR;
	regVal &= ~((0xFFFFUL << 16) | (0x07UL << 8));
	regVal = ((0x5FAUL << 16) | (prioGroupTmp << 8));
	RK_CORE_SCB->AIRCR = regVal;

	return (RK_CORE_SCB->AIRCR & (0x07UL << 8));
}

unsigned kCoreGetPriorityGrouping( void)
{
	return ((RK_CORE_SCB->AIRCR & (0x07UL << 8)) >> 8);
}

void kCoreEnableFaults( void)
{
	RK_CORE_SCB->SHCSR |= 0x00070000;/* Enable Usage, Bus, and Memory Fault */
}

void kCoreSetInterruptPriority( int IRQn, unsigned priority)
{
	if (IRQn < 0)
	{
		/* System handler priority */
		RK_CORE_SCB->SHP[(((unsigned) IRQn) & 0xF) - 4] =
				(unsigned char) ((priority << 4) & 0xFF);
	}
	else
	{
		/* IRQ priority */
		RK_CORE_NVIC->IP[IRQn] = (unsigned char) ((priority << 4) & 0xFF);
	}
}

unsigned kCoreGetInterruptPriority( int IRQn)
{
	if (IRQn < 0)
	{
		/* System handler priority */
		return (RK_CORE_SCB->SHP[(((unsigned) IRQn) & 0xF) - 4] >> 4);
	}
	else
	{
		/* IRQ priority */
		return (RK_CORE_NVIC->IP[IRQn] >> 4);
	}
}

void kCoreEnableInterrupt( int IRQn)
{
	if (IRQn >= 0)
	{
		RK_CORE_NVIC->ISER[IRQn >> 5] = (1UL << (IRQn & 0x1F));
	}
}

void kCoreDisableInterrupt( int IRQn)
{
	if (IRQn >= 0)
	{
		RK_CORE_NVIC->ICER[IRQn >> 5] = (1UL << (IRQn & 0x1F));
	}
}

void kCoreClearPendingInterrupt( int IRQn)
{
	if (IRQn >= 0)
	{
		RK_CORE_NVIC->ICPR[IRQn >> 5] = (1UL << (IRQn & 0x1F));
	}
}

void kCoreSetPendingInterrupt( int IRQn)
{
	if (IRQn >= 0)
	{
		RK_CORE_NVIC->ISPR[IRQn >> 5] = (1UL << (IRQn & 0x1F));
	}
}

unsigned kCoreGetPendingInterrupt( int IRQn)
{
	if (IRQn >= 0)
	{
		return (((RK_CORE_NVIC->ISPR[IRQn >> 5] & (1UL << (IRQn & 0x1F))) != 0) ?
				1U : 0U);
	}
	else
	{
		return 0U;
	}
}

/*
 * SysTickCore Functions
 */

unsigned kCoreSysTickConfig( unsigned ticks)
{
	/* CheckCore if number of ticks is valid */
	if ((ticks - 1) > 0xFFFFFFUL) /*24-bit max*/
	{
		return (0xFFFFFFFF);
	}

	/* Set reload register */
	RK_CORE_SYSTICK->LOAD = (ticks - 1);

	/* Reset the SysTick counter */
	RK_CORE_SYSTICK->VAL = 0;

	RK_CORE_SYSTICK->CTRL = 0x06; /* keep interrupt disabled */

	return (0);
}

unsigned kCoreGetSysTickValue( void)
{
	return (RK_CORE_SYSTICK->VAL);
}

void kCoreEnableSysTick( void)
{
	RK_CORE_SYSTICK->CTRL |= 0x01;
}

void kCoreDisableSysTick( void)
{
	RK_CORE_SYSTICK->CTRL &= (unsigned) ~0x01;
}
