/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.13.2                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_DSIGNAL_H
#define RK_DSIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kcommondefs.h>
#include <kobjs.h>

#if (RK_CONF_DSIGNAL == ON)

RK_ERR kDSignalInit(RK_DS_RECORD *const,
                    RK_DSIGNAL_CATCHER *const,
                    ULONG const);

RK_ERR kDSignalAttachTask(RK_TASK_HANDLE const,
                          RK_DS_RECORD *const);

RK_ERR kDSignalRegisterHandler(RK_DS_RECORD *const,
                               RK_DSIGNAL_ID const,
                               RK_DSIGNAL_CATCHER const);

/* Queue one asynchronous signal occurrence. */
RK_ERR kDSignal(RK_TASK_HANDLE const,
                RK_DSIGNAL_ID const,
                RK_DSIGNAL_INFO const);

#define kDSignalVal(taskHandle, signalId, value)                               \
    kDSignal((taskHandle), (signalId), RK_DSIGNAL_VAL_FROM_ULONG(value))

#define kDSignalPtr(taskHandle, signalId, ptrValue)                            \
    kDSignal((taskHandle), (signalId), RK_DSIGNAL_VAL_FROM_PTR(ptrValue))

/* Return payload of the signal currently being dispatched. */
RK_DSIGNAL_INFO kDSignalGetInfo(VOID);

/*
 * Internal dequeue/dispatch helpers used by the kernel signal handler path.
 *
 * Not part of the public API. These are deliberately guarded so applications
 * cannot call into the scheduler's delivery machinery directly.
 */
#if defined(RK_SOURCE_CODE) || defined(RK_QEMU_UNIT_TEST)
RK_ERR kDSignalRecv(RK_TASK_HANDLE const taskHandle,
                    RK_DSIGNAL_ID *const signalIdPtr);

RK_ERR kDSignalRecvInfo(RK_TASK_HANDLE const taskHandle,
                        RK_DSIGNAL_ID *const signalIdPtr,
                        RK_DSIGNAL_INFO *const signalInfoPtr);

VOID kDSignalDispatchResume(RK_TASK_HANDLE const taskHandle);
#endif /* RK_CONF_DSIGNAL == ON */
#endif
#ifdef __cplusplus
}
#endif

#endif /* RK_DSIGNAL_H */
