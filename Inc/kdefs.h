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
 * 	In this header:
 * 					o System defines: code values, typedefs, macro helpers
 *
 ******************************************************************************/

#ifndef RK_DEFS_H
#define RK_DEFS_H

#include "kenv.h"

/* C PROGRAMMING PRIMITIVES */

typedef void VOID;
typedef char CHAR;
typedef unsigned char BYTE;
typedef signed INT;/* stack type */
typedef unsigned UINT;
typedef unsigned long ULONG;
typedef long LONG;
typedef void *ADDR;/* Generic address type */

/* if no stdbool.h */
#if !defined(bool)
typedef unsigned BOOL;
#define RK_FALSE (unsigned)(0U)
#define RK_TRUE  (unsigned)(1U)
#define bool
#else
typedef _Bool BOOL;
#define RK_TRUE			true
#define RK_FALSE	    alse
#endif

/* Task Initialisation Defines: these values are all subtracted from the
     top of the stack */
#define PSR_OFFSET  1 /* Program Status Register offset */
#define PC_OFFSET   2 /* Program Counter offset */
#define LR_OFFSET   3 /* Link Register offset */
#define R12_OFFSET  4 /* R12 Register offset */
#define R3_OFFSET   5 /* R3 Register offset */
#define R2_OFFSET   6 /* R2 Register offset */
#define R1_OFFSET   7 /* R1 Register offset */
#define R0_OFFSET   8 /* R0 Register offset */
#define R11_OFFSET  9 /* R11 Register offset */
#define R10_OFFSET  10 /* R10 Register offset */
#define R9_OFFSET   11 /* R9 Register offset */
#define R8_OFFSET   12 /* R8 Register offset */
#define R7_OFFSET   13 /* R7 Register offset */
#define R6_OFFSET   14 /* R6 Register offset */
#define R5_OFFSET   15 /* R5 Register offset */
#define R4_OFFSET   16 /* R4 Register offset */

/*** Configuration Defines for kconfig.h ***/
#define RK_TIMHANDLER_ID       255
#define RK_IDLETASK_ID         0
#define RK_N_SYSTASKS          2 /*idle task + tim handler*/
#define RK_NTHREADS            (RK_CONF_N_USRTASKS + RK_N_SYSTASKS)
#define RK_NPRIO               (RK_CONF_MIN_PRIO + 1)
#ifdef  RK_SYSTEMCORECLOCK
#define RK_TICK_10MS           ((RK_SYSTEMCORECLOCK)/100)  /*  Tick period of 10ms */
#define RK_TICK_5MS            ((RK_SYSTEMCORECLOCK)/200)  /* Tick period of 5ms */
#define RK_TICK_1MS            ((RK_SYSTEMCORECLOCK)/1000) /*  Tick period of 1ms */
#endif
/* Assembly Helpers */
#define _RK_DMB                          asm volatile ("dmb 0xF":::"memory");
#define _RK_DSB                          asm volatile ("dsb 0xF":::"memory");
#define _RK_ISB                          asm volatile ("isb 0xF":::"memory");
#define _RK_NOP                          asm volatile ("nop");
#define _RK_STUP                         asm volatile("svc #0xAA");

/* Processor Core Management Helpers */
#define RK_CR_AREA  UINT crState_;
#define RK_CR_ENTER crState_ = kEnterCR();
#define RK_CR_EXIT  kExitCR(crState_);
#define RK_PEND_CTXTSWTCH RK_TRAP_PENDSV
#define RK_READY_HIGHER_PRIO(ptr) ((ptr->priority < nextTaskPrio) ? 1 : 0)
#define RK_TRAP_PENDSV  \
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; \
    _RK_DSB \
    _RK_ISB

#define RK_TRAP_SVC(N)  \
    do { asm volatile ("svc %0" :: "i" (N)); } while(0U)

#define RK_TICK_EN  SysTick->CTRL |= 0xFFFFFFFF;
#define RK_TICK_DIS SysTick->CTRL &= 0xFFFFFFFE;

