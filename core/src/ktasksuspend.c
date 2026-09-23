/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.82.2                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: TASK SUSPEND/RESUME                                             */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktasksuspend.h>
#include <ksch.h>

#if (RK_CONF_ERR_CHECK == ON)
static inline RK_BOOL kTaskIrqsDisabled_(VOID)
{
    unsigned state;

    RK_ASM volatile("MRS %0, PRIMASK" : "=r"(state) : : "memory");

    return ((state != 0U) ? RK_TRUE : RK_FALSE);
}
#endif

RK_ERR kTaskSelfSuspend(VOID)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR() != 0U)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    if ((RK_gRunPtr == NULL) || (RK_gRunPtr->status != RK_RUNNING))
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        return (RK_ERR_TASK_INVALID_ST);
    }

    if ((RK_gSchLock != 0U) || (kTaskIrqsDisabled_() == RK_TRUE))
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        return (RK_ERR_TASK_INVALID_ST);
    }
#endif

    RK_CR_AREA
    RK_CR_ENTER

    kPendCtxSwtch();
    RK_gRunPtr->status = RK_SELF_SUSPENDED;

    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}

RK_ERR kTaskResume(RK_TASK_HANDLE const taskHandle)
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
    if ((taskHandle == RK_gRunPtr) && (kIsISR() == 0U))
    {                                  //corner case
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    if (taskHandle->init != RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (taskHandle->status != RK_SELF_SUSPENDED)
    {
        K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
        RK_CR_EXIT
        return (RK_ERR_TASK_INVALID_ST);
    }
#endif

    RK_ERR const err = kReadySwtch(taskHandle);
    RK_CR_EXIT

    if ((err == RK_ERR_RESCHED_NOT_NEEDED) ||
        (err == RK_ERR_RESCHED_PENDING))
    {
        return (RK_ERR_SUCCESS);
    }

    return (err);
}
