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

RK_TICK kTickGetMsCore(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_TICK ret = 0UL;
    if (RK_gSysTickInterval != 0)
    {
        ret = (RK_gRunTime.globalTick * RK_gSysTickInterval);
    }
    else
    {
        K_PANIC("System tick interval not set");
    }
    RK_CR_EXIT
    return (ret);
}
