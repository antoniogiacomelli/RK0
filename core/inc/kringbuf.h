/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.16.1                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

#ifndef RK_RINGBUF_H
#define RK_RINGBUF_H

#include <kenv.h>
#include <kcoredefs.h>
#include <kcommondefs.h>
#include <kobjs.h>

#ifdef __cplusplus
extern "C" {
#endif


RK_ERR kRingBufInit(struct RK_STRUCT_RING_BUFFER *const kobj,
                    VOID *const bufPtr,
                    ULONG const dataSize, ULONG const maxBuf);
VOID kRingBufReset(struct RK_STRUCT_RING_BUFFER *const kobj);
RK_BOOL kRingBufIsEmpty(struct RK_STRUCT_RING_BUFFER const *const kobj);
RK_BOOL kRingBufIsFull(struct RK_STRUCT_RING_BUFFER const *const kobj);
VOID kRingBufWrite(struct RK_STRUCT_RING_BUFFER *const kobj,
                   ULONG const *srcPtr);
VOID kRingBufRead(struct RK_STRUCT_RING_BUFFER *const kobj, ULONG *dstPtr);
VOID kRingBufPeek(struct RK_STRUCT_RING_BUFFER const *const kobj,
                  ULONG *dstPtr);
VOID kRingBufJam(struct RK_STRUCT_RING_BUFFER *const kobj, ULONG const *srcPtr);
VOID kRingBufOverwrite(struct RK_STRUCT_RING_BUFFER *const kobj,
                       ULONG const *srcPtr);

#ifdef __cplusplus
}
#endif

#endif
