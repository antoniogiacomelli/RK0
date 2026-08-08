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

VOID*kMemPartitionAllocCore(RK_MEM_PARTITION *const kobj)
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
        kTraceRecordObject(kobj, RK_TRACE_OP_ALLOC, RK_ERR_SUCCESS,
                           kobj->nFreeBlocks);
    }
    else
    {
        kTraceRecordObject(kobj, RK_TRACE_OP_ALLOC, RK_ERR_BUFFER_EMPTY,
                           kobj->nFreeBlocks);
    }
    RK_CR_EXIT
    return (allocPtr);
}
