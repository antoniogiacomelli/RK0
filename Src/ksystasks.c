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

#define SIGNAL_RANGE 0x3
VOID TimerHandlerTask( VOID *args)
{
    RK_UNUSEARGS

    RK_TICK_EN

    ULONG gotFlags = 0;
    while (1)
    {

        kSignalGet( SIGNAL_RANGE, &gotFlags, RK_FLAGS_ANY, RK_WAIT_FOREVER);

#if (RK_CONF_CALLOUT_TIMER==ON)
        if (gotFlags & RK_SIG_TIMER)
        {

            while (timerListHeadPtr != NULL && timerListHeadPtr->dtick == 0)
            {
                RK_TIMEOUT_NODE *node = ( RK_TIMEOUT_NODE*) timerListHeadPtr;
                timerListHeadPtr = node->nextPtr;
                kRemoveTimerNode( node);

                RK_TIMER *timer = RK_GET_CONTAINER_ADDR( node, RK_TIMER,
                        timeoutNode);
                if (timer->funPtr != NULL)
                {
                    timer->funPtr( timer->argsPtr);
                }
                if (timer->reload)
                {
/* discard phase delay when reloading */
                    kTimerInit( timer, 0, timer->timeoutNode.timeout,
                            timer->funPtr, timer->argsPtr, timer->reload);
                }
            }
        }
#endif
#if (RK_CONF_EVENT_GROUPS==ON)
        if (gotFlags & RK_SIG_EVENTGROUP)
        {
            BOOL conditionMet = 0;
/* process waiting tasks */
            if (eventWaitingQueue.size > 0)
            {
                RK_TCB *currTcbPtr = NULL;
                currTcbPtr = kTCBQPeek( &eventWaitingQueue);
                RK_NODE *currNodePtr = &currTcbPtr->tcbNode;

                while (currNodePtr != &eventWaitingQueue.listDummy)
                {
                    ULONG currentFlags = *currTcbPtr->groupPtr;

                    BOOL all =
                            (currTcbPtr->groupOptions & RK_FLAGS_ALL_KEEP)
                                    || (currTcbPtr->groupOptions
                                            & RK_FLAGS_ALL_CONSUME);

                    BOOL clear =
                            (currTcbPtr->groupOptions & RK_FLAGS_ALL_CONSUME)
                                    || (currTcbPtr->groupOptions
                                            & RK_FLAGS_ANY_CONSUME);
/* wake tasks which condition is met */
                    if (all)/* AND */
                    {
                        conditionMet = ((currentFlags
                                & currTcbPtr->requiredGroupFlags)
                                == (currTcbPtr->requiredGroupFlags));
                    }
                    else/* OR */
                    {
                        conditionMet = (currentFlags
                                & currTcbPtr->requiredGroupFlags);
                    }
                    if (conditionMet)

                    {
                        currTcbPtr->gotGroupFlags = currentFlags;
                        if (clear)
                        {
                            *(currTcbPtr->groupPtr) &=
                                    ~(currTcbPtr->requiredGroupFlags);
                        }
                        kListRemove( &eventWaitingQueue, currNodePtr);
                        kReadyCtxtSwtch( currTcbPtr);
                        currTcbPtr = kTCBQPeek( &eventWaitingQueue);
                        currNodePtr = &currTcbPtr->tcbNode;

                    }
                    else
                    {
                        currNodePtr = currNodePtr->nextPtr;
                        currTcbPtr = RK_LIST_GET_TCB_NODE( currNodePtr, RK_TCB);
                    }
                }

            }
        }
         
    #endif
    }
}
