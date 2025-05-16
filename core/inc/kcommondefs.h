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
 * 	In this header:
 * 					o Common system defines 
 *
 ******************************************************************************/

#ifndef RK_COMMONDEFS_H
#define RK_COMMONDEFS_H

#include <kenv.h>
/* C PROGRAMMING PRIMITIVES */
/* for user application, stddint types can be used, as
this lib is always included in kenv.h */
typedef void VOID;
typedef char CHAR;
typedef unsigned char BYTE;
typedef signed INT;/* stack type */
typedef unsigned UINT;
typedef unsigned long ULONG;
typedef long LONG;

/* if no stdbool.h */
#if !defined(bool)
typedef unsigned BOOL;
#define FALSE (unsigned)(0)
#define TRUE  (unsigned)(1)
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
#define RK_TIMHANDLER_ID       ((RK_PID)(0xFF))
#define RK_IDLETASK_ID         ((RK_PID)(0x00))
#define RK_N_SYSTASKS          2U /*idle task + tim handler*/
#define RK_NTHREADS            (RK_CONF_N_USRTASKS + RK_N_SYSTASKS)
#define RK_NPRIO               (RK_CONF_MIN_PRIO + 1U)
#ifdef  RK_SYSTEMCORECLOCK
#define RK_TICK_10MS           ((RK_SYSTEMCORECLOCK)/100U)  /*  Tick period of 10ms */
#define RK_TICK_5MS            ((RK_SYSTEMCORECLOCK)/200U)  /* Tick period of 5ms */
#define RK_TICK_1MS            ((RK_SYSTEMCORECLOCK)/1000U) /*  Tick period of 1ms */
#endif


/* Kernel Types Aliases */
typedef BYTE          RK_PID; 
typedef BYTE          RK_PRIO; 
typedef INT           RK_TICK; 
typedef INT           RK_ERR;
typedef UINT          RK_TASK_STATUS;
typedef INT           RK_FAULT;
typedef UINT          RK_KOBJ_ID;

/* Function pointers */
typedef void (*RK_TASKENTRY)( void*);/* Task entry function pointer */
typedef void (*RK_TIMER_CALLOUT)( void*);/* Callout (timers)             */

/* Max and Min Values for C Primitives  */

/* maximum usigned =  N-bit number 2^N - 1
   maximum signed  =  N-bit number 2^(N-1) - 1 */

#define RK_PRIO_TYPE_MAX ((1UL << (8UL * sizeof(RK_PRIO))) - 1UL)
#define RK_INT_MAX       ((1UL << ((8UL * sizeof(INT)) - 1UL)) - 1UL)
#define RK_UINT_MAX      ((1UL << (8UL * sizeof(UINT))) - 1UL)
#define RK_ULONG_MAX     ((1UL << (8UL * sizeof(ULONG))) - 1UL)
#define RK_LONG_MAX      ((1UL << ((8UL * sizeof(LONG)) - 1UL)) - 1UL)
#define RK_TICK_TYPE_MAX  ((1UL << ((8UL * sizeof(RK_TICK)) - 1UL)) - 1UL)

/* KERNEL SERVICES */

/* Task Run-to-completion */
#define RK_PREEMPT          0U
#define RK_NO_PREEMPT       1U

/* Timeout options */
#define RK_WAIT_FOREVER      ((RK_TICK)0xFFFFFFFF)
#define RK_NO_WAIT           ((RK_TICK)0x0)

/* Timeout code */
#define RK_TIMEOUT_BLOCKING  ((UINT)0x1)
#define RK_TIMEOUT_ELAPSING  ((UINT)0x2)
#define RK_TIMEOUT_TIMER     ((UINT)0x3)
#define RK_TIMEOUT_SLEEP     ((UINT)0x4)
#define RK_SLEEP_TIMEOUT     ((UINT)0x0)

/* Task Flags Options */
#define RK_FLAGS_OR             ((UINT)0x1)
#define RK_FLAGS_ANY            ((UINT)0x4)
#define RK_FLAGS_ALL            ((UINT)0x8)

/* System Task Signals */
#define RK_SIG_TIMER            ((ULONG)0x2)


/* Mutex Priority Inh */
#define RK_NO_INHERIT           ((UINT)0)
#define RK_INHERIT              ((UINT)1)


/* Kernel Return Values */

#define RK_SUCCESS                   ((RK_ERR)0x0)
/* Generic error (-1) */
#define RK_ERROR                     ((RK_ERR)0xFFFFFFFF)

/* Non-service specific errors (-100) */
#define RK_ERR_OBJ_NULL              ((RK_ERR)0xFFFFFF9C)
#define RK_ERR_OBJ_NOT_INIT          ((RK_ERR)0xFFFFFF9B)
#define RK_ERR_LIST_EMPTY            ((RK_ERR)0xFFFFFF9A)
#define RK_ERR_EMPTY_WAITING_QUEUE   ((RK_ERR)0xFFFFFF99)
#define RK_ERR_READY_QUEUE           ((RK_ERR)0xFFFFFF98)
#define RK_ERR_INVALID_PRIO          ((RK_ERR)0xFFFFFF97)
#define RK_ERR_OVERFLOW              ((RK_ERR)0xFFFFFF96)
#define RK_ERR_KERNEL_VERSION        ((RK_ERR)0xFFFFFF95)
#define RK_ERR_TIMEOUT               ((RK_ERR)0xFFFFFF94)
#define RK_ERR_INVALID_TIMEOUT       ((RK_ERR)0xFFFFFF93)
#define RK_ERR_TASK_INVALID_ST       ((RK_ERR)0xFFFFFF92)
#define RK_ERR_INVALID_ISR_PRIMITIVE ((RK_ERR)0xFFFFFF91)
#define RK_ERR_INVALID_PARAM         ((RK_ERR)0xFFFFFF90)

