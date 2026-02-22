/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.10.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/    
#define RK_SOURCE_CODE

#include <kversion.h>

/* no file system, no NVM map, this is the best we can do */
struct RK_gKversion const RK_gKversion =
{
    RK_VERSION_MAJOR,
    RK_VERSION_MINOR,
    RK_VERSION_PATCH
};


unsigned kIsValidVersion(void)
{
    return ((unsigned)((RK_gKversion.major << 16) |
                       (RK_gKversion.minor << 8) |
                       (RK_gKversion.patch << 0)) ==
            RK_VALID_VERSION);
}


unsigned kGetVersion(void)
{
    return (RK_VALID_VERSION);
}

void kGetInfo(const char** infoPPtr)
{
    if (infoPPtr != 0)
    {
        *infoPPtr = RK_VER_INFO;
    }
}
