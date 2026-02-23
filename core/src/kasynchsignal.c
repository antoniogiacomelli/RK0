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
/* COMPONENT: ASYNCHRONOUS SIGNALS (ASR)                                      */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kasynchsignal.h>
#include <ksch.h>
#include <kstring.h>
#include <stdio.h>

#if (RK_CONF_ASR == ON)

#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON) && (RK_CONF_ASR_QUEUE_MAX_PER_SIGNAL == 0UL)
#error "RK_CONF_ASR_QUEUE_MAX_PER_SIGNAL must be > 0 when queueing is enabled."
#endif

static inline RK_BOOL kIsSingleBit_(ULONG const mask)
{
    /* Used to validate handler registration (one handler per bit). */
    return (mask != 0UL) && ((mask & (mask - 1UL)) == 0UL);
}

static inline ULONG kAllowedMask_(ULONG const nSignals)
{
    ULONG const wordBits = (ULONG)(8UL * (ULONG)sizeof(ULONG));
    if (nSignals >= wordBits)
    {
        return (~0UL);
    }
    /* (1<<nSignals)-1, safe because nSignals < wordBits. */
    return ((1UL << nSignals) - 1UL);
}

static inline RK_BOOL kIsValidSignalSlots_(ULONG const nSignals)
{
    return ((nSignals == 1UL) || (nSignals == 4UL) || (nSignals == 8UL) ||
            (nSignals == 16UL) || (nSignals == 24UL) || (nSignals == 32UL))
               ? RK_TRUE
               : RK_FALSE;
}

static inline ULONG kLowestSetBit_(ULONG const mask)
{
    return (mask & (~mask + 1UL));
}

static inline ULONG kHighestSetBit_(ULONG mask)
{
    /* mask must be not 0 */
    ULONG bit = 1UL;
    while ((mask >>= 1U) != 0UL)
    {
        bit <<= 1U;
    }
    return (bit);
}

static inline ULONG kNextDeliveryBit_(ULONG const pendingMask)
{
#if (RK_CONF_ASR_DELIVER_LOWBIT_FIRST == ON)
    return kLowestSetBit_(pendingMask);
#else
    return kHighestSetBit_(pendingMask);
#endif
}

static VOID kAsrNopHandler_(ULONG const signalMask)
{
    (void)signalMask;
    RK_NOP
}

static inline VOID kInitHandlerTable_(RK_TASK_SIGNAL_HANDLER *const handlerTablePtr,
                                      ULONG const nSignals)
{
    for (ULONG i = 0UL; i < nSignals; i++)
    {
        handlerTablePtr[i] = kAsrNopHandler_;
    }
}

#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
static inline VOID kClearPendingCounts_(RK_ASR_RECORD *const kobj)
{
    for (ULONG i = 0UL; i < RK_CONF_SIGNAL_QUEUE_SIZE; i++)
    {
        kobj->pendingCount[i] = 0UL;
    }
}
#endif

#if (RK_CONF_ASR_SIGINFO == ON) && (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
static inline VOID kClearPendingSigInfo_(RK_ASR_RECORD *const kobj)
{
    for (ULONG i = 0UL; i < RK_CONF_SIGNAL_QUEUE_SIZE; i++)
    {
        for (ULONG j = 0UL; j < RK_CONF_ASR_QUEUE_MAX_PER_SIGNAL; j++)
        {
            kobj->pendingInfoQ[i][j] = 0UL;
        }
        kobj->pendingInfoHead[i] = 0UL;
        kobj->pendingInfoTail[i] = 0UL;
    }
}
#elif (RK_CONF_ASR_SIGINFO == ON)
static inline VOID kClearPendingSigInfo_(RK_ASR_RECORD *const kobj)
{
    for (ULONG i = 0UL; i < RK_CONF_SIGNAL_QUEUE_SIZE; i++)
    {
        kobj->pendingInfo[i] = 0UL;
    }
}
#endif

#if defined(__GNUC__)
static inline UINT kBitIndex_(ULONG const bit)
{
    /* bit must be non-zero (exactly one bit set). */
    return (UINT)__builtin_ctzl(bit);
}
#else
static inline UINT kBitIndex_(ULONG const bit)
{
    UINT idx = 0U;
    ULONG v = bit;
    while ((v & 1UL) == 0UL)
    {
        v >>= 1;
        idx++;
    }
    return idx;
}
#endif

