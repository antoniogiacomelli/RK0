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
/******************************************************************************
 *
 *  Module          : HAL
 *  Depends on      : -
 *  Provides to     : EXECUTIVE
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
	unsigned reg_value;
	unsigned PriorityGroupTmp = (priorityGroup & 0x07UL);

	reg_value = RK_CORE_SCB->AIRCR;
	reg_value &= ~((0xFFFFUL << 16) | (0x07UL << 8));
	reg_value = ((0x5FAUL << 16) | (PriorityGroupTmp << 8));
	RK_CORE_SCB->AIRCR = reg_value;

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
	if ((ticks - 1) > 0xFFFFFFUL)
	{
		return (0xFFFFFFFF);
	}

	/* Set reload register */
	RK_CORE_SYSTICK->LOAD = (ticks - 1);

	/* Reset the SysTickCore counter */
	RK_CORE_SYSTICK->VAL = 0;

	/* Enable SysTickCore IRQ and timer */
	RK_CORE_SYSTICK->CTRL = 0x07;/* Enable counter, interrupts, and use processor clockCore */

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
