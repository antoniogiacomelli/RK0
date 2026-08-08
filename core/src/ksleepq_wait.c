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

RK_ERR kSleepQueueWaitCore(RK_SLEEP_QUEUE *const kobj, RK_TICK const timeout)
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

    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

#endif

    if (timeout == RK_NO_WAIT)
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_WAIT, RK_ERR_NOWAIT,
                           kobj->waitingQueue.size);
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0))
    {
        RK_TASK_TIMEOUT_WAITINGQUEUE_SETUP

        RK_ERR err = kTimeoutNodeAdd(&RK_gRunPtr->timeoutNode, timeout);
        if (err != RK_ERR_SUCCESS)
        {
            RK_gRunPtr->timeoutNode.timeoutType = 0;
            RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
            RK_CR_EXIT
            return (err);
        }
    }
    RK_gRunPtr->status = RK_SLEEPING;
    kTraceRecordObject(kobj, RK_TRACE_OP_WAIT_BLOCK, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size + 1UL);
    kTCBQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);

    kPendCtxSwtch();
    RK_CR_EXIT
    /* resuming here, if time is out, return error */
    RK_CR_ENTER
    if (RK_gRunPtr->timeOut)
    {
        RK_gRunPtr->timeOut = RK_FALSE;
        kTraceRecordObject(kobj, RK_TRACE_OP_TIMEOUT, RK_ERR_TIMEOUT,
                           kobj->waitingQueue.size);
        RK_CR_EXIT
        return (RK_ERR_TIMEOUT);
    }

    if ((timeout != RK_WAIT_FOREVER) && (timeout > 0) &&
        (RK_gRunPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING))
    {
        kRemoveTimeoutNode(&RK_gRunPtr->timeoutNode);
        RK_gRunPtr->timeoutNode.timeoutType = 0;
        RK_gRunPtr->timeoutNode.waitingQueuePtr = NULL;
    }

    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                       kobj->waitingQueue.size);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif
