/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.60.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_MRM_H
#define RK_MRM_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
#include <kstring.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_MRM == ON)
RK_ERR kMRMInit(RK_MRM *const, RK_MRM_BUF *const, VOID *, ULONG const,
                ULONG const);
#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kMRMCreate(RK_MRM_HANDLE *const, RK_MRM_BUF *const, VOID *, ULONG const,
                  ULONG const);
RK_ERR kMRMDestroy(RK_MRM_HANDLE *const);
#endif
RK_MRM_BUF *kMRMReserve(RK_MRM *const);
RK_ERR kMRMPublish(RK_MRM *const, RK_MRM_BUF *const, VOID const *);
RK_MRM_BUF *kMRMGet(RK_MRM *const, VOID *const);
RK_ERR kMRMUnget(RK_MRM *const, RK_MRM_BUF *const);
#endif

#ifdef __cplusplus
}
#endif

#endif
