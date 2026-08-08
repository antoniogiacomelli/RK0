/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: COUNTING SEMAPHORE                                              */
/******************************************************************************/

#define RK_SOURCE_CODE

#include "ksema_private.h"

#if (RK_CONF_SEMAPHORE == ON)

RK_ERR kSemaphorePostCore(RK_SEMAPHORE *const kobj)
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

    if (kobj->objID != RK_SEMAPHORE_KOBJ_ID)
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

    RK_TCB *nextTCBPtr = NULL;
    RK_ERR ret = -1;
    if (kobj->waitingQueue.size > 0)
    {
        kTCBQDeq(&(kobj->waitingQueue), &nextTCBPtr);
        if (nextTCBPtr->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
        {
            kRemoveTimeoutNode(&nextTCBPtr->timeoutNode);
            nextTCBPtr->timeoutNode.timeoutType = 0;
            nextTCBPtr->timeoutNode.waitingQueuePtr = NULL;
        }
        ret = kReadySwtch(nextTCBPtr);
        kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, ret,
                           kobj->waitingQueue.size);
    }
    else
    {
        /* there are no waiting tasks */
        if (K_SEMA_IS_BINARY(kobj))
        {
            if (kobj->value != 0U)
            {
                ret = RK_ERR_SEMA_FULL;
            }
            else
            {
                kobj->value = 1U;
                ret = RK_ERR_SUCCESS;
            }
        }
        else
        {
            if (kobj->value == kobj->maxValue)
            {
                ret = RK_ERR_SEMA_FULL;
            }
            else
            {
                kobj->value += 1U;
                ret = RK_ERR_SUCCESS;
            }
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_POST, ret, kobj->value);
    }
    RK_CR_EXIT
    return (ret);
}

#endif
