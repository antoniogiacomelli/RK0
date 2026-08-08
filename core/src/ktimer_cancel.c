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
/* COMPONENT: TIMER                                                           */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktimer.h>
#include <ktrace.h>

#if (RK_CONF_CALLOUT_TIMER == ON)

RK_ERR kTimerCancelCore(RK_TIMER *const kobj)
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
    if ((kobj->objID != RK_TIMER_KOBJ_ID) || (kobj->init != RK_TRUE))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }
#endif

    RK_TIMEOUT_NODE *node = (RK_TIMEOUT_NODE *)&kobj->timeoutNode;
    RK_ERR err = RK_ERR_SUCCESS;
    if (kTimeoutNodeIsArmed(node) == RK_TRUE)
    {
        err = kTimeoutNodeDisarm(node);
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_CANCEL, err, 0UL);

    RK_CR_EXIT
    return (err);
}

#endif
