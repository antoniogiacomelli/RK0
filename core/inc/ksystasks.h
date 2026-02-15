/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.17                                              */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_SYSTASKS_H
#define RK_SYSTASKS_H

#include <kenv.h>
#include <kdefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif
extern RK_TASK_HANDLE RK_gPostProcTaskHandle;
extern RK_TASK_HANDLE RK_gIdleTaskHandle;

#define RK_POSTPROC_JOB_SEMA_FLUSH      ((UINT)0x1)
#define RK_POSTPROC_JOB_SLEEPQ_WAKE     ((UINT)0x2)

void IdleTask(void*);
void PostProcSysTask(void*);
RK_ERR kPostProcJobEnq(UINT jobType, VOID *const objPtr, UINT nTasks);
#ifdef __cplusplus
 }
#endif
#endif /* KSYSTASKS_H */
