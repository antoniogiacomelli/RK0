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

RK_ERR kSemaphoreInitCore(RK_SEMAPHORE *const kobj, const UINT initValue,
                      const UINT maxValue)
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
    if ((maxValue == 0U) || (initValue > maxValue))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }
    if (kTCBQInit(&(kobj->waitingQueue)) != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (RK_ERR_ERROR);
    }
#endif

    kobj->init = RK_TRUE;
    kobj->objID = RK_SEMAPHORE_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->maxValue = maxValue;
    kobj->value = initValue;
    kTraceRegisterObject(kobj, RK_SEMAPHORE_KOBJ_ID);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif
