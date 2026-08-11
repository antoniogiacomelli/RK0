/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.60.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
#ifndef RK_DYNOBJS_H
#define RK_DYNOBJS_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (RK_CONF_DYNAMIC_OBJECTS == ON)
RK_ERR kObjPartitionsInit(VOID);
#define RK_INIT_OBJ_PARTITIONS do { kObjPartitionsInit(); } while(0);
#else
#define RK_INIT_OBJ_PARTITIONS do { } while(0);
#endif

#ifdef __cplusplus
}
#endif

#endif
