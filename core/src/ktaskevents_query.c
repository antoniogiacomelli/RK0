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

RK_ERR kEventQueryCore(RK_TASK_HANDLE const taskHandle, ULONG *const queryFlagsPtr)
{

    RK_CR_AREA
    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
    if (queryFlagsPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (kIsISR() && (taskHandle == NULL))
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        RK_CR_EXIT
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif
    RK_TASK_HANDLE handle = (taskHandle) ? (taskHandle) : (RK_gRunPtr);
    (*queryFlagsPtr = handle->flagsCurr);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
