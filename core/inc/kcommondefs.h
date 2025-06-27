/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.4
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

#ifdef __cplusplus
extern "C" {
#endif


#include <kenv.h>

/* C PROGRAMMING PRIMITIVES */
/* for user application, stddint types can be used, as
this lib is always included in kenv.h */

typedef void VOID;
/* armv6/7m char is unsigned, but compiler flags  */
/* can override it */
typedef unsigned char BYTE;
typedef signed char SCHAR;
typedef char CHAR;
typedef signed INT;
typedef unsigned UINT;
typedef unsigned long ULONG;
typedef long LONG;

/* if no stdbool.h */
#if !defined(bool)
typedef unsigned BOOL;
#define FALSE (unsigned)(0)
#define TRUE (unsigned)(1)
#define bool
#else
typedef _Bool BOOL;
#define TRUE true
#define FALSE false
#endif

/* Task Initialisation Defines: these values are all subtracted from the
     top of the stack */
#define PSR_OFFSET 1  /* Program Status Register offset */
#define PC_OFFSET 2   /* Program Counter offset */
#define LR_OFFSET 3   /* Link Register offset */
#define R12_OFFSET 4  /* R12 Register offset */
#define R3_OFFSET 5   /* R3 Register offset */
#define R2_OFFSET 6   /* R2 Register offset */
#define R1_OFFSET 7   /* R1 Register offset */
#define R0_OFFSET 8   /* R0 Register offset */
#define R11_OFFSET 9  /* R11 Register offset */
#define R10_OFFSET 10 /* R10 Register offset */
#define R9_OFFSET 11  /* R9 Register offset */
#define R8_OFFSET 12  /* R8 Register offset */
#define R7_OFFSET 13  /* R7 Register offset */
#define R6_OFFSET 14  /* R6 Register offset */
#define R5_OFFSET 15  /* R5 Register offset */
#define R4_OFFSET 16  /* R4 Register offset */

/* Stack paint */
#define RK_STACK_GUARD              (0x0BADC0DEU)
#define RK_STACK_PATTERN            (0xA5A5A5A5U)

/*** Configuration Defines for kconfig.h ***/
#define RK_TIMHANDLER_ID            ((RK_PID)(0xFF))
#define RK_IDLETASK_ID              ((RK_PID)(0x00))
#define RK_N_SYSTASKS               2U /*idle task + tim handler*/
#define RK_NTHREADS                 (RK_CONF_N_USRTASKS + RK_N_SYSTASKS)
#define RK_NPRIO                    (RK_CONF_MIN_PRIO + 1U)

/* Kernel Types Aliases */
typedef BYTE RK_PID;
typedef BYTE RK_PRIO;
typedef ULONG RK_TICK;
typedef LONG RK_STICK;
typedef INT RK_ERR;
typedef UINT RK_TASK_STATUS;
typedef INT RK_FAULT;
typedef UINT RK_KOBJ_ID;
typedef UINT RK_STACK;

/* Function pointers */
typedef void (*RK_TASKENTRY)(void *);     /* Task entry function pointer */
typedef void (*RK_TIMER_CALLOUT)(void *); /* Callout (timers)             */

#ifndef UINT8_MAX
#define UINT8_MAX (0xFF) /* 255 */
#endif
#ifndef INT8_MAX
#define INT8_MAX (0x7F) /* 127 */
#endif
#ifndef UINT32_MAX
#define UINT32_MAX (0xFFFFFFFF) /* 4,294,976,295 */
#endif
#ifndef INT32_MAX
#define INT32_MAX (0x7FFFFFFF) /* 2,147,483,547 */
#endif

#define RK_PRIO_TYPE_MAX UINT8_MAX
#define RK_INT_MAX INT32_MAX
#define RK_UINT_MAX UINT32_MAX
#define RK_ULONG_MAX UINT32_MAX
#define RK_LONG_MAX INT32_MAX
#define RK_TICK_TYPE_MAX RK_ULONG_MAX

/* KERNEL SERVICES */

/* Task Preempt/Non-preempt */
#define RK_PREEMPT 1UL
#define RK_NO_PREEMPT 0UL

/* Timeout options */
#define RK_WAIT_FOREVER                     ((RK_TICK)0xFFFFFFFF)
#define RK_NO_WAIT                          ((RK_TICK)0x0)
#define RK_MAX_PERIOD                       ((RK_TICK)(~(RK_TICK)0 >> 1)) /* 0x7FFFFFFF */

/* Timeout code */
#define RK_TIMEOUT_BLOCKING                 ((UINT)0x1)
#define RK_TIMEOUT_ELAPSING                 ((UINT)0x2)
#define RK_TIMEOUT_TIMER                    ((UINT)0x3)
#define RK_TIMEOUT_SLEEP                    ((UINT)0x4)

/* Task Flags Options */
#define RK_FLAGS_ANY                        ((UINT)0x4)
#define RK_FLAGS_ALL                        ((UINT)0x8)

/* Timer Handle System Task Signals */
#define RK_SIG_TIMER                        ((ULONG)0x2)

/* Mutex Priority Inh */
#define RK_NO_INHERIT                       ((UINT)0)
#define RK_INHERIT                          ((UINT)1)

/* Semaphore Type */
#define RK_SEMA_COUNT                       ((UINT)0)
#define RK_SEMA_BIN                         ((UINT)1)
/* Semaphore max value */
#define RK_SEMA_MAX_VALUE 0xFFU

/* Kernel object name string */

#define RK_OBJ_MAX_NAME_LEN                         (8U)

/* Kernel Return Values */

#define RK_SUCCESS                          ((RK_ERR)0x0)
/* Generic error (-1) */
#define RK_ERROR                            ((RK_ERR)-1)

/* Non-service specific retval (100) */
#define RK_ERR_OBJ_NULL                     ((RK_ERR)-100)
#define RK_ERR_OBJ_NOT_INIT                 ((RK_ERR)-101)
#define RK_ERR_LIST_EMPTY                   ((RK_ERR)102)
#define RK_ERR_EMPTY_WAITING_QUEUE          ((RK_ERR)103)
#define RK_ERR_READY_QUEUE                  ((RK_ERR)-104)
#define RK_ERR_INVALID_PRIO                 ((RK_ERR)-105)
#define RK_ERR_TIMEOUT                      ((RK_ERR)106)
#define RK_ERR_INVALID_TIMEOUT              ((RK_ERR)-107)
#define RK_ERR_TASK_INVALID_ST              ((RK_ERR)-108)
#define RK_ERR_INVALID_ISR_PRIMITIVE        ((RK_ERR)-109)
#define RK_ERR_INVALID_PARAM                ((RK_ERR)-110)
#define RK_ERR_INVALID_OBJ                  ((RK_ERR)-111)

/* Memory Pool Service retval (200)*/
#define RK_ERR_MEM_FREE                     ((RK_ERR)-200)
#define RK_ERR_MEM_INIT                     ((RK_ERR)-201)

/* Synchronisation Services retval (300) */
#define RK_ERR_MUTEX_REC_LOCK               ((RK_ERR)-300)
#define RK_ERR_MUTEX_NOT_OWNER              ((RK_ERR)-301)
#define RK_ERR_MUTEX_LOCKED                 ((RK_ERR)302)
#define RK_ERR_MUTEX_NOT_LOCKED             ((RK_ERR)-303)
#define RK_ERR_FLAGS_NOT_MET                ((RK_ERR)304)
#define RK_ERR_BLOCKED_SEMA                 ((RK_ERR)305)
#define RK_ERR_SEMA_MAX_COUNT               ((RK_ERR)306)

/* Message Passing Services retval (400) */
#define RK_ERR_INVALID_QUEUE_SIZE           ((RK_ERR)-400)
#define RK_ERR_INVALID_MESG_SIZE            ((RK_ERR)-401)
#define RK_ERR_MBOX_FULL                    ((RK_ERR)402)
#define RK_ERR_MBOX_EMPTY                   ((RK_ERR)403)
#define RK_ERR_STREAM_FULL                  ((RK_ERR)404)
#define RK_ERR_STREAM_EMPTY                 ((RK_ERR)405)
#define RK_ERR_QUEUE_FULL                   ((RK_ERR)406)
#define RK_ERR_QUEUE_EMPTY                  ((RK_ERR)407)
#define RK_ERR_INVALID_RECEIVER             ((RK_ERR)-408)
#define RK_ERR_PORT_OWNER                   ((RK_ERR)-409)

/* Faults */

#define RK_GENERIC_FAULT                    RK_ERROR
#define RK_FAULT_READY_QUEUE                RK_ERR_READY_QUEUE
#define RK_FAULT_OBJ_NULL                   RK_ERR_OBJ_NULL
#define RK_FAULT_OBJ_NOT_INIT               RK_ERR_OBJ_NOT_INIT
#define RK_FAULT_TASK_INVALID_PRIO          RK_ERR_INVALID_PRIO
#define RK_FAULT_UNLOCK_OWNED_MUTEX         RK_ERR_MUTEX_NOT_OWNER
#define RK_FAULT_MUTEX_REC_LOCK             RK_ERR_MUTEX_REC_LOCK
#define RK_FAULT_MUTEX_NOT_LOCKED           RK_ERR_MUTEX_NOT_LOCKED
#define RK_FAULT_INVALID_ISR_PRIMITIVE      RK_ERR_INVALID_ISR_PRIMITIVE
#define RK_FAULT_TASK_INVALID_STATE         RK_ERR_TASK_INVALID_ST
#define RK_FAULT_INVALID_OBJ                RK_ERR_INVALID_OBJ
#define RK_FAULT_INVALID_PARAM              RK_ERR_INVALID_PARAM
#define RK_FAULT_PORT_OWNER                 RK_ERR_PORT_OWNER
#define RK_FAULT_INVALID_TIMEOUT            RK_ERR_INVALID_TIMEOUT
#define RK_FAULT_STACK_OVERFLOW             ((RK_FAULT)0xFAFAFAFA)
#define RK_FAULT_TASK_COUNT_MISMATCH        ((RK_FAULT)0xFBFBFBFB)
#define RK_FAULT_KERNEL_VERSION             ((RK_FAULT)0xFCFCFCFC)

/* Task Status */

#define RK_INVALID_TASK_STATE               ((RK_TASK_STATUS)0x00)
#define RK_READY                            ((RK_TASK_STATUS)0x10)
#define RK_RUNNING                          ((RK_TASK_STATUS)0x20)
#define RK_SLEEPING                         ((RK_TASK_STATUS)0x30)
#define RK_PENDING                          ((RK_TASK_STATUS)(RK_SLEEPING + 1U))
#define RK_BLOCKED                          ((RK_TASK_STATUS)(RK_SLEEPING + 2U))
#define RK_SENDING                          ((RK_TASK_STATUS)(RK_SLEEPING + 3U))
#define RK_RECEIVING                        ((RK_TASK_STATUS)(RK_SLEEPING + 4U))
#define RK_SLEEPING_DELAY                   ((RK_TASK_STATUS)(RK_SLEEPING + 5U))
#define RK_SLEEPING_PERIOD                  ((RK_TASK_STATUS)(RK_SLEEPING + 6U))

/* Kernel Objects ID */

#define RK_INVALID_KOBJ                     ((RK_KOBJ_ID)0x0)
#define RK_SEMAPHORE_KOBJ_ID                ((RK_KOBJ_ID)0x1)
#define RK_EVENT_KOBJ_ID                    ((RK_KOBJ_ID)0x2)
#define RK_MUTEX_KOBJ_ID                    ((RK_KOBJ_ID)0x3)
#define RK_MAILBOX_KOBJ_ID                  ((RK_KOBJ_ID)0x4)
#define RK_MAILQUEUE_KOBJ_ID                ((RK_KOBJ_ID)0x5)
#define RK_STREAMQUEUE_KOBJ_ID              ((RK_KOBJ_ID)0x6)
#define RK_MRM_KOBJ_ID                      ((RK_KOBJ_ID)0x7)
#define RK_TIMER_KOBJ_ID                    ((RK_KOBJ_ID)0x8)
#define RK_MEMALLOC_KOBJ_ID                 ((RK_KOBJ_ID)0x9)
#define RK_TASKHANDLE_KOBJ_ID               ((RK_KOBJ_ID)0xA)

/* Kernel Objects Typedefs */

/* Any typedef _HANDLE is a pointer to an object */

typedef struct kTcb RK_TCB;                  /* Task Control Block */
typedef struct kMemBlock RK_MEM;             /* Mem Alloc Control Block */
typedef struct kList RK_LIST;                /* DList ADT used for TCBs */
typedef struct kListNode RK_NODE;            /* DList Node */
typedef RK_LIST RK_TCBQ;                     /* Alias for RK_LIST for readability */
typedef struct kTcb *RK_TASK_HANDLE;         /* Pointer to TCB is a Task Handle */
typedef struct kTimeoutNode RK_TIMEOUT_NODE; /* Node for time-out delta list */
#if (RK_CONF_CALLOUT_TIMER == ON)
typedef struct kTimer RK_TIMER;
#endif
#if (RK_CONF_EVENT == ON)
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
#if (RK_CONF_QUEUE == ON)
typedef struct kQ RK_QUEUE;
#endif
#if (RK_CONF_STREAM == ON)
typedef struct kStream RK_STREAM;
#endif
#if (RK_CONF_MRM == ON)
typedef struct kMRMBuf RK_MRM_BUF;
typedef struct kMRMMem RK_MRM;
#endif

#define K_ERR_HANDLER kErrHandler

#define K_IS_BLOCK_ON_ISR(timeout) ((kIsISR() && (timeout > 0)) ? (1U) : (0))

#define K_IS_PENDING_CTXTSWTCH() \
    (kCoreGetPendingInterrupt(RK_CORE_PENDSV_IRQN) ? (1U) : (0))

#if (defined(STDDEF_H_) || defined(_STDDEF_H_) || defined(__STDEF_H__))

#define K_GET_CONTAINER_ADDR(memberPtr, containerType, memberName) \
    ((containerType *)((unsigned char *)(memberPtr) -              \
                       offsetof(containerType, memberName)))
#else
#error "Need stddef.h for offsetof()"

#endif

#define RK_NO_ARGS (NULL)

#define RK_UNUSEARGS (void)(args);

#define K_UNUSE(x) (void)(x)

#ifndef UNUSED
#define UNUSED(x) K_UNUSE(x)
#endif

#ifdef NDEBUG
#define kassert(x) (void)(x)
#else
#define kassert(x) assert(x)
#endif

/* GNU GCC Attributes*/
#ifdef __GNUC__
#ifndef __ASM
#define __ASM __asm
#endif

#ifndef __K_ALIGN
#define __K_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifndef __RK_WEAK
#define __RK_WEAK __attribute__((weak))
#endif

#ifndef __RK_INLINE
#define __RK_INLINE __attribute__((always_inline))
#endif

#endif /* __GNUC__*/

#ifdef __cplusplus
    }
#endif

#endif /* RK_COMMONDEFS_H */