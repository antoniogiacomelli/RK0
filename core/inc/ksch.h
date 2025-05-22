/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
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
    K_GET_CONTAINER_ADDR(nodePtr, containerType, tcbNode)


#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
