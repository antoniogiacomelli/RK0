/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.83.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: CYCLIC BARRIER                                                  */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kbarrier.h>
#include <ksch.h>
#include <ktrace.h>

#if (RK_CONF_BARRIER == ON)
RK_ERR kBarrierInit(RK_BARRIER *const kobj, UINT const parties)
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

    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

    if (parties == 0U)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = RK_TRUE;
    kobj->objID = RK_BARRIER_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->parties = parties;
    kobj->arrived = 0U;
    kobj->generation = 0U;
    kTraceRegisterObject(kobj, RK_BARRIER_KOBJ_ID);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kBarrierWait(RK_BARRIER *const kobj)
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

    if (kobj->objID != RK_BARRIER_KOBJ_ID)
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

    kobj->arrived++;

    if (kobj->arrived < kobj->parties)
    {
        RK_gRunPtr->status = RK_BLOCKED;
        kTraceRecordObject(kobj, RK_TRACE_OP_WAIT_BLOCK, RK_ERR_SUCCESS,
                           (ULONG)kobj->arrived);
        kWaitQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);
        kPendCtxSwtch();
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    kobj->arrived = 0U;
    kobj->generation++;

    RK_TCB *chosenTCBPtr = NULL;
    RK_ERR ret = RK_ERR_SUCCESS;
    UINT released = 0U;

    while (kobj->waitingQueue.size > 0UL)
    {
        RK_TCB *nextTCBPtr = NULL;
        ret = kWaitQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (ret != RK_ERR_SUCCESS)
        {
            break;
        }

        ret = kReadyNoSwtch(nextTCBPtr);
        if (ret != RK_ERR_SUCCESS)
        {
            break;
        }

        released++;
        if ((chosenTCBPtr == NULL) ||
            (nextTCBPtr->priority < chosenTCBPtr->priority))
        {
            chosenTCBPtr = nextTCBPtr;
        }
    }

    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, ret, (ULONG)released);
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }

    RK_CR_EXIT
    return (ret);
}
#endif
