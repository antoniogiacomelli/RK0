/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.9                                               */
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

#define RK_VALID_VERSION  (unsigned)(RK_gkVersion.major << 16 | RK_gKversion.minor << 8 | RK_gKversion.patch << 0)
#define RK_VERSION 0x000909

unsigned kIsValidVersion(void);

#endif /* KVERSION_H */