/* Misc Helpers */

#define KERR                kErrHandler

#define RK_IS_BLOCK_ON_ISR(timeout) ((kIsISR() && (timeout > 0)) ? (1) : (0))

#define RK_GET_CONTAINER_ADDR(memberPtr, containerType, memberName) \
    ((containerType *)((unsigned char *)(memberPtr) - \
     offsetof(containerType, memberName)))

#define RK_IS_NULL_PTR(ptr) ((ptr) == NULL ? 1 : 0)

#define RK_NOARGS (NULL)

#define RK_UNUSEARGS (void)(args);

#ifdef NDEBUG
#define kassert(x) ((void)0)
#else
#define kassert(x) ((x) ? (void)0 : KERR(0))
#endif

__STATIC_FORCEINLINE unsigned kIsISR( void)
{
    unsigned ipsr_value;
    asm("MRS %0, IPSR" : "=r"(ipsr_value));
    _RK_DMB
    return (ipsr_value);
}

/* C Primitives as Kernel Type Aliases */

typedef unsigned char RK_PID;/* System defined Task ID type */
typedef unsigned char RK_PRIO;/* Task priority type */
typedef unsigned long RK_TICK;/* Tick count type */

/* Function pointers */

typedef void (*RK_TASKENTRY)( void*);/* Task entry function pointer */
typedef void (*RK_TIMER_CALLOUT)( void*);/* Callout (timers)             */

/* Max and Min Values for C Primitives  */

/* maximum usigned =  N-bit number 2^N - 1
   maximum signed  =  N-bit number 2^(N-1) - 1 */

#define RK_TICK_TYPE_MAX ((1ULL << (8 * sizeof(ULONG))) - 1)
#define RK_PRIO_TYPE_MAX ((1ULL << (8 * sizeof(BYTE))) - 1)
#define RK_INT_MAX       ((1ULL << ((8 * sizeof(INT)) - 1)) - 1)
#define RK_UINT_MAX      ((1ULL << (8 * sizeof(UINT))) - 1)
#define RK_ULONG_MAX     ((1ULL << (8 * sizeof(ULONG))) - 1)
#define RK_LONG_MAX      ((1ULL << ((8 * sizeof(LONG)) - 1)) - 1)

/* KERNEL SERVICES */

/* Timeout options */
#define RK_WAIT_FOREVER      ((ULONG)0xFFFFFFFF)
#define RK_NO_WAIT           ((ULONG)0)

/* Timeout Types */

#define RK_BLOCKING_TIMEOUT  ((UINT)1)
#define RK_ELAPSING_TIMEOUT  ((UINT)2)
#define RK_TIMER_TIMEOUT     ((UINT)3)
#define RK_INVALID_TIMEOUT   ((UINT)0)

/* Event Flags Options */
/* Get Options */
#define RK_FLAGS_ALL_KEEP     ((ULONG)1)
#define RK_FLAGS_ANY_KEEP     ((ULONG)2)
#define RK_FLAGS_ALL_CONSUME  ((ULONG)4)
#define RK_FLAGS_ANY_CONSUME  ((ULONG)8)
#define RK_FLAGS_ALL_CLEAR    (RK_FLAGS_ALL_CONSUME)
#define RK_FLAGS_ANY_CLEAR    (RK_FLAGS_ANY_CONSUME)
/* Set Options */
#define RK_FLAGS_OR           ((ULONG)1)
#define RK_FLAGS_AND          ((ULONG)2)
#define RK_FLAGS_OVW          ((ULONG)4)

#define RK_SIGNALS_OR           ((ULONG)1)
#define RK_SIGNALS_OVW          ((ULONG)3)

/* Kernel Return Values */

typedef INT RK_ERR;

