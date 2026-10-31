/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.90.3                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: GENERIC RING BUFFER                                             */
/******************************************************************************/

/**
 * @brief
 * This file implements a generic ring buffer data structure used 
 * by the Message Queue.
 */

#define RK_SOURCE_CODE

#include <kringbuf.h>

/** 
 This macro performs a copy from a source address to a destination address,
 of z WORDS. D and Z must be pointers to ULONG, and Z must be greater than 0. 
 Note that if z is 1, while(){} is skipped.
*/
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

/* Advance the buffer pointer. */
static inline ULONG *kRingBufAdvance_(struct RK_STRUCT_RING_BUFFER const 
    *const kobj,  ULONG *ptr)
{
    /* walk forward */
    ptr += kobj->dataSize;
    /* then wrap around if reaching end of buffer */
    if (ptr == kobj->bufEndPtr)
    {
        ptr = kobj->bufPtr;
    }
    /* return the position address */
    return (ptr);
}

/* Walk back the buffer pointer. */
static inline ULONG *kRingBufRetreat_(struct RK_STRUCT_RING_BUFFER const *const kobj,
                               ULONG *ptr)
{
    /* Wrap around first, if beginning of buffer */
    if (ptr == kobj->bufPtr)
    {
        ptr = kobj->bufEndPtr;
    }
    /* Then walk back */
    ptr -= kobj->dataSize;
    /* Return the position address */
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

    /* Assign numerical values*/
    kobj->dataSize = dataSize;
    kobj->maxBuf = maxBuf;

    /* Assign buffer addr */
    kobj->bufPtr = (ULONG *)bufPtr;

    /* Address of the end of the buffer */
    kobj->bufEndPtr = kobj->bufPtr + (dataSize * maxBuf);

    /* Reset, so read and write positions are the same, what means
    * the buffer is empty */
    kRingBufReset(kobj);

    return (RK_ERR_SUCCESS);
}

VOID kRingBufReset(struct RK_STRUCT_RING_BUFFER *const kobj)
{
    
    kobj->nFull = 0UL; /* number of full slots */

    /* On reset read and write positions are the same */
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

    K_RINGBUF_CPY(kobj->writePtr, srcPtr, kobj->dataSize);
    if (kobj->writePtr == kobj->bufEndPtr)
    {
        kobj->writePtr = kobj->bufPtr;
    }
        /* Increase the number of full slots */
    kobj->nFull++;
}

VOID kRingBufRead(struct RK_STRUCT_RING_BUFFER *const kobj, ULONG *dstPtr)
{
    /* Read */
    K_RINGBUF_CPY(dstPtr, kobj->readPtr, kobj->dataSize);
    if (kobj->readPtr == kobj->bufEndPtr)
    {
        kobj->readPtr = kobj->bufPtr;
    }
    /* Reduce the number of full slots */
    kobj->nFull--;
}

VOID kRingBufPeek(struct RK_STRUCT_RING_BUFFER const *const kobj, ULONG *dstPtr)
{
    /* Current read position */
    ULONG *srcPtr = kobj->readPtr;

    /* Copy data word by word */
    /* !Do not advance the read pointer, because it is a peek */

    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);

}


VOID kRingBufJam(struct RK_STRUCT_RING_BUFFER *const kobj, ULONG const *srcPtr)
{

    /* Moves readPtr backward one slot */
    kobj->readPtr = kRingBufRetreat_(kobj, kobj->readPtr);
    
    /* Write new value at the location pointed to by readPtr */
    /* do not advance readPtr */
    ULONG *dstPtr = kobj->readPtr;
    
    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
    
    /* Increments the number of full slots */
    kobj->nFull++;
}

/* Copies a single record to the current write pointer, overwriting it */
VOID kRingBufOverwrite(struct RK_STRUCT_RING_BUFFER *const kobj,
                       ULONG const *srcPtr)
{
    ULONG *dstPtr = kobj->writePtr;

    K_RINGBUF_CPY(dstPtr, srcPtr, kobj->dataSize);
    kobj->writePtr = kobj->bufPtr;
    kobj->readPtr = kobj->bufPtr;
    kobj->nFull = 1UL;
}
