/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.19.2                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_COMMONDEFS_H
#define RK_COMMONDEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kconfig.h>


/* GNU GCC Attributes*/
#ifdef __GNUC__

#ifndef RK_ASM
#define RK_ASM asm
#endif

#ifndef RK_BARRIER
#define RK_BARRIER asm volatile ("" ::: "memory");
#endif

#ifndef K_ALIGN
#define K_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifndef RK_FUNC_WEAK
#define RK_FUNC_WEAK __attribute__((weak))
#endif

#ifndef RK_FORCE_INLINE
#define RK_FORCE_INLINE __attribute__((always_inline))
#endif

#ifndef RK_SECTION_HEAP
#define RK_SECTION_HEAP __attribute__((section("_user_heap")))
#endif

#endif /* __GNUC__*/

/*** PRIMITIVES  ***/

typedef signed INT;
typedef unsigned UINT;
typedef unsigned long ULONG;
typedef long LONG;
typedef unsigned long long ULLONG;
typedef long long LLONG;
typedef unsigned short USHORT;
typedef short SHORT;
typedef void VOID;
typedef char CHAR;
typedef float FLOAT;
typedef double DOUBLE;

/* by default in ARMv6/7 char is unsigned */
/* but as one can change it via compiler  */
/* we define SCHAR and BYTE */
typedef signed char SCHAR;
typedef unsigned char BYTE;

/*** KERNEL TYPE ALIASES ***/
typedef BYTE RK_PID;
typedef BYTE RK_PRIO;
typedef ULONG RK_TICK;
typedef LONG RK_STICK;
typedef INT RK_ERR;
typedef UINT RK_TASK_STATUS;
typedef INT RK_FAULT;
typedef UINT RK_ID;
typedef UINT RK_STACK;
typedef UINT RK_BOOL;
typedef ULONG RK_EVENT_FLAG;
typedef UINT RK_OPTION;

/*** KERNEL OBJECTS TYPEDEFS ***/
typedef struct RK_OBJ_TCB RK_TCB;
typedef struct RK_OBJ_MEM_PARTITION RK_MEM_PARTITION;
typedef struct RK_STRUCT_LIST RK_LIST;
typedef struct RK_STRUCT_LIST_EXT RK_LIST_EXT;
typedef struct RK_STRUCT_LIST_NODE RK_NODE;
typedef RK_LIST RK_TCBQ;

/* Pointer to TCB is a Task Handle */
typedef struct RK_OBJ_TCB* RK_TASK_HANDLE;
typedef struct RK_STRUCT_TIMEOUT_NODE RK_TIMEOUT_NODE;

/*  internal functions */

VOID kSchLock(VOID);
VOID kSchUnlock(VOID);
VOID kPendCtxSwtch(VOID);

#if (RK_CONF_DYNAMIC_TASK == ON)
typedef struct RK_STRUCT_DYNAMIC_TASK_ATTR RK_DYNAMIC_TASK_ATTR;
#endif

#if (RK_CONF_CALLOUT_TIMER == ON)

typedef struct RK_OBJ_TIMER RK_TIMER;

#endif

#if (RK_CONF_SLEEP_QUEUE == ON)

typedef struct RK_OBJ_SLEEP_QUEUE RK_SLEEP_QUEUE;

#endif

#if (RK_CONF_SEMAPHORE == ON)

typedef struct RK_OBJ_SEMAPHORE RK_SEMAPHORE;
#endif

#if (RK_CONF_MUTEX == ON)

typedef struct RK_OBJ_MUTEX RK_MUTEX;

#endif

#if (RK_CONF_MESG_QUEUE == ON)
typedef struct RK_OBJ_MESG_QUEUE RK_MESG_QUEUE;
#endif

#if (RK_CONF_CHANNEL == ON)
typedef struct RK_OBJ_CHANNEL RK_CHANNEL;
typedef struct RK_STRUCT_REQUEST_MESG_BUF RK_REQ_BUF;
#endif

#if (RK_CONF_MRM == ON)

typedef struct RK_OBJ_MRM_BUF RK_MRM_BUF;
typedef struct RK_OBJ_MRM RK_MRM;

#endif

