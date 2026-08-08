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

RK_ERR kMemPartitionFreeCore(RK_MEM_PARTITION *const kobj, VOID *blockPtr)
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
    /* check memory address is in range and multiple of size */
    BYTE *poolStartPtr = kobj->poolPtr;
    BYTE *const poolEndPtr = poolStartPtr + (kobj->blkSize * kobj->nMaxBlocks);
    BYTE *freeBytePtr = (BYTE *)blockPtr;
    ULONG diff = (ULONG)(freeBytePtr - poolStartPtr);
    ULONG q = diff / kobj->blkSize;
    ULONG rem = diff - (q * kobj->blkSize);
    RK_BOOL outBound =
        ((freeBytePtr < poolStartPtr) || (freeBytePtr >= poolEndPtr));
    /* all blocks belonging to this pool are free */
    RK_BOOL allFree = (kobj->nFreeBlocks == kobj->nMaxBlocks);
    if (rem != 0UL || outBound || allFree)
    {
        K_ERR_HANDLER(RK_ERR_MEM_FREE);
        RK_CR_EXIT
        return (RK_ERR_MEM_FREE);
    }

#endif

    *(VOID **)blockPtr = kobj->freeListPtr;
    kobj->freeListPtr = blockPtr;
    kobj->nFreeBlocks += 1;
    kTraceRecordObject(kobj, RK_TRACE_OP_FREE, RK_ERR_SUCCESS,
                       kobj->nFreeBlocks);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}