/* Success / Informational codes (non-faulty, positive values) */
#define RK_SUCCESS                   ((INT)0)       /* No Error */
#define RK_ERR_TIMEOUT               ((INT)0x1)
#define RK_QUERY_MBOX_EMPTY          ((INT)0x2)
#define RK_QUERY_MBOX_FULL           ((INT)0x3)
#define RK_QUERY_MUTEX_LOCKED        ((INT)0x4)
#define RK_QUERY_MUTEX_UNLOCKED      ((INT)0x5)
#define RK_ERR_MBOX_FULL             ((INT)0x6)
#define RK_ERR_MBOX_SIZE             ((INT)0x7)
#define RK_ERR_MBOX_EMPTY            ((INT)0x8)
#define RK_ERR_MBOX_ISR              ((INT)0x9)
#define RK_ERR_MBOX_NO_WAITERS       ((INT)0xA)
#define RK_ERR_STREAM_FULL           ((INT)0xB)
#define RK_ERR_STREAM_EMPTY          ((INT)0xC)
#define RK_ERR_MUTEX_LOCKED          ((INT)0xD)
#define RK_ERR_MUTEX_NOT_LOCKED      ((INT)0xE)
#define RK_ERR_INVALID_PARAM         ((INT)0xF)
#define RK_ERR_EMPTY_WAITING_QUEUE   ((INT)0x10)
#define RK_ERR_SCH_LOCKED            ((INT)0x11)

/* Faulty (negative) return values */
#define RK_ERROR                     ((INT)0xFFFFFFFF)
#define RK_ERR_OBJ_NULL              ((INT)0xFFFFFFFE)
#define RK_ERR_OBJ_NOT_INIT          ((INT)0xFFFFFFFD)
#define RK_ERR_LIST_ITEM_NOT_FOUND   ((INT)0xFFFFFFFC)
#define RK_ERR_LIST_EMPTY            ((INT)0xFFFFFFFB)
#define RK_ERR_MEM_INIT              ((INT)0xFFFFFFFA)
#define RK_ERR_MEM_FREE              ((INT)0xFFFFFFF9)
#define RK_ERR_MEM_ALLOC             ((INT)0xFFFFFFF8)
#define RK_ERR_TIMER_POOL_EMPTY      ((INT)0xFFFFFFF7)
#define RK_ERR_READY_QUEUE           ((INT)0xFFFFFFF6)
#define RK_ERR_INVALID_PRIO          ((INT)0xFFFFFFF5)
#define RK_ERR_INVALID_QUEUE_SIZE    ((INT)0xFFFFFFF4)
#define RK_ERR_INVALID_MESG_SIZE     ((INT)0xFFFFFFF3)
#define RK_ERR_MESG_CPY              ((INT)0xFFFFFFF2)
#define RK_ERR_PDBUF_SIZE            ((INT)0xFFFFFFF1)
#define RK_ERR_SEM_INVALID_VAL       ((INT)0xFFFFFFF0)
#define RK_ERR_INVALID_TSLICE        ((INT)0xFFFFFFEF)
#define RK_ERR_KERNEL_VERSION        ((INT)0xFFFFFFEE)
#define RK_ERR_MBOX_INIT_MAIL        ((INT)0xFFFFFFED)
#define RK_ERR_MUTEX_REC_LOCK        ((INT)0xFFFFFFEC)
#define RK_ERR_MUTEX_NOT_OWNER       ((INT)0xFFFFFFEB)
#define RK_ERR_TASK_INVALID_ST       ((INT)0xFFFFFFEA)
#define RK_ERR_INVALID_ISR_PRIMITIVE ((INT)0xFFFFFFE9)
#define RK_ERR_OVERFLOW              ((INT)0xFFFFFFE8)
#define RK_ERR_PORT_OWNER            ((INT)0xFFFFFFE7)
#define RK_ERR_INVALID_RECEIVER      ((INT)0xFFFFFFE6)

/* Fault codes */

typedef INT RK_FAULT;

