/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.13                                              */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_VERSION_H
#define RK_VERSION_H


struct RK_gKversion
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch;
};

#define RK_VERSION_MAJOR 0
#define RK_VERSION_MINOR 9
#define RK_VERSION_PATCH 13


extern struct RK_gKversion const RK_gKversion;

#define RK_VALID_VERSION (unsigned)((RK_VERSION_MAJOR << 16) | (RK_VERSION_MINOR << 8) | (RK_VERSION_PATCH << 0))


#define RK_VERSION RK_VALID_VERSION


unsigned kIsValidVersion(void);

#endif /* KVERSION_H */
