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
 * Header for the Module: SYSTEM TASKS
 * 
 ******************************************************************************/
#ifndef RK_SYSTASKS_H
#define RK_SYSTASKS_H
#ifdef __cplusplus
extern "C" {
#endif
#include <kservices.h>
extern RK_TASK_HANDLE timTaskHandle;
extern RK_TASK_HANDLE idleTaskHandle;

void IdleTask(void*);
void TimerHandlerTask(void*);
BOOL kTimerHandler(void*);
#ifdef __cplusplus
 }
#endif
#endif /* KSYSTASKS_H */