/* Memory Pool Service errors (-200)*/
#define RK_ERR_MEM_FREE              ((RK_ERR)0xFFFFFF38)
#define RK_ERR_MEM_INIT              ((RK_ERR)0xFFFFFF37)

/* Synchronisation Services errors (-300) */
#define RK_ERR_MUTEX_REC_LOCK        ((RK_ERR)0xFFFFFED4)
#define RK_ERR_MUTEX_NOT_OWNER       ((RK_ERR)0xFFFFFED3)
#define RK_ERR_MUTEX_LOCKED          ((RK_ERR)0xFFFFFED2)
#define RK_ERR_MUTEX_NOT_LOCKED      ((RK_ERR)0xFFFFFED1)
#define RK_ERR_FLAGS_NOT_MET         ((RK_ERR)0xFFFFFED0)
#define RK_ERR_BLOCKED_SEMA          ((RK_ERR)0xFFFFFECF)

/* Message Passing Services (-400) */
#define RK_ERR_INVALID_QUEUE_SIZE    ((RK_ERR)0xFFFFFE70)
#define RK_ERR_INVALID_MESG_SIZE     ((RK_ERR)0xFFFFFE69)
#define RK_ERR_MBOX_FULL             ((RK_ERR)0xFFFFFE68)
#define RK_ERR_MBOX_EMPTY            ((RK_ERR)0xFFFFFE67)
#define RK_ERR_STREAM_FULL           ((RK_ERR)0xFFFFFE66)
#define RK_ERR_STREAM_EMPTY          ((RK_ERR)0xFFFFFE65)
#define RK_ERR_QUEUE_FULL            ((RK_ERR)0xFFFFFE64)
#define RK_ERR_QUEUE_EMPTY           ((RK_ERR)0xFFFFFE63)
#define RK_ERR_INVALID_RECEIVER      ((RK_ERR)0xFFFFFE62)
#define RK_ERR_PORT_OWNER            ((RK_ERR)0xFFFFFE61)

/* Faults */

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


#define RK_INVALID_TASK_STATE     ((RK_TASK_STATUS)0x00)
#define RK_READY                  ((RK_TASK_STATUS)0x10)
#define RK_RUNNING                ((RK_TASK_STATUS)0x20)
#define RK_SLEEPING               ((RK_TASK_STATUS)0x30)
#define RK_PENDING                ((RK_TASK_STATUS)(RK_SLEEPING + 1U))
#define RK_BLOCKED                ((RK_TASK_STATUS)(RK_SLEEPING + 2U))
#define RK_SENDING                ((RK_TASK_STATUS)(RK_SLEEPING + 3U))
#define RK_RECEIVING              ((RK_TASK_STATUS)(RK_SLEEPING + 4U))


/* Kernel Objects ID */

#define RK_INVALID_KOBJ           ((RK_KOBJ_ID)0x0)

#define RK_SEMAPHORE_KOBJ_ID      ((RK_KOBJ_ID)0x1)
#define RK_EVENT_KOBJ_ID          ((RK_KOBJ_ID)0x2)
#define RK_MUTEX_KOBJ_ID          ((RK_KOBJ_ID)0x3)
#define RK_MAILBOX_KOBJ_ID        ((RK_KOBJ_ID)0x4)
#define RK_MAILQUEUE_KOBJ_ID      ((RK_KOBJ_ID)0x5)
#define RK_STREAMQUEUE_KOBJ_ID    ((RK_KOBJ_ID)0x6)
#define RK_MRM_KOBJ_ID            ((RK_KOBJ_ID)0x7)
#define RK_TIMER_KOBJ_ID          ((RK_KOBJ_ID)0x8)
#define RK_MEMALLOC_KOBJ_ID       ((RK_KOBJ_ID)0x9)
#define RK_TASKHANDLE_KOBJ_ID     ((RK_KOBJ_ID)0xA)

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

/* Misc Helpers */
#define K_ERR_HANDLER                kErrHandler

#define K_IS_BLOCK_ON_ISR(timeout) ((kIsISR() && (timeout > 0)) ? (1U) : (0))

#if (defined(STDDEF_H_) || defined(_STDDEF_H_) || defined (__STDEF_H__))

#define K_GET_CONTAINER_ADDR(memberPtr, containerType, memberName) \
    ((containerType *)((unsigned char *)(memberPtr) - \
     offsetof(containerType, memberName)))
#else
#    error "Need stddef.h for offsetof()"

#endif

#define RK_NO_ARGS (NULL)

#define RK_UNUSEARGS (void)(args);

#ifdef NDEBUG
#define kassert(x) ((void)0)
#else
#define kassert(x) ((x) ? (void)0 : K_ERR_HANDLER(0))
#endif

/* GNU GCC Attributes*/
#ifdef __GNUC__

#ifndef __K_ALIGN
#define __K_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifndef __RK_WEAK
#define __RK_WEAK     __attribute__((weak))
#endif

#ifndef __RK_INLINE
#define __RK_INLINE   __attribute__((always_inline))
#endif

#endif /* __GNUC__*/


#endif /* RK_COMMONDEFS_H */