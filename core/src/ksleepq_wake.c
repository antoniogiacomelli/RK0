/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SLEEP QUEUE                                                     */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ksleepq.h>
#include <ksystasks.h>
#include <ktimer.h>
#include <ktrace.h>
#include <kmutex.h>

#if (RK_CONF_SLEEP_QUEUE == ON)

RK_ERR kSleepQueueWakeCore(RK_SLEEP_QUEUE *const kobj, UINT nTasks, UINT *uTasksPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_SLEEPQ_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

#endif

    UINT nWaiting = kobj->waitingQueue.size;

    if (nWaiting == 0)
    {
        if (uTasksPtr)
            *uTasksPtr = 0;
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE,
                           RK_ERR_EMPTY_WAITING_QUEUE, 0UL);
        RK_CR_EXIT
        return (RK_ERR_EMPTY_WAITING_QUEUE);
    }

    /* Wake up to nTasks, but no more than nWaiting */
    UINT toWake = 0;
    if (nTasks == 0)
    {
        /* if 0, wake'em all */
        toWake = nWaiting;
    }
    else
    {
        toWake = (nTasks < nWaiting) ? (nTasks) : (nWaiting);
    }

    if (kIsISR())
    {
        if (uTasksPtr != NULL)
        {
#if (RK_CONF_ERR_CHECK == ON)
            K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
            RK_CR_EXIT
            return (RK_ERR_INVALID_PARAM);
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                           (ULONG)toWake);
        RK_CR_EXIT
        return (
            kPostProcJobEnq(RK_POSTPROC_JOB_SLEEPQ_WAKE, (VOID *)kobj, toWake));
    }

    RK_CR_EXIT

    kSchLock();

    RK_TCB *chosenTCBPtr = NULL;
    RK_ERR ret = RK_ERR_SUCCESS;

    for (UINT i = 0U; i < toWake; i++)
    {
        RK_CR_ENTER
        if (kobj->waitingQueue.size == 0U)
        {
            RK_CR_EXIT
            break;
        }

        RK_TCB *nextTCBPtr = NULL;
        ret = kTCBQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (ret != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            break;
        }
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        ret = kReadyNoSwtch(nextTCBPtr);
        if (ret != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            break;
        }
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }

        RK_CR_EXIT
    }

    RK_CR_ENTER
    if (uTasksPtr)
    {
        *uTasksPtr = (UINT)kobj->waitingQueue.size;
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, ret,
                       (ULONG)(nWaiting - kobj->waitingQueue.size));
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }
    RK_CR_EXIT

    kSchUnlock();
    return (ret);
}

#endif