#if (RK_CONF_ASR_SIGINFO == ON)
/* Siginfo bound to the signal currently being dispatched by ASR. */
static ULONG RK_gAsrDispatchInfo = 0UL;
#endif

#if (RK_CONF_ASR_WARN_UNHANDLED_SEND == ON)
static inline RK_BOOL kHasAnyUserHandler_(RK_ASR_RECORD const *const kobj,
                                          ULONG signalMask)
{
    while (signalMask != 0UL)
    {
        ULONG bit = kLowestSetBit_(signalMask);
        signalMask &= ~bit;
        UINT idx = kBitIndex_(bit);
        if ((kobj->handlersPtr != NULL) &&
            (idx < (UINT)kobj->nSignals) &&
            (kobj->handlersPtr[idx] != kAsrNopHandler_))
        {
            return (RK_TRUE);
        }
    }
    return (RK_FALSE);
}
#endif

RK_ERR kSignalAsynchInit(RK_ASR_RECORD *const kobj,
                         RK_TASK_SIGNAL_HANDLER *const handlerTablePtr,
                         ULONG const nSignals)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (handlerTablePtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if ((nSignals > RK_CONF_SIGNAL_QUEUE_SIZE) ||
        (kIsValidSignalSlots_(nSignals) == RK_FALSE))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
#endif

    /* Default all slots to an explicit no-op handler. */
    kInitHandlerTable_(handlerTablePtr, nSignals);

    kobj->objID = RK_ASR_KOBJ_ID;
    kobj->init = RK_TRUE;
    kobj->nSignals = nSignals;
    kobj->pending = 0UL;
#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
    kClearPendingCounts_(kobj);
#endif
#if (RK_CONF_ASR_SIGINFO == ON)
    kClearPendingSigInfo_(kobj);
#endif
    kobj->handlersPtr = handlerTablePtr;
    kobj->ownerPtr = NULL;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchAttachTask(RK_TASK_HANDLE const taskHandle,
                               RK_ASR_RECORD *const kobj)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((taskHandle == NULL) || (kobj == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_ASR_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    /*
     * Only one owner task is allowed per ASR record.
     *
     * If the task already had a different ASR attached, detach it here to
     * avoid leaving ownership/pending state behind.
     */
    if ((kobj->ownerPtr != NULL) && (kobj->ownerPtr != taskHandle))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_HAS_OWNER);
    }

    RK_ASR_RECORD *oldAsrPtr = taskHandle->asrPtr;
    if ((oldAsrPtr != NULL) && (oldAsrPtr != kobj) &&
        (oldAsrPtr->ownerPtr == taskHandle))
    {
        oldAsrPtr->ownerPtr = NULL;
        oldAsrPtr->pending = 0UL;
#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
        kClearPendingCounts_(oldAsrPtr);
#endif
#if (RK_CONF_ASR_SIGINFO == ON)
        kClearPendingSigInfo_(oldAsrPtr);
#endif
    }

    kobj->ownerPtr = taskHandle;
    taskHandle->asrPtr = kobj;
    /* Avoid delivering stale pending signals across re-attach. */
    kobj->pending = 0UL;
#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
    kClearPendingCounts_(kobj);
#endif
#if (RK_CONF_ASR_SIGINFO == ON)
    kClearPendingSigInfo_(kobj);
#endif
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchDetachTask(RK_TASK_HANDLE const taskHandle)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif

    /* Best-effort detach: clears ownership and drops any pending bits. */
    RK_ASR_RECORD *kobj = taskHandle->asrPtr;
    if (kobj != NULL)
    {
        if (kobj->ownerPtr == taskHandle)
        {
            kobj->ownerPtr = NULL;
            kobj->pending = 0UL;
#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
            kClearPendingCounts_(kobj);
#endif
#if (RK_CONF_ASR_SIGINFO == ON)
            kClearPendingSigInfo_(kobj);
#endif
        }
        taskHandle->asrPtr = NULL;
        RK_DMB
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchRegisterHandler(RK_ASR_RECORD *const kobj,
                                    ULONG const signalMask,
                                    RK_TASK_SIGNAL_HANDLER const handler)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_ASR_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
    if (!kIsSingleBit_(signalMask))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    /* Reject masks outside the configured number of signals. */
    ULONG allowed = kAllowedMask_(kobj->nSignals);
    if ((signalMask & ~allowed) != 0UL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    UINT idx = kBitIndex_(signalMask);
    if (kobj->handlersPtr != NULL)
    {
        /*
         * NULL unregisters and reverts that signal slot back to no-op.
         * This keeps every ASR handler slot always initialised.
         */
        kobj->handlersPtr[idx] = (handler != NULL) ? handler : kAsrNopHandler_;
    }
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

static RK_ERR kSignalAsynchPost_(RK_TASK_HANDLE const taskHandle,
                                 ULONG const signalMask,
                                 ULONG const signalInfo)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ASR_SIGINFO == OFF)
    (void)signalInfo;
#endif

#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (signalMask == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_ASR_RECORD *kobj = taskHandle->asrPtr;
    if (kobj == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj->objID != RK_ASR_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (kobj->ownerPtr != taskHandle)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

    ULONG allowed = kAllowedMask_(kobj->nSignals);
    if ((signalMask & ~allowed) != 0UL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

#if (RK_CONF_ASR_WARN_UNHANDLED_SEND == ON)
    if (kHasAnyUserHandler_(kobj, signalMask) == RK_FALSE)
    {
#if !defined(RK_QEMU_UNIT_TEST)
        if (!kIsISR())
        {
            fprintf(stderr,
                    "WARN: ASR send with no registered user handler (mask=0x%08lx)\n",
                    (unsigned long)signalMask);
        }
#endif
    }
#endif

#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
    /* Pre-check capacity so the operation remains all-or-nothing. */
    ULONG bits = signalMask;
    while (bits != 0UL)
    {
        ULONG bit = kLowestSetBit_(bits);
        bits &= ~bit;
        UINT idx = kBitIndex_(bit);
        if (kobj->pendingCount[idx] >= RK_CONF_ASR_QUEUE_MAX_PER_SIGNAL)
        {
            RK_CR_EXIT
            return (RK_ERR_SIGNALQ_FULL);
        }
    }

    bits = signalMask;
    while (bits != 0UL)
    {
        ULONG bit = kLowestSetBit_(bits);
        bits &= ~bit;
        UINT idx = kBitIndex_(bit);
#if (RK_CONF_ASR_SIGINFO == ON)
        ULONG tail = kobj->pendingInfoTail[idx];
        kobj->pendingInfoQ[idx][tail] = signalInfo;
        kobj->pendingInfoTail[idx] =
            (tail + 1UL) % RK_CONF_ASR_QUEUE_MAX_PER_SIGNAL;
#endif
        kobj->pendingCount[idx] += 1UL;
    }
#elif (RK_CONF_ASR_SIGINFO == ON)
    /* Coalescing mode keeps the latest siginfo for each pending bit. */
    ULONG bits = signalMask;
    while (bits != 0UL)
    {
        ULONG bit = kLowestSetBit_(bits);
        bits &= ~bit;
        UINT idx = kBitIndex_(bit);
        kobj->pendingInfo[idx] = signalInfo;
    }
#endif

    /* Pend bits for deferred dispatch by the system signal-handler task. */
    kobj->pending |= signalMask;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynch(RK_TASK_HANDLE const taskHandle,
                     ULONG const signalMask)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }
    return (kSignalAsynchPost_(taskHandle, signalMask, 0UL));
}

RK_ERR kSignalAsynchInfo(RK_TASK_HANDLE const taskHandle,
                         ULONG const signalMask,
                         ULONG const signalInfo)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }
    return (kSignalAsynchPost_(taskHandle, signalMask, signalInfo));
}

