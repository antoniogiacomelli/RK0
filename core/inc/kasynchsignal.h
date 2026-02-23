/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.1                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_ASYNCHSIGNAL_H
#define RK_ASYNCHSIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kcommondefs.h>
#include <kobjs.h>

/*
 * Asynchronous task signals (ASR record)
 *
 * A task owns one ASR record with:
 * - handler registry mapping numeric signal IDs to handlers,
 * - queue of RK_OBJ_SIGNAL entries.
 *
 * Queue order:
 * - RK_CONF_ASR_DELIVER_LOWBIT_FIRST == ON:
 *   lower numeric signal ID has higher priority.
 * - RK_CONF_ASR_DELIVER_LOWBIT_FIRST == OFF:
 *   strict FIFO by send order.
 *
 * Each queued RK_OBJ_SIGNAL carries:
 * - id      : signal ID (1..UINT_MAX),
 * - val     : RK_SIGNAL_VAL payload (ULONG or pointer),
 * - funcPtr : handler snapshot used at dispatch.
 */

RK_ERR kSignalAsynchInit(RK_ASR_RECORD *const kobj,
                         RK_TASK_SIGNAL_HANDLER *const handlerTablePtr,
                         ULONG const nSignals);

RK_ERR kSignalAsynchAttachTask(RK_TASK_HANDLE const taskHandle,
                               RK_ASR_RECORD *const kobj);

RK_ERR kSignalAsynchRegisterHandler(RK_ASR_RECORD *const kobj,
                                    RK_SIGNAL_ID const signalId,
                                    RK_TASK_SIGNAL_HANDLER const handler);

/* Queue one asynchronous signal occurrence. */
RK_ERR kSignalAsynch(RK_TASK_HANDLE const taskHandle,
                     RK_SIGNAL_ID const signalId);

/* Queue one asynchronous signal occurrence carrying payload. */
RK_ERR kSignalAsynchInfo(RK_TASK_HANDLE const taskHandle,
                         RK_SIGNAL_ID const signalId,
                         RK_SIGNAL_VAL const signalInfo);

#ifndef kSignalAsynchInfoVal
#define kSignalAsynchInfoVal(taskHandle, signalId, value)                      \
    kSignalAsynchInfo((taskHandle), (signalId),                                \
                      RK_SIGNAL_VAL_FROM_ULONG(value))
#endif

#ifndef kSignalAsynchInfoPtr
#define kSignalAsynchInfoPtr(taskHandle, signalId, ptrValue)                   \
    kSignalAsynchInfo((taskHandle), (signalId),                                \
                      RK_SIGNAL_VAL_FROM_PTR(ptrValue))
#endif

/* Return payload of the signal currently being dispatched. */
RK_SIGNAL_VAL kSignalAsynchGetInfo(VOID);

/*
 * Internal dequeue/dispatch helpers used by the kernel signal handler path.
 *
 * Not part of the public API. These are deliberately guarded so applications
 * cannot call into the scheduler's delivery machinery directly.
 */
#if defined(RK_SOURCE_CODE) || defined(RK_QEMU_UNIT_TEST)
RK_ERR kSignalAsynchRecv(RK_TASK_HANDLE const taskHandle,
                         RK_SIGNAL_ID *const signalIdPtr);

RK_ERR kSignalAsynchRecvInfo(RK_TASK_HANDLE const taskHandle,
                             RK_SIGNAL_ID *const signalIdPtr,
                             RK_SIGNAL_VAL *const signalInfoPtr);

VOID kSignalAsynchDsptchResume(RK_TASK_HANDLE const taskHandle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RK_ASYNCHSIGNAL_H */
