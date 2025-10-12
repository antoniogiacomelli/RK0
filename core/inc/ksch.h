/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
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
extern RK_TCB* runPtr; /* Pointer to the running TCB */
extern RK_TCB tcbs[RK_NTHREADS]; /* Pool of TCBs */
extern volatile RK_FAULT faultID; /* Fault ID */
extern UINT idleStack[RK_CONF_IDLE_STACKSIZE]; /* Stack for idle task */
extern UINT postprocStack[RK_CONF_TIMHANDLER_STACKSIZE];
extern RK_TCBQ readyQueue[RK_CONF_MIN_PRIO + 2]; /* Table of ready queues */
extern ULONG readyQBitMask;
extern ULONG readyQRightMask;
extern volatile UINT isPendingCtxtSwtch;
#if (RK_CONF_IDLE_TICK_COUNT==ON)
extern volatile ULONG idleTicks;
#endif

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



#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
