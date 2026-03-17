/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.14.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_TASK_MAIL_H
#define RK_TASK_MAIL_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif

RK_ERR kMailSend(RK_TASK_HANDLE, VOID *const);

RK_ERR kMailRecv(VOID **const, RK_TICK);

RK_ERR kMailQuery(RK_TASK_HANDLE);


#ifdef __cplusplus
}
#endif

#endif /* RK_TASK_MAIL_H */
