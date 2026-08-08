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
/* COMPONENT: TASK EVENT REGISTER                                             */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ktaskevents.h>

RK_ERR kEventClearCore(RK_TASK_HANDLE taskHandle, ULONG const flagsToClear)
{
    /* a clear cannot be interrupted */
    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    /* an ISR has no TCB */
    if (kIsISR() && (taskHandle == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
    if (flagsToClear == 0UL)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        RK_CR_EXIT
        return (RK_FAULT_INVALID_PARAM);
    }
#endif

    RK_TCB *taskPtr = (taskHandle) ? taskHandle : RK_gRunPtr;

    taskPtr->flagsCurr &= ~flagsToClear;
    RK_DMB
    RK_CR_EXIT

    return (RK_ERR_SUCCESS);
}
