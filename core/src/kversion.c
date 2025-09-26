/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
 
#include <kversion.h>

/* no file system, no NVM map, this is the best we can do */
struct kversion const KVERSION =
    {0, 8, 0};

unsigned int kGetVersion(void)
{
    unsigned int version = (KVERSION.major << 16 | KVERSION.minor << 8 | KVERSION.patch << 0);
    return (version);
}

unsigned int kIsValidVersion(void)
{
    return (kGetVersion() == RK_VALID_VERSION);
}
