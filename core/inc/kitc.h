/******************************************************************************
 *
 *                     [RK0 - Real-Time Kernel '0']
 *
 * Version          :   V0.4.0
 * Architecture     :   ARMv6/7m
 *
 *
 * Copyright (c) 2025 Antonio Giacomelli
 *
 * Header for the Module: INTER-TASK COMMUNICATION AND SYNCHRONISATION
 * 
 ******************************************************************************/
#ifndef RK_ITC_H
#define RK_ITC_H

#ifdef __cplusplus
extern "C" {
#endif
/* Note the header expose only the API needed by others kernel modules,
in this case, the System Task TimerHandlerTask uses Signals to be notified.
*/
RK_ERR kSignalGet( ULONG const, UINT const,  ULONG *const, RK_TICK const);
RK_ERR kSignalSet( RK_TASK_HANDLE const, ULONG const);

#ifdef __cplusplus
}
#endif
#endif /* RK_ITC_H */
