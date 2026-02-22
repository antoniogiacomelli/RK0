/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_SIGNALQ_H
#define RK_SIGNALQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

RK_ERR kSignalQueueInit(RK_TASK_SIGNAL_QUEUE *const kobj,
                            RK_TASK_SIGNAL *const bufPtr,
                            ULONG const depth);

RK_ERR kSignalQueueAttachTask(RK_TASK_HANDLE const taskHandle,
                              RK_TASK_SIGNAL_QUEUE *const kobj);

RK_ERR kSignalQueueDetachTask(RK_TASK_HANDLE const taskHandle);

RK_ERR kSignalQueueSend(RK_TASK_HANDLE const taskHandle,
                   ULONG const eventID,
                   RK_TASK_HANDLE const senderHandle,
                   VOID *const argsPtr,
                   RK_TASK_SIGNAL_HANDLER const handler);

RK_ERR kSignalQueueRecv(RK_TASK_HANDLE const taskHandle,
                   RK_TASK_SIGNAL *const signalPtr);

VOID kSignalQueueDsptchResume(RK_TASK_HANDLE const taskHandle);

#ifndef RK_DECLARE_TASK_SIGNAL_QUEUE
#define RK_DECLARE_TASK_SIGNAL_QUEUE(NAME, DEPTH)                           \
    RK_TASK_SIGNAL NAME##_buf[(DEPTH)] K_ALIGN(4);                          \
    RK_TASK_SIGNAL_QUEUE NAME;
#endif

#ifdef __cplusplus
}
#endif

#endif