/* Function pointers */
typedef void (*RK_TASKENTRY)(void*);         /* Task entry function pointer */
typedef void (*RK_TIMER_CALLOUT)(void*);     /* Callout (timers)             */



/* Values */

#ifndef UINT8_MAX
#define UINT8_MAX (0xFF) /* 255 */
#endif
#ifndef INT8_MAX
#define INT8_MAX (0x7F) /* 127 */
#endif
#ifndef UINT16_MAX
#define UINT16_MAX (0xFFFF)
#endif
#ifndef INT16_MAX
#define INT16_MAX (0x7FFF)
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

/* we do not use std _Bool on kernel objects */
#define RK_FALSE (RK_BOOL)0U
#define RK_TRUE  (RK_BOOL)1U

/* Null pointer */
#ifndef NULL
#define NULL ((void *)(0))
#endif

/*** Stackframe Registers offset ***/

#define PSR_OFFSET 1
#define PC_OFFSET 2
#define LR_OFFSET 3
#define R12_OFFSET 4
#define R3_OFFSET 5
#define R2_OFFSET 6
#define R1_OFFSET 7
#define R0_OFFSET 8
#define R11_OFFSET 9
#define R10_OFFSET 10
#define R9_OFFSET 11
#define R8_OFFSET 12
#define R7_OFFSET 13
#define R6_OFFSET 14
#define R5_OFFSET 15
#define R4_OFFSET 16

/* Stack paint */
#define RK_STACK_GUARD (0x0BADC0DEU)
#define RK_STACK_PATTERN (0xA5A5A5A5U)
#define RK_MIN_STACKSIZE 128U

/*** For kconfig.h ***/
#ifndef RK_CONF_MIN_PRIO
#define RK_CONF_MIN_PRIO 31
#endif
#define RK_POSTPROC_TASK_ID ((RK_PID)(0x01))
#define RK_IDLETASK_ID ((RK_PID)(0x00))
#define RK_N_SYSTASKS 2U /* idle + post-processing */
#define RK_NTHREADS (RK_CONF_N_USRTASKS_MAX + RK_N_SYSTASKS)
#define RK_CONF_NTASKS RK_NTHREADS
#ifndef RK_RDYQSIZ
#define RK_RDYQSIZ (RK_CONF_MIN_PRIO + 2U)
#endif

/*** SERVICE TOKENS  ***/
/* Blocking option */
#define RK_WAIT_FOREVER ((RK_TICK)0xFFFFFFFF)
#define RK_NO_WAIT ((RK_TICK)0x0)

/* PostProcessing  Signals */
#define RK_POSTPROC_SIG ((RK_EVENT_FLAG)0x1)
#define RK_POSTPROC_TIMER_SIG ((RK_EVENT_FLAG)0x2)

/* OPTIONS */
#define RK_NO_PREEMPT (RK_OPTION)0U
#define RK_PREEMPT (RK_OPTION)1U

#define RK_EVENT_ANY ((RK_OPTION)0x2)
#define RK_EVENT_ALL ((RK_OPTION)0x4)
/* aliases for backward compatibility */
#define RK_EVENT_FLAGS_ANY RK_EVENT_ANY
#define RK_EVENT_FLAGS_ALL RK_EVENT_ALL
#define RK_FLAGS_ANY RK_EVENT_ANY
#define RK_FLAGS_ALL RK_EVENT_ALL

#define RK_TIMER_RELOAD (RK_OPTION)1U
#define RK_TIMER_ONESHOT (RK_OPTION)0U

/* TIMEOUT CODES */
/* elapsed bounded waiting on a public event object */
#define RK_TIMEOUT_BLOCKING ((UINT)0x1)

/* elapsed bounded waiting on a private event */
#define RK_TIMEOUT_EVENTFLAGS ((UINT)0x2)

/* elapsed waiting on an application timer */
#define RK_TIMEOUT_CALL ((UINT)0x4)

/* elapsed waiting on a sleep/delay/until/release */
#define RK_TIMEOUT_TIME_EVENT ((UINT)0x8)

/*** Task Events ***/

#define RK_ALL_EVENTS ((RK_EVENT_FLAG)0xFFFFFFFF)

