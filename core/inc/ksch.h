/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.6
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
/* Globals */
extern RK_TCB* runPtr; /* Pointer to the running TCB */
extern RK_TCB tcbs[RK_NTHREADS]; /* Pool of TCBs */
extern volatile RK_FAULT faultID; /* Fault ID */
extern UINT idleStack[RK_CONF_IDLE_STACKSIZE]; /* Stack for idle task */
extern UINT timerHandlerStack[RK_CONF_TIMHANDLER_STACKSIZE];
extern RK_TCBQ readyQueue[RK_CONF_MIN_PRIO + 2]; /* Table of ready queues */
extern volatile ULONG idleTicks;
extern ULONG readyQBitMask;
extern ULONG readyQRightMask;
extern volatile UINT isPendingCtxtSwtch;
#if (RK_CONF_IDLE_TICK_COUNT==ON)
extern volatile ULONG idleTicks;
#endif

VOID kSchSwtch(VOID);
VOID kInit(VOID);
VOID kYield(VOID);
VOID kApplicationInit(VOID);



#ifdef __cplusplus
}
#endif

#endif /* KSCH_H */
