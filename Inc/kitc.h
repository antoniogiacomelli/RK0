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

RK_ERR kSignalGet( ULONG const, ULONG *const,
    ULONG const, RK_TICK const);
RK_ERR kSignalSet( RK_TASK_HANDLE const, ULONG const);
RK_ERR kSignalQuery( ULONG *const);
RK_ERR kSignalClear( VOID);
 

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
