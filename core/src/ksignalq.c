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
/* COMPONENT: TASK SIGNAL QUEUE                                               */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ksignalq.h>
#include <ksch.h>
#include <kstring.h>

static ULONG kTaskSignalQNext_(ULONG const pos, ULONG const depth)
{
    ULONG next = pos + 1UL;
    if (next >= depth)
    {
        next = 0UL;
    }
    return (next);
}

RK_ERR kSignalQueueInit(RK_TASK_SIGNAL_QUEUE *const kobj,
                            RK_TASK_SIGNAL *const bufPtr,
                            ULONG const depth)
{
    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj == NULL) || (bufPtr == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if ((depth == 0UL) || (depth > RK_CONF_SIGNAL_QUEUE_DEPTH))
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

    RK_MEMSET(bufPtr, 0, (size_t)(depth * (ULONG)sizeof(RK_TASK_SIGNAL)));
    kobj->objID = RK_SIGNALQ_KOBJ_ID;
    kobj->init = RK_TRUE;
    kobj->depth = depth;
    kobj->count = 0UL;
    kobj->head = 0UL;
    kobj->tail = 0UL;
    kobj->bufPtr = bufPtr;
    kobj->ownerPtr = NULL;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalQueueAttachTask(RK_TASK_HANDLE const taskHandle,
                              RK_TASK_SIGNAL_QUEUE *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)
    if ((taskHandle == NULL) || (kobj == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kobj->objID != RK_SIGNALQ_KOBJ_ID)
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

    RK_TASK_SIGNAL_QUEUE *oldQueuePtr = taskHandle->signalQueuePtr;
    if ((oldQueuePtr != NULL) && (oldQueuePtr != kobj) &&
        (oldQueuePtr->ownerPtr == taskHandle))
    {
        oldQueuePtr->ownerPtr = NULL;
    }

    kobj->ownerPtr = taskHandle;
    taskHandle->signalQueuePtr = kobj;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalQueueDetachTask(RK_TASK_HANDLE const taskHandle)
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
#endif

    RK_TASK_SIGNAL_QUEUE *kobj = taskHandle->signalQueuePtr;
    if (kobj != NULL)
    {
        if (kobj->ownerPtr == taskHandle)
        {
            kobj->ownerPtr = NULL;
        }
        taskHandle->signalQueuePtr = NULL;
        RK_DMB
    }

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalQueueSend(RK_TASK_HANDLE const taskHandle,
                   ULONG const eventID,
                   RK_TASK_HANDLE const senderHandle,
                   VOID *const argsPtr,
                   RK_TASK_SIGNAL_HANDLER const handler)
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
    if (eventID == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_TASK_SIGNAL_QUEUE *kobj = taskHandle->signalQueuePtr;

    if (kobj == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj->objID != RK_SIGNALQ_KOBJ_ID)
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

    if (kobj->count >= kobj->depth)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_FULL);
    }

    RK_TASK_SIGNAL *slotPtr = &kobj->bufPtr[kobj->tail];
    slotPtr->eventID = eventID;
    slotPtr->senderPtr = senderHandle;
    slotPtr->argsPtr = argsPtr;
    slotPtr->handler = handler;

    kobj->tail = kTaskSignalQNext_(kobj->tail, kobj->depth);
    kobj->count += 1UL;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kSignalQueueRecv(RK_TASK_HANDLE const taskHandle,
                   RK_TASK_SIGNAL *const signalPtr)
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
    RK_TASK_SIGNAL_QUEUE *kobj = targetTask->signalQueuePtr;

    if (kobj == NULL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_NOT_ATTACHED);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kobj->objID != RK_SIGNALQ_KOBJ_ID)
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

    if (kobj->count == 0UL)
    {
        RK_CR_EXIT
        return (RK_ERR_SIGNALQ_EMPTY);
    }

    RK_TASK_SIGNAL *slotPtr = &kobj->bufPtr[kobj->head];
    *signalPtr = *slotPtr;
    kobj->head = kTaskSignalQNext_(kobj->head, kobj->depth);
    kobj->count -= 1UL;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

VOID  kSignalQueueDsptchResume(RK_TASK_HANDLE const taskHandle)
{
    if (taskHandle == NULL)
    {
        return;
    }

    RK_TASK_SIGNAL_QUEUE *kobj = taskHandle->signalQueuePtr;
    if ((kobj == NULL) || (kobj->ownerPtr != taskHandle) || (kobj->count == 0UL))
    {
        return;
    }

    ULONG budget = kobj->depth;
    while (budget > 0UL)
    {
        RK_TASK_SIGNAL signal;
        RK_ERR err = kSignalQueueRecv(taskHandle, &signal);
        if (err == RK_ERR_SIGNALQ_EMPTY)
        {
            break;
        }
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }
        if (signal.handler != NULL)
        {
            signal.handler(&signal);
        }
        budget -= 1UL;
    }
}
