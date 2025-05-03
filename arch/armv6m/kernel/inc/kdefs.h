/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version              :   V0.4.0
 * Architecture         :   ARMv6m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 * www.kernel0.org
 *
 * Header: ARCHITECTURE-SPECIFIC DEFINES (ARMv6M)
 *
 ******************************************************************************/

#ifndef RK_DEFS_H
#define RK_DEFS_H

#include <kservices.h>

/* Assembly Helpers - ARMv6-M (Cortex-M0) compatible versions */
/* ARMv6-M doesn't have explicit DMB, DSB, ISB instructions, use memory barriers */
#define _RK_DMB                          __asm volatile("nop");
#define _RK_DSB                          __asm volatile("nop");
#define _RK_ISB                          __asm volatile("nop");
#define _RK_NOP                          __asm volatile("nop");
#define _RK_STUP                         __asm volatile("svc #0xAA");

/* Processor Core Management */
#define RK_CR_AREA    volatile UINT state;
#define RK_CR_ENTER   do { state = __get_PRIMASK(); __disable_irq(); } while(0);
#define RK_CR_EXIT    do { __set_PRIMASK(state); } while(0);

#define RK_PEND_CTXTSWTCH RK_TRAP_PENDSV
#define RK_TRAP_PENDSV  \
     RK_CORE_SCB->ICSR |= (1<<28U); \

#define RK_TRAP_SVC(N)  \
    do { __asm volatile ("svc %0" :: "i" (N)); } while(0)

#define RK_TICK_EN  RK_CORE_SYSTICK->CTRL |= 0x00000001;
#define RK_TICK_DIS RK_CORE_SYSTICK->CTRL &= 0xFFFFFFFE;

/* Modified for ARMv6-M (Cortex-M0) */
__attribute__((always_inline)) static inline
unsigned kIsISR( void)
{
	unsigned ipsr_value;
	/* ARMv6-M compatible way to read IPSR */
	__asm ("MRS %0, IPSR" : "=r"(ipsr_value));
	__asm volatile ("" ::: "memory");
	/* Memory barrier */
	return (ipsr_value);
}
/* for armv6m that does not have CLZ instruction
 * we use the builtin CTZ from gcc
 * */
extern unsigned __getReadyPrio(unsigned);

#endif /* RK_DEFS_H */
