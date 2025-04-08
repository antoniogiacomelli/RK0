/******************************************************************************
 *
 * RK0 - Real-Time Kernel '0'
 * Version  :   V0.4.0
 * Target   :   ARMv7m
 *
 * Copyright (c) 2025 Antonio Giacomelli
 ******************************************************************************/

/******************************************************************************
 *  In this header:
 *                  o Inter-task synchronisation and communication kernel
 *                    module.
 *
 *****************************************************************************/

#ifndef RK_ITC_H
#define RK_ITC_H

#ifdef __cplusplus
extern "C" {
#endif

/* publicised only needed prototypes */

#if(RK_CONF_MUTEX==ON)
RK_ERR kMutexLock( RK_MUTEX* const, RK_TICK const, BOOL);
RK_ERR kMutexUnlock( RK_MUTEX* const);
#endif

#if (RK_CONF_EVENT==ON)
RK_ERR kEventInit( RK_EVENT* const);
RK_ERR kEventWake( RK_EVENT* const);
RK_ERR kEventSignal( RK_EVENT* const);
RK_ERR kEventSleep( RK_EVENT* const, const RK_TICK);
#endif

RK_ERR kSignalGet( ULONG const, ULONG *const,
    ULONG const, RK_TICK const);
RK_ERR kSignalSet( RK_TASK_HANDLE const, ULONG const);
RK_ERR kSignalQuery( ULONG *const);
RK_ERR kSignalClear( VOID);
 
#if ((RK_CONF_EVENT == ON) && (RK_CONF_MUTEX == ON))
/* this is a helper for condition variables to perform the wait atomically
 unlocking the mutex and going to sleep
 for signal and wake one can use EventSignal and EventWake */
__attribute__((always_inline))
   inline RK_ERR kCondVarWait( RK_EVENT *eventPtr,
        RK_MUTEX *mutexPtr, RK_TICK timeout)
{
    RK_ERR err;
/* atomic */
    RK_CR_AREA RK_CR_ENTER
    kMutexUnlock( mutexPtr);
    err = kEventSleep( eventPtr, timeout);
    RK_CR_EXIT
    return (err);
/* upon returning (after wake) the condition variable must lock */
/* the mutex and loop around the condition */
}
__attribute__((always_inline))
   inline RK_ERR kCondVarSignal( RK_EVENT *eventPtr)
{
    return (kEventSignal( eventPtr));
}
__attribute__((always_inline))
   inline RK_ERR kCondVarBroad( RK_EVENT *eventPtr)
{
    return (kEventWake( eventPtr));
}

#endif

#ifndef _STRING_H_
static inline ADDR kMemSet( ADDR const destPtr, ULONG const val, ULONG size)
{
        BYTE *destTempPtr = (BYTE*) destPtr;
        while(--size)
        {
            *destTempPtr++ = (BYTE) val;
        }
        *destTempPtr = (BYTE)val;
        return (destPtr);
}
#endif

#ifdef __cplusplus
}
#endif
#endif /* RK_ITC_H */
