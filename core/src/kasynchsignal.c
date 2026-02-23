/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.11.0                                                           */
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
#include <stdio.h>

#if (RK_CONF_ASR == ON)

static inline RK_BOOL kIsValidSignalSlots_(ULONG const nSignals)
{
    return ((nSignals == 1UL) || (nSignals == 4UL) || (nSignals == 8UL) ||
            (nSignals == 16UL) || (nSignals == 24UL) || (nSignals == 32UL))
               ? RK_TRUE
               : RK_FALSE;
}

static VOID kAsrNopHandler_(RK_SIGNAL_ID const signalId)
{
    (void)signalId;
    RK_NOP
}

static inline VOID kInitHandlerRegistry_(RK_ASR_RECORD *const kobj,
                                         RK_SIGNAL_CATCHER *const handlerTablePtr,
                                         ULONG const nSignals)
{
    for (ULONG i = 0UL; i < RK_CONF_SIGNAL_QUEUE_SIZE; i++)
    {
        kobj->handlerSignalId[i] = 0U;
    }

    for (ULONG i = 0UL; i < nSignals; i++)
    {
        handlerTablePtr[i] = kAsrNopHandler_;
    }
}

static inline VOID kClearSignalQueue_(RK_ASR_RECORD *const kobj)
{
    kobj->queueHead = 0UL;
    kobj->queueTail = 0UL;
    kobj->queueCount = 0UL;

    for (ULONG i = 0UL; i < RK_CONF_SIGNAL_QUEUE_SIZE; i++)
    {
        kobj->asynchSignal[i].id = 0U;
        kobj->asynchSignal[i].val.sigval = 0UL;
        kobj->asynchSignal[i].funcPtr = kAsrNopHandler_;
    }
}

static inline VOID kPriorityInsertSignal_(RK_ASR_RECORD *const kobj,
                                          RK_SIGNAL const *const signalPtr)
{
    ULONG insertIdx = kobj->queueCount;

    for (ULONG i = 0UL; i < kobj->queueCount; i++)
    {
        if ((ULONG)kobj->asynchSignal[i].id > (ULONG)signalPtr->id)
        {
            insertIdx = i;
            break;
        }
    }

    for (ULONG i = kobj->queueCount; i > insertIdx; i--)
    {
        kobj->asynchSignal[i] = kobj->asynchSignal[i - 1UL];
    }

    kobj->asynchSignal[insertIdx] = *signalPtr;
    kobj->queueCount += 1UL;
    kobj->queueHead = 0UL;
    kobj->queueTail = kobj->queueCount;
}

static inline VOID kFifoInsertSignal_(RK_ASR_RECORD *const kobj,
                                      RK_SIGNAL const *const signalPtr)
{
    kobj->asynchSignal[kobj->queueCount] = *signalPtr;
    kobj->queueCount += 1UL;
    kobj->queueHead = 0UL;
    kobj->queueTail = kobj->queueCount;
}

static inline RK_SIGNAL_CATCHER kLookupHandler_(RK_ASR_RECORD const *const kobj,
                                                 RK_SIGNAL_ID const signalId)
{
    for (ULONG i = 0UL; i < kobj->nSignals; i++)
    {
        if (kobj->handlerSignalId[i] == signalId)
        {
            RK_SIGNAL_CATCHER handler = kobj->handlersPtr[i];
            return (handler != NULL) ? handler : kAsrNopHandler_;
        }
    }

    return (kAsrNopHandler_);
}

#if (RK_CONF_ASR_WARN_UNHANDLED_SEND == ON)
static inline RK_BOOL kHasUserHandler_(RK_ASR_RECORD const *const kobj,
                                       RK_SIGNAL_ID const signalId)
{
    return (kLookupHandler_(kobj, signalId) != kAsrNopHandler_) ? RK_TRUE : RK_FALSE;
}
#endif

static RK_SIGNAL_VAL RK_gAsrDispatchInfo = {0UL};

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

    kobj->objID = RK_ASR_KOBJ_ID;
    kobj->init = RK_TRUE;
    kobj->nSignals = nSignals;
    kobj->handlersPtr = handlerTablePtr;
    kobj->ownerPtr = NULL;

    kInitHandlerRegistry_(kobj, handlerTablePtr, nSignals);
    kClearSignalQueue_(kobj);

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

    if ((kobj->ownerPtr != NULL) && (kobj->ownerPtr != taskHandle))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_HAS_OWNER);
    }
    if (taskHandle->asrPtr != NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_HAS_OWNER);
    }

    kobj->ownerPtr = taskHandle;
    taskHandle->asrPtr = kobj;
    kClearSignalQueue_(kobj);

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchRegisterHandler(RK_ASR_RECORD *const kobj,
                                    RK_SIGNAL_ID const signalId,
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
    if (signalId == 0U)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    LONG foundIdx = -1L;
    LONG freeIdx = -1L;

    for (ULONG i = 0UL; i < kobj->nSignals; i++)
    {
        if (kobj->handlerSignalId[i] == signalId)
        {
            foundIdx = (LONG)i;
            break;
        }
        if ((freeIdx < 0L) && (kobj->handlerSignalId[i] == 0U))
        {
            freeIdx = (LONG)i;
        }
    }

    if (foundIdx >= 0L)
    {
        ULONG slot = (ULONG)foundIdx;
        if (handler == NULL)
        {
            kobj->handlerSignalId[slot] = 0U;
            kobj->handlersPtr[slot] = kAsrNopHandler_;
        }
        else
        {
            kobj->handlersPtr[slot] = handler;
        }

        RK_DMB
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if (handler == NULL)
    {
        RK_DMB
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    if (freeIdx < 0L)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_FULL);
    }

    ULONG slot = (ULONG)freeIdx;
    kobj->handlerSignalId[slot] = signalId;
    kobj->handlersPtr[slot] = handler;

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

static RK_ERR kSignalAsynchPost_(RK_TASK_HANDLE const taskHandle,
                                 RK_SIGNAL_ID const signalId,
                                 RK_SIGNAL_VAL const signalInfo)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (taskHandle == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (signalId == 0U)
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

    if (kobj->queueCount >= kobj->nSignals)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_FULL);
    }

    RK_SIGNAL_CATCHER handler = kLookupHandler_(kobj, signalId);