#define RK_EVENT_1 ((RK_EVENT_FLAG)0x00000001)
#define RK_EVENT_2 ((RK_EVENT_FLAG)0x00000002)
#define RK_EVENT_3 ((RK_EVENT_FLAG)0x00000004)
#define RK_EVENT_4 ((RK_EVENT_FLAG)0x00000008)
#define RK_EVENT_5 ((RK_EVENT_FLAG)0x00000010)
#define RK_EVENT_6 ((RK_EVENT_FLAG)0x00000020)
#define RK_EVENT_7 ((RK_EVENT_FLAG)0x00000040)
#define RK_EVENT_8 ((RK_EVENT_FLAG)0x00000080)
#define RK_EVENT_9 ((RK_EVENT_FLAG)0x00000100)
#define RK_EVENT_10 ((RK_EVENT_FLAG)0x00000200)
#define RK_EVENT_11 ((RK_EVENT_FLAG)0x00000400)
#define RK_EVENT_12 ((RK_EVENT_FLAG)0x00000800)
#define RK_EVENT_13 ((RK_EVENT_FLAG)0x00001000)
#define RK_EVENT_14 ((RK_EVENT_FLAG)0x00002000)
#define RK_EVENT_15 ((RK_EVENT_FLAG)0x00004000)
#define RK_EVENT_16 ((RK_EVENT_FLAG)0x00008000)
#define RK_EVENT_17 ((RK_EVENT_FLAG)0x00010000)
#define RK_EVENT_18 ((RK_EVENT_FLAG)0x00020000)
#define RK_EVENT_19 ((RK_EVENT_FLAG)0x00040000)
#define RK_EVENT_20 ((RK_EVENT_FLAG)0x00080000)
#define RK_EVENT_21 ((RK_EVENT_FLAG)0x00100000)
#define RK_EVENT_22 ((RK_EVENT_FLAG)0x00200000)
#define RK_EVENT_23 ((RK_EVENT_FLAG)0x00400000)
#define RK_EVENT_24 ((RK_EVENT_FLAG)0x00800000)
#define RK_EVENT_25 ((RK_EVENT_FLAG)0x01000000)
#define RK_EVENT_26 ((RK_EVENT_FLAG)0x02000000)
#define RK_EVENT_27 ((RK_EVENT_FLAG)0x04000000)
#define RK_EVENT_28 ((RK_EVENT_FLAG)0x08000000)
#define RK_EVENT_29 ((RK_EVENT_FLAG)0x10000000)
#define RK_EVENT_30 ((RK_EVENT_FLAG)0x20000000)
#define RK_EVENT_31 ((RK_EVENT_FLAG)0x40000000)
#define RK_EVENT_32 ((RK_EVENT_FLAG)0x80000000)


/* Mutex Priority Inh */
#define RK_NO_INHERIT ((UINT)0)
#define RK_INHERIT ((UINT)1)

/* Kernel object name string */
#define RK_OBJ_MAX_NAME_LEN (8U)

/* Max period - half-way ULONG*/
#define RK_MAX_PERIOD ((RK_TICK)(~(RK_TICK)0 >> 1))
/* 0x7FFFFFFF */

/* RETURN VALUES */

#define RK_ERR_SUCCESS ((RK_ERR)0x0)
/* Generic error (-1) */
#define RK_ERR_ERROR ((RK_ERR) -1)

/* Non-service specific retval (100) */
#define RK_ERR_OBJ_NULL ((RK_ERR) -100)
#define RK_ERR_OBJ_NOT_INIT ((RK_ERR) -101)
#define RK_ERR_LIST_EMPTY ((RK_ERR)102)
#define RK_ERR_EMPTY_WAITING_QUEUE ((RK_ERR)103)
#define RK_ERR_READY_QUEUE ((RK_ERR) -104)
#define RK_ERR_INVALID_PRIO ((RK_ERR) -105)
#define RK_ERR_TASK_INVALID_ST ((RK_ERR) -108)
#define RK_ERR_INVALID_ISR_PRIMITIVE ((RK_ERR) -109)
#define RK_ERR_INVALID_PARAM ((RK_ERR) -110)
#define RK_ERR_INVALID_OBJ ((RK_ERR) -111)
#define RK_ERR_OBJ_DOUBLE_INIT ((RK_ERR) -112)
#define RK_ERR_TASK_POOL_NOT_INIT ((RK_ERR) -113)
#define RK_ERR_TASK_ALREADY_INIT ((RK_ERR) 113)
#define RK_ERR_TASK_POOL_EMPTY ((RK_ERR)114)
/* Memory Pool Service retval (200)*/
#define RK_ERR_MEM_FREE ((RK_ERR) -200)
#define RK_ERR_MEM_INIT ((RK_ERR) -201)

