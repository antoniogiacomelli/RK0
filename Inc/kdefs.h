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

#include <kenv.h>

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
#define FALSE (unsigned)(0U)
#define TRUE  (unsigned)(1U)
#define bool
#else
typedef _Bool BOOL;
#define TRUE		true
#define FALSE	    false
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


/* C Primitives as Kernel Type Aliases */

typedef unsigned char RK_PID;/* System defined Task ID type */
typedef unsigned char RK_PRIO;/* Task priority type */
typedef long          RK_TICK;/* Tick count type */

/* Function pointers */

typedef void (*RK_TASKENTRY)( void*);/* Task entry function pointer */
typedef void (*RK_TIMER_CALLOUT)( void*);/* Callout (timers)             */

/* Max and Min Values for C Primitives  */

/* maximum usigned =  N-bit number 2^N - 1
   maximum signed  =  N-bit number 2^(N-1) - 1 */

#define RK_PRIO_TYPE_MAX ((1ULL << (8 * sizeof(BYTE))) - 1)
#define RK_INT_MAX       ((1ULL << ((8 * sizeof(LONG)) - 1)) - 1)
#define RK_UINT_MAX      ((1ULL << (8 * sizeof(ULONG))) - 1)
#define RK_ULONG_MAX     ((1ULL << (8 * sizeof(ULONG))) - 1)
#define RK_LONG_MAX      ((1ULL << ((8 * sizeof(LONG)) - 1)) - 1)
#define RK_TICK_TYPE_MAX RK_LONG_MAX

/* KERNEL SERVICES */

/* Timeout options */
#define RK_WAIT_FOREVER      ((LONG)0xFFFFFFFF)
#define RK_NO_WAIT           ((LONG)0)


#define RK_BLOCKING_TIMEOUT  ((ULONG)1)
#define RK_ELAPSING_TIMEOUT  ((ULONG)2)
#define RK_TIMER_TIMEOUT     ((ULONG)3)
#define RK_SLEEP_TIMEOUT     ((ULONG)4)
#define RK_INVALID_TIMEOUT   ((ULONG)0)

/* Task Flags */
#define RK_FLAGS_OR             ((ULONG)1)
#define RK_FLAGS_AND            ((ULONG)2)
#define RK_FLAGS_ANY            ((ULONG)4)
#define RK_FLAGS_ALL            ((ULONG)8)
#define RK_FLAGS_ALL_KEEP       ((ULONG)16)
#define RK_FLAGS_ANY_KEEP       ((ULONG)32)

#define RK_FLAGS_ANY_CONSUME    RK_FLAGS_ANY
#define RK_FLAGS_ALL_CONSUME    RK_FLAGS_ALL

/* System Task Signals */

#define RK_SIG_EVENTGROUP       ((ULONG)0x01)
#define RK_SIG_TIMER            ((ULONG)0x02)

/* Kernel Return Values */

typedef LONG RK_ERR;

#define RK_SUCCESS                   ((LONG)0L)
/* Generic error (-1) */
#define RK_ERROR                     ((LONG)0xFFFFFFFF)

/* Non-service specific errors (-100) */
#define RK_ERR_OBJ_NULL              ((LONG)0xFFFFFF9C)
#define RK_ERR_OBJ_NOT_INIT          ((LONG)0xFFFFFF9B)
#define RK_ERR_LIST_EMPTY            ((LONG)0xFFFFFF9A)
#define RK_ERR_EMPTY_WAITING_QUEUE   ((LONG)0xFFFFFF99)
#define RK_ERR_READY_QUEUE           ((LONG)0xFFFFFF98)
#define RK_ERR_INVALID_PRIO          ((LONG)0xFFFFFF97)
#define RK_ERR_OVERFLOW              ((LONG)0xFFFFFF96)
#define RK_ERR_KERNEL_VERSION        ((LONG)0xFFFFFF95)
#define RK_ERR_TIMEOUT               ((LONG)0xFFFFFF94)
#define RK_ERR_INVALID_TIMEOUT       ((LONG)0xFFFFFF93)
#define RK_ERR_TASK_INVALID_ST       ((LONG)0xFFFFFF92)
#define RK_ERR_INVALID_ISR_PRIMITIVE ((LONG)0xFFFFFF91)
#define RK_ERR_INVALID_PARAM         ((LONG)0xFFFFFF90)

/* Memory Pool Service errors (-200)*/
#define RK_ERR_MEM_FREE              ((LONG)0xFFFFFF38)
#define RK_ERR_MEM_INIT              ((LONG)0xFFFFFF37)

/* Synchronisation Services errors (-300) */
#define RK_ERR_MUTEX_REC_LOCK        ((LONG)0xFFFFFED4)
#define RK_ERR_MUTEX_NOT_OWNER       ((LONG)0xFFFFFED3)
#define RK_ERR_MUTEX_LOCKED          ((LONG)0xFFFFFED2)
#define RK_ERR_MUTEX_NOT_LOCKED      ((LONG)0xFFFFFED1)
#define RK_ERR_FLAGS_NOT_MET         ((LONG)0xFFFFFED0)
#define RK_ERR_BLOCKED_SEMA          ((LONG)0xFFFFFECF)

