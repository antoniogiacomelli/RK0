/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.2                                               */
/** ARCHITECTURE     :   ARMv6/7M                                             */
/**                                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
#ifndef RK_EXECUTIVE_H
#define RK_EXECUTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kcommondefs.h>
#include <kdefs.h>
#include <kconfig.h>
#include <kobjs.h>
#include <kerr.h>
#include <kversion.h>
#include <ksch.h>
#include <klist.h>
#include <kmem.h>
#include <ktaskflags.h>
#include <ksleepq.h>
#include <ksema.h>
#include <kmutex.h>
#include <kmesgq.h>
#include <kmrm.h>
#include <ktimer.h>
#include <ksystasks.h>

#ifdef __cplusplus
    }
#endif

#endif
