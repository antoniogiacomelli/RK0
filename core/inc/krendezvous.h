/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.41.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_RENDEZVOUS_H
#define RK_RENDEZVOUS_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

/*
 * Rendezvous is RK0's unbuffered synchronous message-passing primitive.
 * kRendezvousSend() blocks until the receiver takes exactly one non-NULL
 * pointer with kRendezvousRecv(). It has no buffering and no reply operation.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* RK_RENDEZVOUS_H */
