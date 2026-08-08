/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.51.0                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: MRM BUFFERS                                                     */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmrm.h>
#include <ktrace.h>

#if (RK_CONF_MRM == ON)

RK_MRM_BUF*kMRMReserveCore(RK_MRM *const kobj)
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
    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (NULL);
    }

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (NULL);
    }

#endif

    RK_MRM_BUF *allocPtr = NULL;
    if ((kobj->currBufPtr != NULL))
    {
        if ((kobj->currBufPtr->nUsers == 0))
        {
            allocPtr = kobj->currBufPtr;
            allocPtr->nUsers = 0UL;
            RK_MEMSET(kobj->currBufPtr->mrmData, 0, (kobj->size)*4);
        }
        else
        {
            allocPtr = kMemPartitionAlloc(&kobj->mrmMem);
            if (allocPtr != NULL)
            {
                allocPtr->nUsers = 0UL;
                allocPtr->mrmData =
                    (ULONG *)kMemPartitionAlloc(&kobj->mrmDataMem);
                if (allocPtr->mrmData == NULL)
                {
                    kMemPartitionFree(&kobj->mrmMem, allocPtr);
                    allocPtr = NULL;
                }
            }
        }
    }
    else
    {
        allocPtr = kMemPartitionAlloc(&kobj->mrmMem);
        if (allocPtr != NULL)
        {
            allocPtr->nUsers = 0UL;
            allocPtr->mrmData = (ULONG *)kMemPartitionAlloc(&kobj->mrmDataMem);
            if (allocPtr->mrmData == NULL)
            {
                kMemPartitionFree(&kobj->mrmMem, allocPtr);
                allocPtr = NULL;
            }
        }
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_RESERVE,
                       (allocPtr != NULL) ? RK_ERR_SUCCESS : RK_ERR_BUFFER_EMPTY,
                       kobj->mrmMem.nFreeBlocks);
    RK_CR_EXIT
    return (allocPtr);
}

#endif
