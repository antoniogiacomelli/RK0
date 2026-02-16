/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.18                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_SYSTASKS_H
#define RK_SYSTASKS_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif
extern RK_TASK_HANDLE RK_gPostProcTaskHandle;
extern RK_TASK_HANDLE RK_gIdleTaskHandle;

/* Post-processing deferral threshold:
 * if an ISR would wake/flush more than this many tasks, the work is deferred.
 */
#ifndef RK_POSTPROC_DEFER_WAITERS_THRESHOLD
#define RK_POSTPROC_DEFER_WAITERS_THRESHOLD (3U)
#endif

#define RK_POSTPROC_JOB_SEMA_FLUSH      ((UINT)0x1)
#define RK_POSTPROC_JOB_SLEEPQ_WAKE     ((UINT)0x2)
#define RK_POSTPROC_JOB_MESGQ_RESET     ((UINT)0x3)

void IdleTask(void*);
void PostProcSysTask(void*);
RK_ERR kPostProcJobEnq(UINT jobType, VOID *const objPtr, UINT nTasks);
#ifdef __cplusplus
 }
#endif
#endif /* KSYSTASKS_H */
