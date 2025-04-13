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
/******************************************************************************
 *
 *  Module          : HAL
 *  Depends on      : -
 *  Provides to     : EXECUTIVE
 *  Public API      : NO
 *
 *****************************************************************************/

/*
 This is intended as a minimal CPU HAL for ARMv6-M (Cortex-M0)
 handling NVIC, SCB, SysTick.
 */

#include <khal.h>

unsigned kCoreSetPriorityGrouping( unsigned priorityGroup)
{
	/* Cortex-M0 doesn't support priority grouping, ignore parameter */
	unsigned reg_value;

	reg_value = RK_CORE_SCB->AIRCR;
	reg_value &= ~(0xFFFFUL << 16);
	reg_value = (0x5FAUL << 16); /* Key value needed for writing to AIRCR */
	RK_CORE_SCB->AIRCR = reg_value;

	/* Return 0 since Cortex-M0 only supports one grouping */
	return 0;
}

unsigned kCoreGetPriorityGrouping( void)
{
	/* N/S */
	return 0;
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
 * SysTick Functions
 */

unsigned kCoreSysTickConfig( unsigned ticks)
{
	/* Check if number of ticks is valid */
	if ((ticks - 1) > 0xFFFFFFUL)
	{
		return (0xFFFFFFFF);
	}

	/* set reload register */
	RK_CORE_SYSTICK->LOAD = (ticks - 1);

	/* resetcounter */
	RK_CORE_SYSTICK->VAL = 0;

	 /* Enable counter, interrupts, and use processor clock */
 	RK_CORE_SYSTICK->CTRL = 0x07;

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