/* Message Passing Services (-400) */
#define RK_ERR_INVALID_QUEUE_SIZE    ((LONG)0xFFFFFE70)
#define RK_ERR_INVALID_MESG_SIZE     ((LONG)0xFFFFFE69)
#define RK_ERR_MBOX_FULL             ((LONG)0xFFFFFE68)
#define RK_ERR_MBOX_EMPTY            ((LONG)0xFFFFFE67)
#define RK_ERR_STREAM_FULL           ((LONG)0xFFFFFE66)
#define RK_ERR_STREAM_EMPTY          ((LONG)0xFFFFFE65)
#define RK_ERR_QUEUE_FULL            ((LONG)0xFFFFFE64)
#define RK_ERR_QUEUE_EMPTY           ((LONG)0xFFFFFE63)
#define RK_ERR_INVALID_RECEIVER      ((LONG)0xFFFFFE62)
#define RK_ERR_PORT_OWNER            ((LONG)0xFFFFFE61)

/* Faults */
typedef LONG RK_FAULT;

#define RK_GENERIC_FAULT                 RK_ERROR
#define RK_FAULT_READY_QUEUE             RK_ERR_READY_QUEUE
#define RK_FAULT_OBJ_NULL                RK_ERR_OBJ_NULL
#define RK_FAULT_KERNEL_VERSION          RK_ERR_KERNEL_VERSION
#define RK_FAULT_OBJ_NOT_INIT            RK_ERR_OBJ_NOT_INIT
#define RK_FAULT_TASK_INVALID_PRIO       RK_ERR_INVALID_PRIO
#define RK_FAULT_UNLOCK_OWNED_MUTEX      RK_ERR_MUTEX_NOT_OWNER
#define RK_FAULT_INVALID_ISR_PRIMITIVE   RK_ERR_INVALID_ISR_PRIMITIVE
#define RK_FAULT_TASK_INVALID_STATE      RK_ERR_TASK_INVALID_ST

/* Task Status */

typedef ULONG RK_TASK_STATUS;

#define RK_INVALID_TASK_STATE     ((ULONG)0x00)
#define RK_READY                  ((ULONG)0x10)
#define RK_RUNNING                ((ULONG)0x20)
#define RK_PENDING                ((ULONG)0x30)
#define RK_PENDING_EV_FLAGS       ((ULONG)0x31)
#define RK_PENDING_TASK_FLAGS     ((ULONG)0x32)
#define RK_SLEEPING               ((ULONG)0x40)
#define RK_BLOCKED                ((ULONG)0x41)
#define RK_SENDING                ((ULONG)0x42)
#define RK_RECEIVING              ((ULONG)0x43)


/* Kernel Objects ID */
typedef ULONG RK_KOBJ_ID;

#define RK_INVALID_KOBJ           ((ULONG)0x0)

#define RK_SEMAPHORE_KOBJ_ID      ((ULONG)0x1)
#define RK_EVENT_KOBJ_ID          ((ULONG)0x2)
#define RK_MUTEX_KOBJ_ID          ((ULONG)0x3)
#define RK_MAILBOX_KOBJ_ID        ((ULONG)0x4)
#define RK_MAILQUEUE_KOBJ_ID      ((ULONG)0x5)
#define RK_STREAMQUEUE_KOBJ_ID    ((ULONG)0x6)
#define RK_CAB_KOBJ_ID            ((ULONG)0x7)
#define RK_TIMER_KOBJ_ID          ((ULONG)0x8)
#define RK_MEMALLOC_KOBJ_ID       ((ULONG)0x9)
#define RK_TASKHANDLE_KOBJ_ID     ((ULONG)0xA)
#define RK_PORT_KOBJ_ID           ((ULONG)0xB)

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
#if (RK_CONF_MRM == ON)
typedef struct kMRMBuf RK_MRM_BUF;
typedef struct kMRMMem RK_MRM;
#endif

/* Inlined and Macro Helpers */

/* Assembly Helpers */
#define _RK_DMB                          __ASM volatile ("dmb 0xF":::"memory");
#define _RK_DSB                          __ASM volatile ("dsb 0xF":::"memory");
#define _RK_ISB                          __ASM volatile ("isb 0xF":::"memory");
#define _RK_NOP                          __ASM volatile ("nop");
#define _RK_STUP                         __ASM volatile("svc #0xAA");

/* Processor Core Management  */

#define RK_CR_AREA  UINT crState_;
#define RK_CR_ENTER crState_ = kEnterCR();
#define RK_CR_EXIT  kExitCR(crState_);
#define RK_PEND_CTXTSWTCH RK_TRAP_PENDSV
#define RK_READY_HIGHER_PRIO(ptr) ((ptr->priority < nextTaskPrio) ? 1 : 0)
#define RK_TRAP_PENDSV  \
     RK_CORE_SCB->ICSR |= (1<<28U); \
    _RK_DSB \
    _RK_ISB

#define RK_TRAP_SVC(N)  \
    do { asm volatile ("svc %0" :: "i" (N)); } while(0U)

#define RK_TICK_EN  RK_CORE_SYSTICK->CTRL |= 0xFFFFFFFF;
#define RK_TICK_DIS RK_CORE_SYSTICK->CTRL &= 0xFFFFFFFE;

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


__attribute__((always_inline)) static inline
unsigned kIsISR( void)
{
    unsigned ipsr_value;
    __ASM ("MRS %0, IPSR" : "=r"(ipsr_value));
    __ASM volatile ("dmb 0xF":::"memory");
    return (ipsr_value);
}


#endif
