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
/* COMPONENT: MUTEX LOCK                                                      */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ksch.h>
#include <ktrace.h>
#include <kmutex.h>

#if (RK_CONF_MUTEX == ON)

RK_ERR kMutexInitCore(RK_MUTEX *const kobj, UINT const protocol)
{
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_ERROR);
    }

    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
#endif

    if ((protocol != RK_PRIO_NONE) && (protocol != RK_PRIO_INHERITANCE))
    {
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    kTCBQInit(&(kobj->waitingQueue));
    kobj->init = RK_TRUE;
    kobj->protocol = protocol;
    kobj->objID = RK_MUTEX_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->lock = RK_FALSE;
    kobj->ownerPtr = NULL;
    kTraceRegisterObject(kobj, RK_MUTEX_KOBJ_ID);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

#endif
