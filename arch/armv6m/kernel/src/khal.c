/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.2
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

/******************************************************************************
 *
 *  Module          : HAL
 *  Depends on      : -
 *  Provides to     : ALL
 *  Public API      : NO
 *
 *****************************************************************************/

/*
 This is intended as a minimal CPU HAL for ARMv6-M (Cortex-M0)
 handling NVIC, SCB, SysTick.
 */

#include <khal.h>
extern unsigned long SystemCoreClock;
unsigned kCoreSetPriorityGrouping( unsigned priorityGroup)
{
	/* M0 doesn't support priority grouping, ignore parameter */
	(void)priorityGroup;
    return (1);
}

unsigned kCoreGetPriorityGrouping( void)
{
	/* N/S */
	return (1);
}

void kCoreEnableFaults( void)
{
	/*N/S*/
}

void kCoreSetInterruptPriority( int IRQn, unsigned priority)
{
	/* supports 4 priority levels (2 bits) */

	priority = (priority & 0x3) << 6;

	if (IRQn < 0)
	{
		if (IRQn == RK_CORE_SVC_IRQN || IRQn == RK_CORE_PENDSV_IRQN
				|| IRQn == RK_CORE_SYSTICK_IRQN)
		{
			RK_CORE_SCB->SHP[(((unsigned) IRQn) & 0xF) - 4] =
					(unsigned char) priority;
		}
	}
	else
	{
		/* 32 external interrupts */
		if (IRQn < 32)
		{
			RK_CORE_NVIC->IP[IRQn] = (unsigned char) priority;
		}
	}
}

unsigned kCoreGetInterruptPriority( int IRQn)
{
	if (IRQn < 0)
	{
		/* System handler priority */
		if (IRQn == RK_CORE_SVC_IRQN || IRQn == RK_CORE_PENDSV_IRQN
				|| IRQn == RK_CORE_SYSTICK_IRQN)
		{
			return (RK_CORE_SCB->SHP[(((unsigned) IRQn) & 0xF) - 4] >> 6);
		}
		return 0;
	}
	else
	{
		/* IRQ priority */
		if (IRQn < 32)
		{
			return (RK_CORE_NVIC->IP[IRQn] >> 6);
		}
		return 0;
	}
}

void kCoreEnableInterrupt( int IRQn)
{
	if (IRQn >= 0 && IRQn < 32)
	{
		RK_CORE_NVIC->ISER[0] = (1UL << IRQn);
	}
}

void kCoreDisableInterrupt( int IRQn)
{
	if (IRQn >= 0 && IRQn < 32)
	{
		RK_CORE_NVIC->ICER[0] = (1UL << IRQn);
	}
}

void kCoreClearPendingInterrupt( int IRQn)
{
	if (IRQn >= 0 && IRQn < 32)
	{
		RK_CORE_NVIC->ICPR[0] = (1UL << IRQn);
	}
}

void kCoreSetPendingInterrupt( int IRQn)
{
	if (IRQn >= 0 && IRQn < 32)
	{
		RK_CORE_NVIC->ISPR[0] = (1UL << IRQn);
	}
}

unsigned kCoreGetPendingInterrupt( int IRQn)
{
	if (IRQn >= 0 && IRQn < 32)
	{
		return (((RK_CORE_NVIC->ISPR[0] & (1UL << IRQn)) != 0) ? 1U : 0U);
	}
	else
	{
		return 0U;
	}
}

/*
 * SysTickCore Functions
 */

/*
 * SysTickCore Functions
 */
unsigned long RKVAL_SysCoreClock = RK_CONF_SYSCORECLK;
#ifdef RK_CONF_SYSTICK_DIV
unsigned long RKVAL_SysTickDivisor = RK_CONF_SYSTICK_DIV;
#else
unsigned long RKVAL_SysTickDivisor = 0;
#endif

extern unsigned long int SystemCoreClock;

unsigned kCoreSysTickConfig( unsigned ticks)
{
	/* CheckCore if number of ticks is valid */
	if ((ticks - 1) > 0xFFFFFFUL) /*24-bit max*/
	{
		return (0xFFFFFFFF);
	}
	#if (RK_CONF_SYSCORECLK == 0)

	if (RKVAL_SysCoreClock == 0)
		RKVAL_SysCoreClock = SystemCoreClock;

	#endif	
		/* Set reload register */
	RK_CORE_SYSTICK->LOAD = (ticks - 1);

	/* Reset the SysTick counter */
	RK_CORE_SYSTICK->VAL = 0;

	RK_CORE_SYSTICK->CTRL = 0x06; /* keep interrupt disabled */

	#ifndef RK_CONF_SYSTICK_DIV
	
	RKVAL_SysTickInterval = (ticks * 1000UL) / (RKVAL_SysCoreClock);
	
	#else

	RKVAL_SysTickInterval = 1000UL/RK_CONF_SYSTICK_DIV;

	#endif

	
	return (0);
}

extern unsigned long int SystemCoreClock;


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

void kCoreInit(void)
{ 
	#if (RK_CONF_SYSCORECLK == 0)

    if (RKVAL_SysCoreClock == 0) 
    { 
      kCoreSysTickConfig( SystemCoreClock/RK_CONF_SYSTICK_DIV); 
    } 
    else 
    { 
      kCoreSysTickConfig( RKVAL_SysCoreClock/RK_CONF_SYSTICK_DIV); 
    } 
	
	#else
		kCoreSysTickConfig( RKVAL_SysCoreClock/RK_CONF_SYSTICK_DIV); 
	#endif
	kCoreSetInterruptPriority( RK_CORE_SVC_IRQN, 0x02); 
    kCoreSetInterruptPriority( RK_CORE_SYSTICK_IRQN, 0x03); 
    kCoreSetInterruptPriority( RK_CORE_PENDSV_IRQN, 0x03); 
}