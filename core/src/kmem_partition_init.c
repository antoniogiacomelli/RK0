/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: PARTITION MEMORY ALLOCATOR                                      */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmem.h>
#include <ktrace.h>

RK_ERR kMemPartitionInitCore(RK_MEM_PARTITION *const kobj, VOID *memPoolPtr,
                         ULONG blkSize, ULONG const numBlocks)
{
    RK_CR_AREA

    RK_CR_ENTER

    if ((kobj == NULL) || (memPoolPtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }

#endif

    if ((blkSize == 0UL) || (numBlocks == 0UL) ||
        (blkSize > (RK_ULONG_MAX - (RK_WORD_SIZE - 1UL))) ||
        (((ULONG)memPoolPtr & (RK_WORD_SIZE - 1UL)) != 0UL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    /* rounds up to next multiple of 4*/
    blkSize = ((blkSize + RK_WORD_SIZE - 1) & ~(RK_WORD_SIZE - 1));

    /* initialise freelist of blocks */

    ULONG *blockPtr = (ULONG *)memPoolPtr;
    VOID **nextAddrPtr = (VOID **)memPoolPtr; /* next block address */

    for (ULONG i = 0; i < numBlocks - 1; i++)
    {
        ULONG incSizeWord = blkSize / RK_WORD_SIZE;
        blockPtr += incSizeWord;
        /* save blockPtr addr as the next */
        *nextAddrPtr = (VOID *)blockPtr;
        /* update  */
        nextAddrPtr = (VOID **)(blockPtr);
    }
    *nextAddrPtr = NULL;

    /* init the control block */
    kobj->blkSize = blkSize;
    kobj->nMaxBlocks = numBlocks;
    kobj->nFreeBlocks = numBlocks;
    kobj->freeListPtr = memPoolPtr;
    kobj->poolPtr = memPoolPtr;
    kobj->init = RK_TRUE;
    kobj->objID = RK_MEMALLOC_KOBJ_ID;
    kobj->objName[0] = '\0';
    kTraceRegisterObject(kobj, RK_MEMALLOC_KOBJ_ID);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
