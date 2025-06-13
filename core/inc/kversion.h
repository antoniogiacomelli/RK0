/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.5.0
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

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

extern struct kversion const KVERSION;

#if (RK_CONF_MINIMAL_VER == 0U) /* there is no retrocompatible version */
                                /* the valid is the current            */
#define RK_VALID_VERSION (unsigned)((KVERSION.major << 16 | KVERSION.minor << 8 | KVERSION.patch << 0))

#else

#define RK_VALID_VERSION RK_CONF_MINIMAL_VER

#endif

struct kversion
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch;
};

unsigned int kGetVersion(void);
unsigned int kIsValidVersion(void);
#endif /* KVERSION_H */
