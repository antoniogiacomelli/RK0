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
 * Header for the Module: PARTITION MEMORY (Memory Pools)
 * 
 ******************************************************************************/
#ifndef RK_MEM_H
#define RK_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

RK_ERR kMemInit(RK_MEM* const, VOID *, ULONG const, ULONG);
VOID *kMemAlloc(RK_MEM* const);
RK_ERR kMemFree(RK_MEM* const, VOID *);

#ifdef __cplusplus
}
#endif

#endif