/* Synchronisation Services retval (300) */
#define RK_ERR_MUTEX_REC_LOCK ((RK_ERR) -300)
#define RK_ERR_MUTEX_NOT_OWNER ((RK_ERR) -301)
#define RK_ERR_MUTEX_LOCKED ((RK_ERR)302)
#define RK_ERR_MUTEX_NOT_LOCKED ((RK_ERR) -303)
#define RK_ERR_FLAGS_NOT_MET ((RK_ERR)304)
#define RK_ERR_SEMA_BLOCKED ((RK_ERR)305)
#define RK_ERR_SEMA_FULL ((RK_ERR)306)
#define RK_ERR_NOWAIT ((RK_ERR)307)


/* Message Passing Services retval (400) */
#define RK_ERR_INVALID_DEPTH ((RK_ERR) -400)
#define RK_ERR_INVALID_MSG_SIZE ((RK_ERR) -401)
#define RK_ERR_BUFFER_FULL ((RK_ERR)402)
#define RK_ERR_BUFFER_EMPTY ((RK_ERR)403)
#define RK_ERR_NOT_OWNER ((RK_ERR)-404)
#define RK_ERR_HAS_OWNER ((RK_ERR)-405)
#define RK_ERR_MESGQ_HAS_OWNER RK_ERR_HAS_OWNER
#define RK_ERR_MESGQ_NOT_A_MBOX ((RK_ERR)406)

/* Time-related */
#define RK_ERR_NULL_TIMEOUT_NODE ((RK_ERR) -500)
#define RK_ERR_INVALID_TIMEOUT ((RK_ERR) -501)
#define RK_ERR_TIMEOUT ((RK_ERR)502)
#define RK_ERR_ELAPSED_PERIOD ((RK_ERR)503)

/* Faults */

#define RK_GENERIC_FAULT ((RK_FAULT)RK_ERR_ERROR)
#define RK_FAULT_READY_QUEUE ((RK_FAULT)RK_ERR_READY_QUEUE)
#define RK_FAULT_OBJ_NULL ((RK_FAULT)RK_ERR_OBJ_NULL)
#define RK_FAULT_OBJ_NOT_INIT ((RK_FAULT)RK_ERR_OBJ_NOT_INIT)
#define RK_FAULT_OBJ_DOUBLE_INIT ((RK_FAULT)RK_ERR_OBJ_DOUBLE_INIT)
#define RK_FAULT_HAS_OWNER ((RK_FAULT)RK_ERR_HAS_OWNER)
#define RK_FAULT_TASK_INVALID_PRIO ((RK_FAULT)RK_ERR_INVALID_PRIO)
#define RK_FAULT_UNLOCK_OWNED_MUTEX ((RK_FAULT)RK_ERR_MUTEX_NOT_OWNER)
#define RK_FAULT_MUTEX_REC_LOCK ((RK_FAULT)RK_ERR_MUTEX_REC_LOCK)
#define RK_FAULT_MUTEX_NOT_LOCKED ((RK_FAULT)RK_ERR_MUTEX_NOT_LOCKED)
#define RK_FAULT_INVALID_ISR_PRIMITIVE ((RK_FAULT)RK_ERR_INVALID_ISR_PRIMITIVE)