ULONG kSignalAsynchGetInfo(VOID)
{
#if (RK_CONF_ASR_SIGINFO == ON)
    return (RK_gAsrDispatchInfo);
#else
    return (0UL);
#endif
}

static RK_ERR kSignalAsynchRecvCore_(RK_TASK_HANDLE const taskHandle,
                                     ULONG *const signalMaskPtr
#if (RK_CONF_ASR_SIGINFO == ON)
                                     ,
                                     ULONG *const signalInfoPtr
#endif
)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (signalMaskPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kIsISR() && (taskHandle == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#if (RK_CONF_ASR_SIGINFO == ON)
    if (signalInfoPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
#endif

    /*
     * Internal helper: return exactly one pending bit per call.
     * Used by unit tests and by the kernel's signal-handler path.
     */
    RK_TASK_HANDLE targetTask = (taskHandle != NULL) ? taskHandle : RK_gRunPtr;
    if (targetTask == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    RK_ASR_RECORD *kobj = targetTask->asrPtr;

    if (kobj == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj->objID != RK_ASR_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
    if (kobj->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }
#endif

    if (kobj->ownerPtr != targetTask)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

    ULONG pending = kobj->pending;
    if (pending == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_EMPTY);
    }

    ULONG bit = kNextDeliveryBit_(pending);
#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL == ON)
    UINT idx = kBitIndex_(bit);
#if (RK_CONF_ASR_SIGINFO == ON)
    ULONG head = kobj->pendingInfoHead[idx];
    ULONG signalInfo = kobj->pendingInfoQ[idx][head];
    kobj->pendingInfoHead[idx] =
        (head + 1UL) % RK_CONF_ASR_QUEUE_MAX_PER_SIGNAL;
#endif
    ULONG nQueued = kobj->pendingCount[idx];
    if (nQueued > 1UL)
    {
        kobj->pendingCount[idx] = (nQueued - 1UL);
    }
    else
    {
        kobj->pendingCount[idx] = 0UL;
        kobj->pending = (pending & ~bit);
    }
#elif (RK_CONF_ASR_SIGINFO == ON)
    UINT idx = kBitIndex_(bit);
    ULONG signalInfo = kobj->pendingInfo[idx];
    kobj->pendingInfo[idx] = 0UL;
#else
    kobj->pending = (pending & ~bit);
#endif

#if (RK_CONF_ASR_SIGINFO == ON)
#if (RK_CONF_ASR_QUEUE_SAME_SIGNAL != ON)
    kobj->pending = (pending & ~bit);
#endif
    *signalInfoPtr = signalInfo;
#endif

    *signalMaskPtr = bit;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchRecv(RK_TASK_HANDLE const taskHandle,
                         ULONG *const signalMaskPtr)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }
#if (RK_CONF_ASR_SIGINFO == ON)
    ULONG signalInfoDummy = 0UL;
    return (kSignalAsynchRecvCore_(taskHandle, signalMaskPtr, &signalInfoDummy));
#else
    return (kSignalAsynchRecvCore_(taskHandle, signalMaskPtr));
#endif
}

#if (RK_CONF_ASR_SIGINFO == ON)
RK_ERR kSignalAsynchRecvInfo(RK_TASK_HANDLE const taskHandle,
                             ULONG *const signalMaskPtr,
                             ULONG *const signalInfoPtr)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }
    return (kSignalAsynchRecvCore_(taskHandle, signalMaskPtr, signalInfoPtr));
}
#endif

