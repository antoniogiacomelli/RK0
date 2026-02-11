/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.14                                               */
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
extern volatile UINT RK_gSchLock;

/* internal return values */
#ifndef RK_ERR_RESCHED_PENDING
#define RK_ERR_RESCHED_PENDING                ((RK_ERR)900)
#endif
#ifndef RK_ERR_RESCHED_NOT_NEEDED
#define RK_ERR_RESCHED_PENDING                ((RK_ERR)901)
#endif

VOID kSwtch(VOID);
VOID kInit(VOID);
VOID kYield(VOID);
VOID kApplicationInit(VOID);

/* Task queue management */
RK_ERR kTCBQInit(RK_TCBQ *const);
RK_ERR kTCBQEnq(RK_TCBQ *const, RK_TCB *const);
RK_ERR kTCBQJam(RK_TCBQ *const, RK_TCB *const);
RK_ERR kTCBQDeq(RK_TCBQ *const, RK_TCB **const);
RK_ERR kTCBQRem(RK_TCBQ *const, RK_TCB **const);
RK_TCB *kTCBQPeek(RK_TCBQ *const);
RK_ERR kTCBQEnqByPrio(RK_TCBQ *const, RK_TCB *const);
RK_ERR kReschedTask(RK_TCB *);
RK_ERR kReadySwtch(RK_TCB *const);
RK_ERR kReadyNoSwtch(RK_TCB *const);
RK_ERR kCreateTask(RK_TASK_HANDLE *,
                   const RK_TASKENTRY, VOID *,
                   CHAR *const, RK_STACK *const ,
                   const ULONG, const RK_PRIO,
                   const ULONG);



#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