#define RK_FAULT_TASK_INVALID_STATE ((RK_FAULT)RK_ERR_TASK_INVALID_ST)
#define RK_FAULT_INVALID_OBJ ((RK_FAULT)RK_ERR_INVALID_OBJ)
#define RK_FAULT_INVALID_PARAM ((RK_FAULT)RK_ERR_INVALID_PARAM)
#define RK_FAULT_INVALID_TIMEOUT ((RK_FAULT)RK_ERR_INVALID_TIMEOUT)
#define RK_FAULT_MEM_FREE ((RK_FAULT)RK_ERR_MEM_FREE)
#define RK_FAULT_TASK_POOL_NOT_INIT ((RK_FAULT)RK_ERR_TASK_POOL_NOT_INIT)
#define RK_FAULT_STACK_OVERFLOW ((RK_FAULT)0xFAFAFAFA)
#define RK_FAULT_TASK_COUNT_MISMATCH ((RK_FAULT)0xFBFBFBFB)
#define RK_FAULT_KERNEL_VERSION ((RK_FAULT)0xFCFCFCFC)
#define RK_FAULT_APP_CRASH ((RK_FAULT)0xFFFFFC7C)   /* -900 */
#define RK_FAULT_INIT_KERNEL ((RK_FAULT)0xFFFFFC72) /* -910 */
/* Task Status */

#define RK_INVALID_TASK_STATE ((RK_TASK_STATUS)0x00)

/* after initTcb_ */
#define RK_TCB_INITIALISED ((RK_TASK_STATUS)0x01)

/* Schedulable */
#define RK_READY ((RK_TASK_STATUS)0x10)

/* Took CPU, running */
#define RK_RUNNING ((RK_TASK_STATUS)0x20)

/* Sleeping on a sleep queue for an event */
#define RK_SLEEPING ((RK_TASK_STATUS)0x40)

/* Sleeping for an event flag on its own event register */
#define RK_SLEEPING_EV_FLAG ((RK_TASK_STATUS)0x41)

/* Blocked on a semaphore/mutex */
#define RK_BLOCKED ((RK_TASK_STATUS)0x42)

/* Sender blocked on a full message buffer */
#define RK_SENDING ((RK_TASK_STATUS)0x43)

/* Receiver blocked on an empty message buffer */
#define RK_RECEIVING ((RK_TASK_STATUS)0x44)

/* Task sleeping for a given amount of delay */
#define RK_SLEEPING_DELAY ((RK_TASK_STATUS)0x45)

/* Task sleeping until time meets period, phase-locked */
#define RK_SLEEPING_RELEASE ((RK_TASK_STATUS)0x46)

/* Task sleeping until time meets period, not phase-locked */
#define RK_SLEEPING_UNTIL ((RK_TASK_STATUS)0x47)

/* A ready task was removed from ready queue and is on a sleeping queue */
#define RK_SLEEPING_SUSPENDED ((RK_TASK_STATUS)0x48)

/* Task pending on a deferred signal */
#define RK_PENDING ((RK_TASK_STATUS)0x49)

/* Slot belonged to a task that has been terminated and released */
#define RK_TASK_TERMINATED ((RK_TASK_STATUS)0x4B)

/* KERNEL OBJECT IDS */
#define RK_INVALID_KOBJ ((RK_ID)0x00000000)

#define RK_SEMAPHORE_KOBJ_ID ((RK_ID)0xD00FFF01)
#define RK_SLEEPQ_KOBJ_ID ((RK_ID)0xD00FFF02)
#define RK_MUTEX_KOBJ_ID ((RK_ID)0xD00FFF04)

#define RK_MESGQQUEUE_KOBJ_ID ((RK_ID)0xD01FFF01)
#define RK_ASR_KOBJ_ID ((RK_ID)0xD01FFF03) /* legacy placeholder */
#define RK_MRM_KOBJ_ID ((RK_ID)0xD01FFF02)
#define RK_CHANNEL_KOBJ_ID ((RK_ID)0xD01FFF04)

#define RK_TIMER_KOBJ_ID ((RK_ID)0xD02FFF01)

#define RK_MEMALLOC_KOBJ_ID ((RK_ID)0xD04FFF01)

#define RK_TASKHANDLE_KOBJ_ID ((RK_ID)0xD08FFF01)

/*** Internal return values ***/
#ifndef RK_ERR_RESCHED_PENDING
#define RK_ERR_RESCHED_PENDING ((RK_ERR)900)
#endif
#ifndef RK_ERR_RESCHED_NOT_NEEDED
#define RK_ERR_RESCHED_NOT_NEEDED ((RK_ERR)901)
#endif

