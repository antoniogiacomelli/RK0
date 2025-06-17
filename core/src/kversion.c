/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.2
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
 * 
 *  Module            : VERSION
 *  Provides to       : ALL
 *  Public API        : YES
 * 
 *****************************************************************************/

#include "kversion.h"

/* no file system, no NVM map, this is the best we can do */
struct kversion const KVERSION =
    {0, 6, 2};

unsigned int kGetVersion(void)
{
    unsigned int version = (KVERSION.major << 16 | KVERSION.minor << 8 | KVERSION.patch << 0);
    return (version);
}

unsigned int kIsValidVersion(void)
{
    return (kGetVersion() == RK_VALID_VERSION);
}
