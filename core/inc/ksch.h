/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version              :   V0.4.0
 * Architecture         :   ARMv6/7-M
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 * www.kernel0.org
 *
 * Header for: HIGH-LEVEL SCHEDULER
*
 ******************************************************************************/

#ifndef RK_SCH_H
#define RK_SCH_H
#ifdef __cplusplus
{
#endif
#include <klist.h>
/* shared data */
extern RK_TCB* runPtr; /* Pointer to the running TCB */
extern RK_TCB tcbs[RK_NTHREADS]; /* Pool of TCBs */
extern volatile RK_FAULT faultID; /* Fault ID */
extern INT idleStack[RK_CONF_IDLE_STACKSIZE]; /* Stack for idle task */
extern INT timerHandlerStack[RK_CONF_TIMHANDLER_STACKSIZE];
extern RK_TCBQ readyQueue[RK_CONF_MIN_PRIO + 2]; /* Table of ready queues */
extern RK_TCBQ timeOutQueue;
extern volatile ULONG idleTicks;
BOOL kSchNeedReschedule(RK_TCB*);
VOID kSchSwtch(VOID);
UINT kEnterCR(VOID);
VOID kExitCR(UINT);
VOID kInit(VOID);
VOID kYield(VOID);
VOID kApplicationInit(VOID);
RK_ERR kTCBQInit(RK_LIST* const, CHAR*);
RK_ERR kTCBQEnq(RK_LIST* const, RK_TCB* const);
RK_ERR kTCBQDeq(RK_TCBQ* const, RK_TCB** const);
RK_ERR kTCBQRem(RK_TCBQ* const, RK_TCB** const);
RK_ERR kReadyCtxtSwtch(RK_TCB* const);
RK_ERR kReadyQDeq(RK_TCB** const, RK_PRIO);
RK_TCB* kTCBQPeek(RK_TCBQ* const);
RK_ERR kTCBQEnqByPrio(RK_TCBQ* const, RK_TCB* const);

#define RK_LIST_GET_TCB_NODE(nodePtr, containerType) \
    RK_GET_CONTAINER_ADDR(nodePtr, containerType, tcbNode)


#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
