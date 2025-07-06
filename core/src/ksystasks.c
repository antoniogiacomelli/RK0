/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.5
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

/*******************************************************************************
 * 	Module       : SYSTEM TASKS
 * 	Depends on   : INTER-TASK SYNCHRONISATION
 * 	Provides to  : APPLICATION, TIMERS
 *  Public API	 : N/A
 ******************************************************************************/

#define RK_CODE
#include "kservices.h"

UINT idleStack[RK_CONF_IDLE_STACKSIZE] __RK_ALIGN(8);
UINT timerHandlerStack[RK_CONF_TIMHANDLER_STACKSIZE] __RK_ALIGN(8);

#if (RK_CONF_IDLE_TICK_COUNT == ON)
volatile ULONG idleTicks = 0;
#endif
VOID IdleTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        __RK_ISB

#if (RK_CONF_IDLE_TICK_COUNT == ON)
        idleTicks += 1UL;
#endif

        __WFI();

        __RK_DSB
    }
}

#define SIGNAL_RANGE 0x3
VOID TimerHandlerTask(VOID *args)
{
    RK_UNUSEARGS

    RK_CORE_SYSTICK->CTRL |= 0x01;

    ULONG gotFlags = 0;
    while (1)
    {

        kSignalGet(SIGNAL_RANGE, RK_FLAGS_ANY, &gotFlags, RK_WAIT_FOREVER);

#if (RK_CONF_CALLOUT_TIMER == ON)
        if (gotFlags & RK_SIG_TIMER)
        {

            while (timerListHeadPtr != NULL && timerListHeadPtr->dtick == 0)
            {
                RK_TIMEOUT_NODE *node = (RK_TIMEOUT_NODE *)timerListHeadPtr;
                timerListHeadPtr = node->nextPtr;
                kRemoveTimerNode(node);

                RK_TIMER *timer = K_GET_CONTAINER_ADDR(node, RK_TIMER,
                                                       timeoutNode);
                if (timer->funPtr != NULL)
                {
                    timer->funPtr(timer->argsPtr);
                }
                if (timer->reload)
                {
                    /* discard phase delay when reloading */
                    kTimerInit(timer, 0, timer->timeoutNode.timeout,
                               timer->funPtr, timer->argsPtr, timer->reload);
                }
            }
        }
#endif
    }
}
