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

/******************************************************************************/
#ifndef RK_MEM_H
#define RK_MEM_H
#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>
#ifdef __cplusplus
extern "C" {
#endif

RK_ERR kMemPartitionInit(RK_MEM_PARTITION* const, VOID *, ULONG const, ULONG);
VOID *kMemPartitionAlloc(RK_MEM_PARTITION* const);
RK_ERR kMemPartitionFree(RK_MEM_PARTITION* const, VOID *);

#ifdef __cplusplus
}
#endif

#endif
