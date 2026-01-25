/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.8                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/******************************************************************************
 *           o Kernel Version record definition
 *                  xx.xx.xx
 *                  major minor patch
 *
 *****************************************************************************/
#ifndef RK_VERSION_H
#define RK_VERSION_H


struct RK_gKversion
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch;
};

extern struct RK_gKversion const RK_gKversion;

#define RK_VERSION_MAJOR (unsigned)(RK_gKversion.major << 16 )
#define RK_VERSION_MINOR (unsigned)(RK_gKversion.major << 8 )
#define RK_VERSION_PATH (unsigned)(RK_gKversion.major << 0)

#define RK_VALID_VERSION  RK_VERSION_MAJOR | RK_VERSION_MINOR | RK_VERSION_PATH


unsigned kIsValidVersion(void);

#endif /* KVERSION_H */
