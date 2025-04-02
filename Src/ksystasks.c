/******************************************************************************
 *
 * RK0 - Real-Time Kernel '0'
 * Version  :   V0.4.0
 * Target   :   ARMv7m
 *
 * Copyright (c) 2025 Antonio Giacomelli
 ******************************************************************************/
/******************************************************************************
 * 	Module       : SYSTEM TASKS
 * 	Depends on   : EXECUTIVE
 * 	Provides to  : APPLICATION
 *  Public API	 : N/A
 * 	In this unit :
 * 					o  System Tasks and Deferred ISR handler tasks
 *
 *****************************************************************************/

#define RK_CODE
#include "kexecutive.h"

INT idleStack[RK_CONF_IDLE_STACKSIZE];
INT timerHandlerStack[RK_CONF_TIMHANDLER_STACKSIZE];

#if (RK_CONF_IDLE_TICK_COUNT==ON)
volatile ULONG idleTicks = 0;
#endif
VOID IdleTask( VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        __DSB();
#if (RK_CONF_IDLE_TICK_COUNT==ON)
        idleTicks ++;
#endif
        __WFI();
        __ISB();
    }
}

VOID TimerHandlerTask( VOID *args)
{
    RK_UNUSEARGS

    RK_TICK_EN

    while (1)
    {

        RK_PEND();
        
#if (RK_CONF_CALLOUT_TIMER==ON)

        while (timerListHeadPtr != NULL && timerListHeadPtr->dtick == 0)
        {
            RK_TIMEOUT_NODE *node = ( RK_TIMEOUT_NODE*) timerListHeadPtr;
            timerListHeadPtr = node->nextPtr;
            kRemoveTimerNode( node);

            RK_TIMER *timer = RK_GET_CONTAINER_ADDR( node, RK_TIMER, timeoutNode);
            if (timer->funPtr != NULL)
            {
                timer->funPtr( timer->argsPtr);
            }
            if (timer->reload)
            {
                 /* discard phase delay when reloading */
                kTimerInit( timer, 0, timer->timeoutNode.timeout, timer->funPtr,
                        timer->argsPtr, timer->reload);
            }

        }
  #endif

    }

}
