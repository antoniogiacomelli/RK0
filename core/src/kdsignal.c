/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.13.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: DEFERRED TASK SIGNALS (DTS)                                     */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kdsignal.h>
#include <ksch.h>
#include <stdio.h>

#if (RK_CONF_DSIGNAL == ON)

static VOID kDSignalNopHandler_(RK_DSIGNAL_ID const signalId, RK_DSIGNAL_INFO const info)
{
    (void)signalId;
    (void)info;
    RK_NOP
}

static inline VOID kInitHandlerRegistry_(RK_DS_RECORD *const kobj,
                                         RK_DSIGNAL_CATCHER *const handlerTablePtr,
                                         ULONG const handlerCount)
{
    (void)kobj;
    for (ULONG i = 0UL; i < handlerCount; i++)
    {
        handlerTablePtr[i] = kDSignalNopHandler_;
    }
}

static inline VOID kClearSignalQueue_(RK_DS_RECORD *const kobj)
{
    kobj->qHead = 0UL;
    kobj->qTail = 0UL;
    kobj->qCount = 0UL;

    for (ULONG i = 0UL; i < kobj->qDepth; i++)
    {
        kobj->queue[i].sigID = 0U;
        kobj->queue[i].val.val = 0UL;
    }
}

static inline VOID kFifoInsertSignal_(RK_DS_RECORD *const kobj,
                                      kDTSRecord const *const signalPtr)
{
    kobj->queue[kobj->qCount] = *signalPtr;
    kobj->qCount += 1UL;
    kobj->qHead = 0UL;
    kobj->qTail = kobj->qCount;
}

static inline RK_DSIGNAL_CATCHER kLookupHandler_(RK_DS_RECORD const *const kobj,
                                                 RK_DSIGNAL_ID const signalId)
{
    if ((signalId == 0U) || (signalId > (RK_DSIGNAL_ID)RK_MAX_SIGNALS))
    {
        return (kDSignalNopHandler_);
    }

    RK_DSIGNAL_CATCHER handler = kobj->handlers[signalId - 1UL];
    return (handler != NULL) ? handler : kDSignalNopHandler_;
}

#if (RK_CONF_DSIGNAL_WARN_UNHANDLED_SEND == ON)
static inline RK_BOOL kHasUserHandler_(RK_DS_RECORD const *const kobj,
                                       RK_DSIGNAL_ID const signalId)
{
    return (kLookupHandler_(kobj, signalId) != kDSignalNopHandler_) ? RK_TRUE : RK_FALSE;
}
#endif

static RK_DSIGNAL_INFO RK_gDSignalDispatchInfo = {0UL};

RK_ERR kDSignalInit(RK_DS_RECORD *const kobj,
                    RK_DSIGNAL_CATCHER *const handlerTablePtr,
                    ULONG const qDepth)
{
    if (RK_CONF_DSIGNAL == OFF)
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
    if ((qDepth < 1UL) || (qDepth > RK_CONF_DSIGNAL_QUEUE_SIZE))
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

    kobj->objID = RK_DSIGNAL_KOBJ_ID;
    kobj->init = RK_TRUE;
    kobj->qDepth = qDepth;
    kobj->ownerPtr = NULL;

    kInitHandlerRegistry_(kobj, kobj->handlers, RK_MAX_SIGNALS);
    for (ULONG i = 0UL; i < RK_MAX_SIGNALS; i++)
    {
            kobj->handlers[i] = handlerTablePtr[i];
    }
    
    kClearSignalQueue_(kobj);

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kDSignalAttachTask(RK_TASK_HANDLE const taskHandle,
                               RK_DS_RECORD *const kobj)
{
    if (RK_CONF_DSIGNAL == OFF)
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
    if (kobj->objID != RK_DSIGNAL_KOBJ_ID)
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
    if (taskHandle->dsPtr != NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_HAS_OWNER);
    }

    kobj->ownerPtr = taskHandle;
    taskHandle->dsPtr = kobj;
    kClearSignalQueue_(kobj);

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kDSignalRegisterHandler(RK_DS_RECORD *const kobj,
                               RK_DSIGNAL_ID const signalId,
                               RK_DSIGNAL_CATCHER const handler)
{
    if (RK_CONF_DSIGNAL == OFF)
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
    if (kobj->objID != RK_DSIGNAL_KOBJ_ID)
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

    if (signalId > (RK_DSIGNAL_ID)RK_MAX_SIGNALS)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    ULONG idx = (ULONG)(signalId - 1UL);
    if (handler == NULL)
    {
        kobj->handlers[idx] = kDSignalNopHandler_;
    }
    else
    {
        kobj->handlers[idx] = handler;
    }

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

static RK_ERR kDSignalPost_(RK_TASK_HANDLE const taskHandle,
                             RK_DSIGNAL_ID const signalId,
                             RK_DSIGNAL_INFO const signalInfo)
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

    RK_DS_RECORD *kobj = taskHandle->dsPtr;
    if (kobj == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj->objID != RK_DSIGNAL_KOBJ_ID)
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

    if (kobj->qCount >= kobj->qDepth)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_FULL);
    }

    kDTSRecord signal = {0};
    signal.sigID = signalId;
    signal.val = signalInfo;

    kFifoInsertSignal_(kobj, &signal);

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kDSignal(RK_TASK_HANDLE const taskHandle,
                RK_DSIGNAL_ID const signalId,
                RK_DSIGNAL_INFO const signalInfo)
{
    if (RK_CONF_DSIGNAL == OFF)
    {
        return (RK_ERR_ERROR);
    }

    return (kDSignalPost_(taskHandle, signalId, signalInfo));
}

