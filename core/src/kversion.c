/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.1                                               */
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
    {0, 9, 1};

unsigned int kGetVersion(void)
{
    unsigned int version = (RK_gKversion.major << 16 | RK_gKversion.minor << 8 | RK_gKversion.patch << 0);
    return (version);
}

unsigned int kIsValidVersion(void)
{
    return (kGetVersion() == RK_VALID_VERSION);
}
