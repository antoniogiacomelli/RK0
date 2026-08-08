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

RK_ERR kSleepQueueBlockReadyTaskCore(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE handle)
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

    if (handle == NULL || handle == RK_gRunPtr || handle->status != RK_READY)
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    RK_TCB **const taskPPtr = (RK_TCB * *const)&handle;
    kTCBQRem(&RK_gReadyQueue[handle->priority], taskPPtr);
    RK_TCB *taskPtr = *taskPPtr;
    RK_ERR err = kTCBQEnqByPrio(&kobj->waitingQueue, taskPtr);
    if (!err)
    {
        taskPtr->status = RK_SLEEPQ_BLOCKED;
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_BLOCK, err, kobj->waitingQueue.size);
    RK_CR_EXIT
    return (err);
}

#endif
