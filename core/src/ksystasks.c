/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.0                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/*******************************************************************************
 * 	COMPONENT        : SYSTEM TASKS
 *  DEPENDS ON       : TASK FLAGS, TIMER
 *  PUBLIC API   	 : YES
 ******************************************************************************/

#define RK_SOURCE_CODE
#include <ksystasks.h>

UINT RK_gIdleStack[RK_CONF_IDLE_STACKSIZE] K_ALIGN(8);
UINT RK_gPostProcStack[RK_CONF_TIMHANDLER_STACKSIZE] K_ALIGN(8);


VOID IdleTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_ISB

        RK_WFI 

        RK_DSB
    }
}

#define POSTPROC_SIGNAL_RANGE 0x3
VOID PostProcSysTask(VOID *args)
{
    RK_UNUSEARGS

    RK_CORE_SYSTICK->CTRL |= 0x01;
    

    while (1)
    {

        ULONG gotFlags = 0;

        kTaskFlagsGet(POSTPROC_SIGNAL_RANGE, RK_FLAGS_ANY, &gotFlags, RK_WAIT_FOREVER);

#if (RK_CONF_CALLOUT_TIMER == ON)
        if (gotFlags & RK_POSTPROC_SIG_TIMER)
        {

         
            while (RK_gTimerListHeadPtr != NULL && RK_gTimerListHeadPtr->dtick == 0)
            {
                RK_TIMEOUT_NODE *node = (RK_TIMEOUT_NODE *)RK_gTimerListHeadPtr;
                RK_gTimerListHeadPtr = node->nextPtr;
                kRemoveTimerNode(node);

                RK_TIMER *timer = K_GET_CONTAINER_ADDR(node, RK_TIMER,
                                                       timeoutNode);
                if (timer->funPtr != NULL)
                {
                    timer->funPtr(timer->argsPtr);
                }
                if (timer->reload)
                {
                    RK_TICK now = kTickGet();
                    RK_TICK base = timer->nextTime;
                    RK_TICK elapsed = K_TICK_DELAY(now, base);
                    RK_TICK skips = ((elapsed / timer->period) + 1);
                    RK_TICK offset = (RK_TICK)(skips * timer->period);
                    timer->nextTime = K_TICK_ADD(base, offset);
                    RK_TICK delay = K_TICK_DELAY(timer->nextTime, now);
                    kTimerReload(timer, delay);
                }
                    
            }
        }
#endif
    }
}