RK_DSIGNAL_INFO kDSignalGetInfo(VOID)
{
    return (RK_gDSignalDispatchInfo);
}

static RK_ERR kDSignalRecvCore_(RK_TASK_HANDLE const taskHandle,
                                     kDTSRecord *const signalPtr)
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

    RK_DS_RECORD *kobj = targetTask->dsPtr;
    if (kobj == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj->objID != RK_DSIGNAL_KOBJ_ID)
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
    if (kobj->qCount == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_EMPTY);
    }

    *signalPtr = kobj->queue[0];

    for (ULONG i = 1UL; i < kobj->qCount; i++)
    {
        kobj->queue[i - 1UL] = kobj->queue[i];
    }

    kobj->qCount -= 1UL;
    kobj->qHead = 0UL;
    kobj->qTail = kobj->qCount;
    kobj->queue[kobj->qCount].sigID = 0U;
    kobj->queue[kobj->qCount].val.val = 0UL;

    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kDSignalRecv(RK_TASK_HANDLE const taskHandle,
                     RK_DSIGNAL_ID *const signalIdPtr)
{
    if (RK_CONF_DSIGNAL == OFF)
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

    kDTSRecord signal = {0};
    RK_ERR err = kDSignalRecvCore_(taskHandle, &signal);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    *signalIdPtr = signal.sigID;
    return (RK_ERR_SUCCESS);
}

RK_ERR kDSignalRecvInfo(RK_TASK_HANDLE const taskHandle,
                         RK_DSIGNAL_ID *const signalIdPtr,
                         RK_DSIGNAL_INFO *const signalInfoPtr)
{
    if (RK_CONF_DSIGNAL == OFF)
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

    kDTSRecord signal = {0};
    RK_ERR err = kDSignalRecvCore_(taskHandle, &signal);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    *signalIdPtr = signal.sigID;
    *signalInfoPtr = signal.val;
    return (RK_ERR_SUCCESS);
}

VOID kDSignalDispatchResume(RK_TASK_HANDLE const taskHandle)
{
    if (RK_CONF_DSIGNAL == OFF)
    {
        return;
    }
    if (taskHandle == NULL)
    {
        return;
    }

    RK_DS_RECORD *kobj = taskHandle->dsPtr;
    if ((kobj == NULL) || (kobj->ownerPtr != taskHandle) ||
        (kobj->qCount == 0UL))
    {
        return;
    }

    for (;;)
    {
        kDTSRecord signal = {0};
        RK_ERR err = kDSignalRecvCore_(taskHandle, &signal);
        if (err == RK_ERR_SIGNALQ_EMPTY)
        {
            break;
        }
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }

        RK_DSIGNAL_CATCHER handler = kLookupHandler_(kobj, signal.sigID);

        if (handler != kDSignalNopHandler_)
        {
            kSchLock();
            RK_gDSignalDispatchInfo = signal.val;
            handler(signal.sigID, signal.val);
            RK_gDSignalDispatchInfo.val = 0UL;
            kSchUnlock();
        }
    }
}

#endif
