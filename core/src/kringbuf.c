/* SPDX-License-Identifier: Apache-2.0 */
/*******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.18.0 */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: GENERIC RING BUFFER                                             */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kringbuf.h>

#ifndef K_RINGBUF_CPY
#define K_RINGBUF_CPY(d, s, z)                                                 \
    do                                                                         \
    {                                                                          \
        ULONG words_ = (z);                                                    \
        while (--words_)                                                       \
        {                                                                      \
            *(d)++ = *(s)++;                                                   \
        }                                                                      \
        *(d)++ = *(s)++;                                                       \
    } while (0)
#endif

static ULONG *kRingBufAdvance_(struct RK_STRUCT_RING_BUFFER const *const kobj,
                               ULONG *ptr)
{
    ptr += kobj->dataSize;
    if (ptr == kobj->bufEndPtr)
    {
        ptr = kobj->bufPtr;
    }
    return (ptr);
}

static ULONG *kRingBufRetreat_(struct RK_STRUCT_RING_BUFFER const *const kobj,
                               ULONG *ptr)
{
    if (ptr == kobj->bufPtr)
    {
        ptr = kobj->bufEndPtr;
    }
    ptr -= kobj->dataSize;
    return (ptr);
}

RK_ERR kRingBufInit(struct RK_STRUCT_RING_BUFFER *const kobj,
                    VOID *const bufPtr, ULONG const dataSize,
                    ULONG const maxBuf)
{
    if ((kobj == NULL) || (bufPtr == NULL) || (dataSize == 0UL) ||
        (maxBuf == 0UL))
    {
        return (RK_ERR_INVALID_PARAM);
    }

    kobj->dataSize = dataSize;
    kobj->maxBuf = maxBuf;
    kobj->bufPtr = (ULONG *)bufPtr;
    kobj->bufEndPtr = kobj->bufPtr + (dataSize * maxBuf);
    kRingBufReset(kobj);

    return (RK_ERR_SUCCESS);
}

VOID kRingBufReset(struct RK_STRUCT_RING_BUFFER *const kobj)
{
    kobj->nFull = 0UL;
    kobj->writePtr = kobj->bufPtr;
    kobj->readPtr = kobj->bufPtr;
}

RK_BOOL kRingBufIsEmpty(struct RK_STRUCT_RING_BUFFER const *const kobj)
{
    return ((kobj->nFull == 0UL) ? RK_TRUE : RK_FALSE);
}

RK_BOOL kRingBufIsFull(struct RK_STRUCT_RING_BUFFER const *const kobj)
{
    return ((kobj->nFull >= kobj->maxBuf) ? RK_TRUE : RK_FALSE);
}

VOID kRingBufWrite(struct RK_STRUCT_RING_BUFFER *const kobj,
                   ULONG const *srcPtr)
{
    ULONG *dstPtr = kobj->writePtr;

    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
    kobj->writePtr = kRingBufAdvance_(kobj, kobj->writePtr);
    kobj->nFull++;
}

VOID kRingBufRead(struct RK_STRUCT_RING_BUFFER *const kobj, ULONG *dstPtr)
{
    ULONG *srcPtr = kobj->readPtr;

    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
    kobj->readPtr = kRingBufAdvance_(kobj, kobj->readPtr);
    kobj->nFull--;
}

VOID kRingBufPeek(struct RK_STRUCT_RING_BUFFER const *const kobj, ULONG *dstPtr)
{
    ULONG *srcPtr = kobj->readPtr;

    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
}

VOID kRingBufJam(struct RK_STRUCT_RING_BUFFER *const kobj, ULONG const *srcPtr)
{
    kobj->readPtr = kRingBufRetreat_(kobj, kobj->readPtr);
    {
        ULONG *dstPtr = kobj->readPtr;
        K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
    }
    kobj->nFull++;
}

VOID kRingBufOverwrite(struct RK_STRUCT_RING_BUFFER *const kobj,
                       ULONG const *srcPtr)
{
    ULONG *dstPtr = kobj->writePtr;

    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
    kobj->writePtr = kobj->bufPtr;
    kobj->readPtr = kobj->bufPtr;
    kobj->nFull = 1UL;
}
