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
static RK_ERR kBarrierCheckObject_(RK_BARRIER *const kobj)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_BARRIER_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        return (RK_ERR_INVALID_OBJ);
    }

    if (kobj->init == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        return (RK_ERR_OBJ_NOT_INIT);
    }
#else
    (void)kobj;
#endif

    return (RK_ERR_SUCCESS);
}

static RK_ERR kBarrierJoinCurrent_(RK_BARRIER *const kobj)
{
    if (kobj->barrierPrioCeilingEnabled != RK_TRUE)
    {
        return (RK_ERR_SUCCESS);
    }

    if (RK_gRunPtr->barrierCeilingPtr == kobj)
    {
        return (RK_ERR_SUCCESS);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (RK_gRunPtr->barrierCeilingPtr != NULL)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        return (RK_ERR_TASK_INVALID_ST);
    }

    if (RK_gRunPtr->priority < kobj->barrierPrioCeiling)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_PRIO);
        return (RK_ERR_INVALID_PRIO);
    }
#endif

    RK_gRunPtr->barrierCeilingPtr = kobj;
    kTaskUpdateEffectivePrioChain(RK_gRunPtr);

    return (RK_ERR_SUCCESS);
}

static RK_ERR kBarrierLeaveCurrent_(RK_BARRIER *const kobj)
{
    if (RK_gRunPtr->barrierCeilingPtr == NULL)
    {
        return (RK_ERR_SUCCESS);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (RK_gRunPtr->barrierCeilingPtr != kobj)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        return (RK_ERR_TASK_INVALID_ST);
    }
#endif

    RK_gRunPtr->barrierCeilingPtr = NULL;
    kTaskUpdateEffectivePrioChain(RK_gRunPtr);

    return (RK_ERR_SUCCESS);
}

static RK_ERR kBarrierInit_(RK_BARRIER *const kobj, UINT const parties,
                            RK_PRIO const ceilingPrio)
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

    if ((ceilingPrio != RK_BARRIER_PRIO_CEILING_NONE) &&
        (ceilingPrio > RK_CONF_MIN_PRIO))
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_PRIO);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PRIO);
    }
#endif

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = RK_TRUE;
    kobj->objID = RK_BARRIER_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->parties = parties;
    kobj->arrived = 0U;
    kobj->generation = 0U;
    if (ceilingPrio == RK_BARRIER_PRIO_CEILING_NONE)
    {
        kobj->barrierPrioCeiling = RK_BARRIER_PRIO_CEILING_NONE;
        kobj->barrierPrioCeilingEnabled = RK_FALSE;
    }
    else
    {
        kobj->barrierPrioCeiling = ceilingPrio;
        kobj->barrierPrioCeilingEnabled = RK_TRUE;
    }
    kTraceRegisterObject(kobj, RK_BARRIER_KOBJ_ID);

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_ERR kBarrierInit(RK_BARRIER *const kobj, UINT const parties)
{
    return (kBarrierInit_(kobj, parties, RK_BARRIER_PRIO_CEILING_NONE));
}

RK_ERR kBarrierInitCeiling(RK_BARRIER *const kobj, UINT const parties,
                           RK_PRIO const ceilingPrio)
{
    return (kBarrierInit_(kobj, parties, ceilingPrio));
}

RK_ERR kBarrierJoin(RK_BARRIER *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR err = kBarrierCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    err = kBarrierJoinCurrent_(kobj);

    RK_CR_EXIT
    return (err);
}

RK_ERR kBarrierLeave(RK_BARRIER *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR err = kBarrierCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    err = kBarrierLeaveCurrent_(kobj);

    RK_CR_EXIT
    return (err);
}

RK_ERR kBarrierWait(RK_BARRIER *const kobj)
{
    RK_CR_AREA
    RK_CR_ENTER

    RK_ERR err = kBarrierCheckObject_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif

    err = kBarrierJoinCurrent_(kobj);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    kobj->arrived++;

    if (kobj->arrived < kobj->parties)
    {
        RK_gRunPtr->status = RK_BLOCKED;
        err = kWaitQEnqByPrio(&kobj->waitingQueue, RK_gRunPtr);
        if (err != RK_ERR_SUCCESS)
        {
            kobj->arrived--;
            RK_gRunPtr->status = RK_RUNNING;
            RK_CR_EXIT
            return (err);
        }
        kTraceRecordObject(kobj, RK_TRACE_OP_WAIT_BLOCK, RK_ERR_SUCCESS,
                           (ULONG)kobj->arrived);
        kPendCtxSwtch();
        RK_CR_EXIT
        return (RK_ERR_SUCCESS);
    }

    kobj->arrived = 0U;
    kobj->generation++;

    RK_TCB *chosenTCBPtr = NULL;
    UINT released = 0U;

    while (kobj->waitingQueue.size > 0UL)
    {
        RK_TCB *nextTCBPtr = NULL;
        err = kWaitQDeq(&kobj->waitingQueue, &nextTCBPtr);
        if (err != RK_ERR_SUCCESS)
        {
            break;
        }

        err = kReadyNoSwtch(nextTCBPtr);
        if (err != RK_ERR_SUCCESS)
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

    kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, err, (ULONG)released);
    if (chosenTCBPtr != NULL)
    {
        kReschedTask(chosenTCBPtr);
    }

    RK_CR_EXIT
    return (err);
}
#endif
