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
#include <stddef.h>           /* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

RK_ERR kSignalGet( ULONG const, UINT const,  ULONG *const, RK_TICK const);
RK_ERR kSignalSet( RK_TASK_HANDLE const, ULONG const);
RK_ERR kSignalQuery( ULONG *const);
RK_ERR kSignalClear( VOID);
 
void *kmemset(void *dest, int val, size_t len);
void *kmemcpy(void *dest, const void *src, size_t len);

#define RK_MEMSET kmemset
#define RK_MEMCPY kmemcpy

#ifdef __cplusplus
}
#endif
#endif /* RK_ITC_H */
