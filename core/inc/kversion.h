/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.3                                               */
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

/*** Minimal valid version **/
/** This is to manage API retrocompatibilities */
#define RK_CONF_MINIMAL_VER 0U

extern struct RK_gKversion const RK_gKversion;

#if (RK_CONF_MINIMAL_VER == 0U) /* there is no retrocompatible version */
                                /* the valid is the current            */
#define RK_VALID_VERSION (unsigned)((RK_gKversion.major << 16 | RK_gKversion.minor << 8 | RK_gKversion.patch << 0))

#else

#define RK_VALID_VERSION RK_CONF_MINIMAL_VER

#endif

struct RK_gKversion
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch;
};

unsigned int kGetVersion(void);
unsigned int kIsValidVersion(void);
#endif /* KVERSION_H */
