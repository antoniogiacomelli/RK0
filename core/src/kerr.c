/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.9                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/*******************************************************************************
 * 	COMPONENT    : ERROR CHECKER
 * 	DEPENDS ON   : LOW-LEVEL SCHEDULER, TIMER
 * 	PROVIDES TO  : ALL
 *  PUBLIC API	 : N/A
 ******************************************************************************/

 #define RK_SOURCE_CODE

#include <kerr.h>

/*** Compile time errors ****/
#if defined(__ARM_ARCH_7EM__) /* Cortex-M4 / M7 */
#define ARCH_CM_7EM 1
#elif defined(__ARM_ARCH_7M__) /* Cortex-M3       */
#define ARCH_CM_7M 1
#elif defined(__ARM_ARCH_6M__) /* Cortex-M0/M0+/ */
#define ARCH_CM_6M 1
#else
#error "Unsupported Cortex-M architecture—check your -mcpu/-march"
#endif

#ifndef __GNUC__
#error "You need GCC as your compiler!"
#endif

#ifndef __CMSIS_GCC_H
#error "You need CMSIS-GCC !"
#endif

#ifndef RK_VALID_VERSION
#error "Missing RK0 version"
#endif

#if (RK_CONF_MIN_PRIO > 31)
#error "Invalid minimal effective priority. (Max numerical value: 31)"
#endif

#if defined(QEMU_MACHINE)
#if (RK_CONF_SYSCORECLK == 0UL)
#error "Invalid RK_CONF_SYSCORECLK for QEMU. Can't be 0."
#endif
#endif

/******************************************************************************
 * ERROR HANDLING
 ******************************************************************************/
#if (RK_CONF_FAULT == ON)
volatile RK_FAULT RK_gFaultID = 0;
volatile struct traceItem RK_gTraceInfo = {0};
#define RK_FAULT_LIST(F) \
   F(RK_GENERIC_FAULT) \
   F(RK_FAULT_READY_QUEUE) \
   F(RK_FAULT_OBJ_NULL) \
   F(RK_FAULT_OBJ_NOT_INIT) \
   F(RK_FAULT_OBJ_DOUBLE_INIT) \
   F(RK_FAULT_TASK_INVALID_PRIO) \
   F(RK_FAULT_UNLOCK_OWNED_MUTEX) \
   F(RK_FAULT_MUTEX_REC_LOCK) \
   F(RK_FAULT_MUTEX_NOT_LOCKED) \
   F(RK_FAULT_INVALID_ISR_PRIMITIVE) \
   F(RK_FAULT_TASK_INVALID_STATE) \
   F(RK_FAULT_INVALID_OBJ) \
   F(RK_FAULT_INVALID_PARAM) \
   F(RK_FAULT_INVALID_TIMEOUT) \
   F(RK_FAULT_STACK_OVERFLOW) \
   F(RK_FAULT_TASK_COUNT_MISMATCH) \
   F(RK_FAULT_KERNEL_VERSION) \
   F(RK_FAULT_APP_CRASH) \
   F(RK_FAULT_INIT_KERNEL)

typedef struct 
{
    RK_FAULT faultCode;
    const char *faultStr;
} RK_FAULT_NAME;

#define RK_FAULT_ENTRY(F) { F, #F },
static const RK_FAULT_NAME kFaultNames[] = 
{
    RK_FAULT_LIST(RK_FAULT_ENTRY)
#undef RK_FAULT_ENTRY
};

static inline const char *kStringfyFault_(RK_FAULT code)
{
    for (ULONG idx = 0; idx < sizeof(kFaultNames)/sizeof(kFaultNames[0]); ++idx) 
    {
        if (kFaultNames[idx].faultCode == code) 
        {
            return (kFaultNames[idx].faultStr);
        }
    }
    return ("RK_FAULT_UNKNOWN");
}


void kErrHandler(RK_FAULT fault) /* generic error handler */
{

    RK_CR_AREA
    RK_CR_ENTER
 
    RK_gTraceInfo.code = fault;
    RK_gFaultID = fault;
    if (RK_gRunPtr)
    {
        RK_gTraceInfo.task = RK_gRunPtr->taskName;
        RK_gTraceInfo.sp = (UINT)RK_gRunPtr->sp;
        RK_gTraceInfo.taskID = (BYTE)RK_gRunPtr->pid;
    }
    else
    {
        RK_gTraceInfo.task = 0;
        RK_gTraceInfo.sp = 0;
    }

    register unsigned lr_value;
    __asm volatile("mov %0, lr" : "=r"(lr_value));
    RK_gTraceInfo.lr = lr_value;
    RK_gTraceInfo.tick = kTickGet();
    #if (DEBUG_CONF_PRINT_ERRORS == 1)
    printf("FATAL: %04x : %s \n\r", RK_gFaultID, kStringfyFault_(RK_gFaultID));
    printf("TASK: %s\n\r", (RK_gTraceInfo.task != 0) ? RK_gTraceInfo.task : "UNKOWN");
    #endif
    
    RK_CR_EXIT

    RK_ABORT
}
#else
void kErrHandler(RK_FAULT fault)
{
    (void)fault;
    return;
}
#endif

VOID kPanic(const char* fmt, ...)
{
    asm volatile("CPSID I" : : : "memory"); 
    fprintf(stderr, "@%lums PANIC ! ", kTickGetMs()); 
    fflush(stderr); 
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    __ASM volatile ("BKPT #0"); \
    while (1) { __ASM volatile ("NOP"); }
}
