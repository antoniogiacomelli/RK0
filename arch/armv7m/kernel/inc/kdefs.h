#ifndef RK_DEFS_H
#define RK_DEFS_H
#include <kservices.h>

/* Assembly Helpers */
#define _RK_DMB                          __ASM volatile ("dmb 0xF":::"memory");
#define _RK_DSB                          __ASM volatile ("dsb 0xF":::"memory");
#define _RK_ISB                          __ASM volatile ("isb 0xF":::"memory");
#define _RK_NOP                          __ASM volatile ("nop");
#define _RK_STUP                         __ASM volatile("svc #0xAA");

/* Processor Core Management  */

#define RK_CR_AREA  volatile UINT crState_;
#define RK_CR_ENTER crState_ = kEnterCR();
#define RK_CR_EXIT  kExitCR(crState_);
#define RK_PEND_CTXTSWTCH RK_TRAP_PENDSV
#define RK_READY_HIGHER_PRIO(ptr) ((ptr->priority < nextTaskPrio) ? 1U : 0)
#define RK_TRAP_PENDSV  \
     RK_CORE_SCB->ICSR |= (1<<28U); \
    _RK_DSB \
    _RK_ISB

#define RK_TRAP_SVC(N)  \
    do { asm volatile ("svc %0" :: "i" (N)); } while(0)

#define RK_TICK_EN  RK_CORE_SYSTICK->CTRL |= 0xFFFFFFFF;
#define RK_TICK_DIS RK_CORE_SYSTICK->CTRL &= 0xFFFFFFFE;


__attribute__((always_inline)) static inline
unsigned kIsISR( void)
{
    unsigned ipsr_value;
    __ASM ("MRS %0, IPSR" : "=r"(ipsr_value));
    __ASM volatile ("dmb 0xF":::"memory");
    return (ipsr_value);
}


#endif