/* CONVENIENCE MACROS */

#ifndef RK_TICK_INTERVAL_MS
#define RK_TICK_INTERVAL_MS (1000UL / RK_CONF_SYSTICK_DIV)
#endif

#ifndef RK_TICKS_TO_MS
#define RK_TICKS_TO_MS(ticks) ((ticks) * RK_TICK_INTERVAL_MS)
#endif


#ifndef K_ERR_HANDLER
#define K_ERR_HANDLER(x) kErrHandler(x)
#endif
#ifndef K_BLOCKING_ON_ISR
#define K_BLOCKING_ON_ISR(timeout) ((kIsISR() && (timeout > 0)) ? (1U) : (0))
#endif

#ifndef K_GET_CONTAINER_ADDR
#ifndef offsetof
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#endif
#define K_GET_CONTAINER_ADDR(memberPtr, containerType, memberName)\
        ((containerType *)((unsigned char *)(memberPtr) -\
                           offsetof(containerType, memberName)))
#endif

#define RK_NO_ARGS (NULL)

#define RK_UNUSEARGS (void)(args);

#define K_UNUSE(x) (void)(x)

#ifndef UNUSED
#define UNUSED(x) K_UNUSE(x)

#endif

#ifndef RK_ABORT
#ifdef NDEBUG
#define RK_ABORT (void)(0);
#else
#define RK_ABORT\
        RK_ASM volatile ("CPSID I" : : : "memory");\
        RK_ASM volatile ("BKPT #0");\
        while (1)\
        {\
            RK_ASM volatile ("NOP");\
        }
#endif
#endif

#ifdef NDEBUG
#define K_ASSERT(x)\
        ((void)(x)) /* it must be void(x) so it does not give unused variable\
                   warnings */
#else
#ifdef assert
#define K_ASSERT(x) assert(x)
#else
/* CONFIGURE YOUR ASSERTION(s) MACROS */
#define K_ASSERT(x)\
        do\
        {\
            if ((x) == 0)\
            {\
                RK_ABORT\
            }\
        } while (0)
#endif
#endif

#ifndef RK_WORD_SIZE
#define RK_WORD_SIZE (sizeof(ULONG))
#endif

/* get the size of a type in bytes and return in words */
#ifndef RK_TYPE_WORD_COUNT
#define RK_TYPE_WORD_COUNT(TYPE)\
        ((UINT)(((sizeof(TYPE) + RK_WORD_SIZE - 1UL)) / RK_WORD_SIZE))
#endif

/* round a number of words to the next power of 2 up to 16 */
#ifndef RK_ROUND_POW2_1_2_4_8_16
#define RK_ROUND_POW2_1_2_4_8_16(W)\
        (((W) <= 1UL)   ? 1UL\
     : ((W) <= 2UL) ? 2UL\
     : ((W) <= 4UL) ? 4UL\
     : ((W) <= 8UL) ? 8UL\
                    : 16UL)
#endif

/* get the size of a type in words rounded to the next power of 2 */
#ifndef RK_TYPE_SIZE_POW2_WORDS
#define RK_TYPE_SIZE_POW2_WORDS(TYPE)\
        RK_ROUND_POW2_1_2_4_8_16(RK_TYPE_WORD_COUNT(TYPE))
#endif

/* Timeout node setup for running tasks */
#ifndef RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP
#define RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP\
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;\
        RK_gRunPtr->timeoutNode.waitingQueuePtr = &kobj->waitingQueue;\
        RK_BARRIER
#endif

#ifndef RK_TASK_TIMEOUT_EVENTFLAGS
#define RK_TASK_TIMEOUT_EVENTFLAGS\
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_EVENTFLAGS;\
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;\
        RK_BARRIER
#endif

#ifndef RK_TASK_SLEEP_TIMEOUT_SETUP
#define RK_TASK_SLEEP_TIMEOUT_SETUP\
        RK_gRunPtr->timeoutNode.timeoutType = RK_TIMEOUT_TIME_EVENT;\
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;\
        RK_BARRIER
#endif




#ifdef __cplusplus
}
#endif
#endif
