/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.10                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/
/**                                                                           */
/** COMPONENT       : PARTITION MEMORY ALLOCATOR                              */
/** DEPENDS ON      : LOW-LEVEL SCHEDULER                                     */
/** PROVIDES TO     : EXECUTIVE                                               */
/** PUBLIC API      : YES                                                     */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#define RK_SOURCE_CODE 

#include <kmem.h>

RK_ERR kMemPartitionInit(RK_MEM_PARTITION *const kobj, VOID *memPoolPtr, ULONG blkSize, ULONG const numBlocks)
{
    RK_CR_AREA

    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->init == RK_TRUE)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_DOUBLE_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_DOUBLE_INIT);
    }
    
#endif

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
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

VOID *kMemPartitionAlloc(RK_MEM_PARTITION *const kobj)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (NULL); 
    }

    if (kobj->objID != RK_MEMALLOC_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (NULL);
    }

    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (NULL);
    }

#endif
    
     VOID *allocPtr = NULL;
        

    if (kobj->nFreeBlocks > 0)
    {    
        allocPtr = kobj->freeListPtr;
        RK_BARRIER
        kobj->nFreeBlocks -= 1;
        kobj->freeListPtr = *(VOID **)allocPtr;
    }
    RK_CR_EXIT
    return (allocPtr);
}

RK_ERR kMemPartitionFree(RK_MEM_PARTITION *const kobj, VOID *blockPtr)
{

    RK_CR_AREA
    RK_CR_ENTER

#if (RK_CONF_ERR_CHECK == ON)

    if (kobj == NULL || blockPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MEMALLOC_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

#endif

    if (kobj->nFreeBlocks == kobj->nMaxBlocks)
    {
        RK_CR_EXIT
        return (RK_ERR_MEM_FREE);
    }

    *(VOID **)blockPtr = kobj->freeListPtr;
    kobj->freeListPtr = blockPtr;
    kobj->nFreeBlocks += 1;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