#if (RK_CONF_ASR_WARN_UNHANDLED_SEND == ON)
    if (kHasUserHandler_(kobj, signalId) == RK_FALSE)
    {
#if !defined(RK_QEMU_UNIT_TEST)
        if (!kIsISR())
        {
            fprintf(stderr,
                    "WARN: ASR send with no registered user handler (id=%u)\n",
                    (unsigned int)signalId);
        }
#endif
    }
#endif

    RK_SIGNAL signal = {0};
    signal.id = signalId;
    signal.val = signalInfo;
    signal.funcPtr = handler;

#if (RK_CONF_ASR_DELIVER_PRIORITY == ON)
    kPriorityInsertSignal_(kobj, &signal);
#else
    kFifoInsertSignal_(kobj, &signal);
#endif

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynch(RK_TASK_HANDLE const taskHandle,
                     RK_SIGNAL_ID const signalId)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

    return (kSignalAsynchPost_(taskHandle,
                               signalId,
                               RK_SIGNAL_VAL_FROM_ULONG(0UL)));
}

RK_ERR kSignalAsynchInfo(RK_TASK_HANDLE const taskHandle,
                         RK_SIGNAL_ID const signalId,
                         RK_SIGNAL_VAL const signalInfo)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

    return (kSignalAsynchPost_(taskHandle, signalId, signalInfo));
}

RK_SIGNAL_VAL kSignalAsynchGetInfo(VOID)
{
    return (RK_gAsrDispatchInfo);
}

static RK_ERR kSignalAsynchRecvCore_(RK_TASK_HANDLE const taskHandle,
                                     RK_SIGNAL *const signalPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if (signalPtr == NULL)
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
#endif

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
    if (kobj->queueCount == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_EMPTY);
    }

    *signalPtr = kobj->asynchSignal[0];

    for (ULONG i = 1UL; i < kobj->queueCount; i++)
    {
        kobj->asynchSignal[i - 1UL] = kobj->asynchSignal[i];
    }

    kobj->queueCount -= 1UL;
    kobj->queueHead = 0UL;
    kobj->queueTail = kobj->queueCount;
    kobj->asynchSignal[kobj->queueCount].id = 0U;
    kobj->asynchSignal[kobj->queueCount].val.sigval = 0UL;
    kobj->asynchSignal[kobj->queueCount].funcPtr = kAsrNopHandler_;

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchRecv(RK_TASK_HANDLE const taskHandle,
                         RK_SIGNAL_ID *const signalIdPtr)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (signalIdPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif

    RK_SIGNAL signal = {0};
    RK_ERR err = kSignalAsynchRecvCore_(taskHandle, &signal);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    *signalIdPtr = signal.id;
    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalAsynchRecvInfo(RK_TASK_HANDLE const taskHandle,
                             RK_SIGNAL_ID *const signalIdPtr,
                             RK_SIGNAL_VAL *const signalInfoPtr)
{
    if (RK_CONF_ASR == OFF)
    {
        return (RK_ERR_ERROR);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if ((signalIdPtr == NULL) || (signalInfoPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }
#endif

    RK_SIGNAL signal = {0};
    RK_ERR err = kSignalAsynchRecvCore_(taskHandle, &signal);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    *signalIdPtr = signal.id;
    *signalInfoPtr = signal.val;
    return (RK_ERR_SUCCESS);
}

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
    if ((kobj == NULL) || (kobj->ownerPtr != taskHandle) ||
        (kobj->queueCount == 0UL))
    {
        return;
    }

    for (;;)
    {
        RK_SIGNAL signal = {0};
        RK_ERR err = kSignalAsynchRecvCore_(taskHandle, &signal);
        if (err == RK_ERR_SIGNALQ_EMPTY)
        {
            break;
        }
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }

        RK_SIGNAL_CATCHER handler = signal.funcPtr;
        if (handler == NULL)
        {
            handler = kAsrNopHandler_;
        }

        if (handler != kAsrNopHandler_)
        {
            kSchLock();
            RK_gAsrDispatchInfo = signal.val;
            handler(signal.id);
            RK_gAsrDispatchInfo.sigval = 0UL;
            kSchUnlock();
        }
    }
}

#endif