#define RK_GENERIC_FAULT                 RK_ERROR
#define RK_FAULT_READY_QUEUE             RK_ERR_READY_QUEUE
#define RK_FAULT_OBJ_NULL                RK_ERR_OBJ_NULL
#define RK_FAULT_KERNEL_VERSION          RK_ERR_KERNEL_VERSION
#define RK_FAULT_OBJ_NOT_INIT            RK_ERR_OBJ_NOT_INIT
#define RK_FAULT_TASK_INVALID_PRIO       RK_ERR_INVALID_PRIO
#define RK_FAULT_UNLOCK_OWNED_MUTEX      RK_ERR_MUTEX_NOT_OWNER
#define RK_FAULT_INVALID_ISR_PRIMITIVE   RK_ERR_INVALID_ISR_PRIMITIVE
#define RK_FAULT_TASK_INVALID_STATE      RK_ERR_TASK_INVALID_ST
#define RK_FAULT_TASK_INVALID_TSLICE     RK_ERR_INVALID_TSLICE

/* Task Status */

typedef UINT RK_TASK_STATUS;

#define RK_INVALID_TASK_STATE ((UINT)0)
#define RK_READY              ((UINT)1)
#define RK_RUNNING            ((UINT)2)
#define RK_PENDING            ((UINT)3)
#define RK_SLEEPING           ((UINT)4)
#define RK_BLOCKED            ((UINT)5)
#define RK_SENDING            ((UINT)6)
#define RK_RECEIVING          ((UINT)7)

/* Kernel Objects ID */
typedef UINT RK_KOBJ_ID;

#define RK_INVALID_KOBJ           ((UINT)0x0)

#define RK_SEMAPHORE_KOBJ_ID      ((UINT)0x1)
#define RK_EVENT_KOBJ_ID          ((UINT)0x2)
#define RK_MUTEX_KOBJ_ID          ((UINT)0x3)
#define RK_MAILBOX_KOBJ_ID        ((UINT)0x4)
#define RK_MAILQUEUE_KOBJ_ID      ((UINT)0x5)
#define RK_STREAMQUEUE_KOBJ_ID    ((UINT)0x6)
#define RK_CAB_KOBJ_ID            ((UINT)0x7)
#define RK_TIMER_KOBJ_ID          ((UINT)0x8)
#define RK_MEMALLOC_KOBJ_ID       ((UINT)0x9)
#define RK_TASKHANDLE_KOBJ_ID     ((UINT)0xA)
#define RK_PORT_KOBJ_ID           ((UINT)0xB)

/* Kernel Objects Typedefs */

/* Any typedef _HANDLE is a pointer to an object */

typedef struct kTcb RK_TCB;/* Task Control Block */
typedef struct kMemBlock RK_MEM;/* Mem Alloc Control Block */
typedef struct kList RK_LIST;/* DList ADT used for TCBs */
typedef struct kListNode RK_NODE;/* DList Node */
typedef RK_LIST RK_TCBQ;/* Alias for RK_LIST for readability */
typedef struct kTcb *RK_TASK_HANDLE;/* Pointer to TCB is a Task Handle */
typedef struct kTimeoutNode RK_TIMEOUT_NODE;/* Node for time-out delta list */
#if (RK_CONF_CALLOUT_TIMER==ON)
typedef struct kTimer RK_TIMER;
#endif
#if (RK_CONF_EVENT==ON)
typedef struct kEvent RK_EVENT;
#endif
#if (RK_CONF_SEMA == ON)
typedef struct kSema RK_SEMA;
#endif
#if (RK_CONF_MUTEX == ON)
typedef struct kMutex RK_MUTEX;
#endif
#if (RK_CONF_MBOX == ON)
typedef struct kMailbox RK_MBOX;
#endif
#if (RK_CONF_QUEUE==ON)
typedef struct kQ RK_QUEUE;
#endif
#if (RK_CONF_STREAM == ON)
typedef struct kStream RK_STREAM;
#endif
#if (RK_CONF_CAB == ON)
typedef struct kMRMBuf RK_MRM_BUF;
typedef struct kMRMMem RK_MRM;
#endif

#endif
