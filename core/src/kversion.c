/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.8                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
 
#define RK_SOURCE_CODE

#include <kversion.h>

/* no file system, no NVM map, this is the best we can do */
struct RK_gKversion const RK_gKversion =
    {0, 9, 8};


unsigned kIsValidVersion(void)
{
    return ((RK_gKversion.major == RK_gKversion.major) 
            &&
            (RK_gKversion.minor == RK_gKversion.minor)
            &&
            (RK_gKversion.patch == RK_gKversion.patch)
            );
}