VOID kSignalAsynchDsptchResume(RK_TASK_HANDLE const taskHandle)
{
    if (RK_CONF_ASR == OFF)
    {
        return;
    }

    if (taskHandle == NULL)
    {
        return;
    }

    RK_ASR_RECORD *kobj = taskHandle->asrPtr;
    if ((kobj == NULL) || (kobj->ownerPtr != taskHandle) || (kobj->pending == 0UL))
    {
        return;
    }

    /* Drain and dispatch one occurrence at a time until the ASR record is empty. */
    for (;;)
    {
        ULONG bit = 0UL;
#if (RK_CONF_ASR_SIGINFO == ON)
        ULONG signalInfo = 0UL;
        RK_ERR err = kSignalAsynchRecvInfo(taskHandle, &bit, &signalInfo);
#else
        RK_ERR err = kSignalAsynchRecv(taskHandle, &bit);
#endif
        if (err == RK_ERR_SIGNALQ_EMPTY)
        {
            break;
        }
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }

        UINT idx = kBitIndex_(bit);
        RK_TASK_SIGNAL_HANDLER handler = kAsrNopHandler_;
        if ((kobj->handlersPtr != NULL) && (idx < (UINT)kobj->nSignals))
        {
            handler = kobj->handlersPtr[idx];
        }

        if (handler != kAsrNopHandler_)
        {
            /*
             * Handlers execute with the scheduler locked so delivery is
             * atomic relative to preemption/yield. This keeps the ASR
             * model close to "deferred interrupt" semantics.
             */
            kSchLock();
#if (RK_CONF_ASR_SIGINFO == ON)
            RK_gAsrDispatchInfo = signalInfo;
#endif
            handler(bit);
#if (RK_CONF_ASR_SIGINFO == ON)
            RK_gAsrDispatchInfo = 0UL;
#endif
            kSchUnlock();
        }
    }
}
#endif
