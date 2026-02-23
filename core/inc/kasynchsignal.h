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
 * This is intentionally NOT a message queue.
 * Entire service is optional and controlled by RK_CONF_ASR.
 *
 * Semantics:
 * - A task owns an ASR record, which is a pending bitmask + handler table.
 * - Send sets one or more pending bits (interrupt-like).
 * - Default mode coalesces repeated same-signal sends.
 * - Optional mode queues repeated same-signal sends
 *   (`RK_CONF_ASR_QUEUE_SAME_SIGNAL`).
 * - Before resuming a READY task that has pending bits, the scheduler runs the
 *   system signal-handler task first (see `kSysSigHandlerTask`).
 * - Delivery is deterministic. Order is selected by
 *   `RK_CONF_ASR_DELIVER_LOWBIT_FIRST`.
 * - Handlers execute in the system signal-handler task context (not on the
 *   target task's stack), with the scheduler locked.
 * - Optional siginfo payload support is controlled by `RK_CONF_ASR_SIGINFO`.
 * - All handler slots are initialised to an internal no-op handler.
 * - Registering `NULL` restores the no-op handler for that signal.
 * - Optional runtime warning for sends targeting only no-op handlers:
 *   `RK_CONF_ASR_WARN_UNHANDLED_SEND`.
 * - Handlers must be short and must not block/yield.
 */

RK_ERR kSignalAsynchInit(RK_ASR_RECORD *const kobj,
                         RK_TASK_SIGNAL_HANDLER *const handlerTablePtr,
                         ULONG const nSignals);

RK_ERR kSignalAsynchAttachTask(RK_TASK_HANDLE const taskHandle,
                               RK_ASR_RECORD *const kobj);

RK_ERR kSignalAsynchDetachTask(RK_TASK_HANDLE const taskHandle);

RK_ERR kSignalAsynchRegisterHandler(RK_ASR_RECORD *const kobj,
                                    ULONG const signalMask,
                                    RK_TASK_SIGNAL_HANDLER const handler);

/* Pend one or more signal bits to a task (coalescing by default). */
RK_ERR kSignalAsynch(RK_TASK_HANDLE const taskHandle,
                     ULONG const signalMask);

/*
 * Pend one or more signal bits carrying siginfo.
 *
 * If `signalMask` has multiple bits, the same siginfo value is associated
 * to each bit in this send operation. Handlers can query the payload with
 * `kSignalAsynchGetInfo()`.
 */
RK_ERR kSignalAsynchInfo(RK_TASK_HANDLE const taskHandle,
                         ULONG const signalMask,
                         ULONG const signalInfo);

/*
 * Return current siginfo associated with the signal being dispatched.
 * Returns 0 when siginfo support is disabled.
 */
ULONG kSignalAsynchGetInfo(VOID);

/*
 * Internal dequeue/dispatch helpers used by the kernel signal handler path.
 *
 * Not part of the public API. These are deliberately guarded so applications
 * cannot call into the scheduler's delivery machinery directly.
 */
#if defined(RK_SOURCE_CODE) || defined(RK_QEMU_UNIT_TEST)
RK_ERR kSignalAsynchRecv(RK_TASK_HANDLE const taskHandle,
                         ULONG *const signalMaskPtr);

#if (RK_CONF_ASR_SIGINFO == ON)
RK_ERR kSignalAsynchRecvInfo(RK_TASK_HANDLE const taskHandle,
                             ULONG *const signalMaskPtr,
                             ULONG *const signalInfoPtr);
#endif

VOID kSignalAsynchDsptchResume(RK_TASK_HANDLE const taskHandle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RK_ASYNCHSIGNAL_H */
