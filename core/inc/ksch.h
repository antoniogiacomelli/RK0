/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.2                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_SCH_H
#define RK_SCH_H
#ifdef __cplusplus
{
#endif
#include <kenv.h>
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
#include <klist.h>
#include <kstring.h>
/* Globals */
extern RK_TCB* RK_gRunPtr; /* Pointer to the running TCB */
extern RK_TCB RK_gTcbs[RK_NTHREADS]; /* Pool of RK_gTcbs */
extern volatile RK_FAULT RK_gFaultID; /* Fault ID */
extern UINT RK_gIdleStack[RK_CONF_IDLE_STACKSIZE]; /* Stack for idle task */
extern UINT RK_gPostProcStack[RK_CONF_TIMHANDLER_STACKSIZE];
extern RK_TCBQ RK_gReadyQueue[RK_CONF_MIN_PRIO + 2]; /* Table of ready queues */
extern volatile ULONG RK_gReadyBitmask;
extern volatile ULONG RK_gReadyPos;
extern volatile UINT RK_gPendingCtxtSwtch;


VOID kSwtch(VOID);
VOID kInit(VOID);
VOID kYield(VOID);
VOID kApplicationInit(VOID);

/* Task queue management */
RK_ERR kTCBQInit(RK_TCBQ *const kobj);
RK_ERR kTCBQEnq(RK_TCBQ *const kobj, RK_TCB *const tcbPtr);
RK_ERR kTCBQJam(RK_TCBQ *const kobj, RK_TCB *const tcbPtr);
RK_ERR kTCBQDeq(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr);
RK_ERR kTCBQRem(RK_TCBQ *const kobj, RK_TCB **const tcbPPtr);
RK_TCB *kTCBQPeek(RK_TCBQ *const kobj);
RK_ERR kTCBQEnqByPrio(RK_TCBQ *const kobj, RK_TCB *const tcbPtr);
VOID kSchedTask(RK_TCB *tcbPtr);
RK_ERR kReadySwtch(RK_TCB *const tcbPtr);
RK_ERR kReadyNoSwtch(RK_TCB *const tcbPtr);
RK_ERR kCreateTask(RK_TASK_HANDLE *,
                   const RK_TASKENTRY , VOID *,
                   CHAR *const , RK_STACK *const ,
                   const UINT , const RK_PRIO ,
                   const ULONG);

/**
 * @brief Declare data needed to create a task
 * @param HANDLE Task Handle
 * @param TASKENTRY Task's entry function
 * @param STACKBUF  Array's name for the task's stack
 * @param NWORDS	Stack Size in number of WORDS (even)
 */
#ifndef RK_DECLARE_TASK
#define  RK_DECLARE_TASK(HANDLE, TASKENTRY, STACKBUF, NWORDS) \
    VOID TASKENTRY(VOID *args);                              \
    RK_STACK STACKBUF[NWORDS] K_ALIGN(8);                  \
    RK_TASK_HANDLE HANDLE;
#endif
        

#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
