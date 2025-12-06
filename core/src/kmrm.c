/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.2                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
 
/******************************************************************************/
/**                                                                           */
/**  COMPONENT       : MRM BUFFERS                                            */
/**  DEPENDS ON      : MEMORY PARTITION                                       */
/**  PROVIDES TO     : APPLICATION                                            */
/**  PUBLIC API      : YES                                                    */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kmrm.h>

#if (RK_CONF_MRM == ON)
/******************************************************************************/
/* MRM Buffers                                                                */
/******************************************************************************/
RK_ERR kMRMInit(RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
                VOID *mesgPoolPtr, ULONG const nBufs, ULONG const dataSizeWords)
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
    RK_ERR err = RK_ERR_ERROR;

    err = kMemPartitionInit(&kobj->mrmMem, mrmPoolPtr, sizeof(RK_MRM_BUF), nBufs);
    if (!err)
        err = kMemPartitionInit(&kobj->mrmDataMem, mesgPoolPtr, dataSizeWords * 4,
                       nBufs);
    if (!err)
    {
        /* nobody is using anything yet */
        kobj->currBufPtr = NULL;
        kobj->init = RK_TRUE;
        kobj->size = dataSizeWords;
        kobj->objID = RK_MRM_KOBJ_ID;
    }

    RK_CR_EXIT
    return (err);
}

RK_MRM_BUF *kMRMReserve(RK_MRM *const kobj)
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
            RK_MEMSET(kobj->currBufPtr->mrmData, 0, kobj->size);
        }
        else
        {
            allocPtr = kMemPartitionAlloc(&kobj->mrmMem);
            if (allocPtr != NULL)
            {
                allocPtr->mrmData = (ULONG *)kMemPartitionAlloc(&kobj->mrmDataMem);
            }
        }
    }
    else
    {
        allocPtr = kMemPartitionAlloc(&kobj->mrmMem);
        if (allocPtr != NULL)
        {
            allocPtr->mrmData = (ULONG *)kMemPartitionAlloc(&kobj->mrmDataMem);
        }
    }
    RK_CR_EXIT
    return (allocPtr);
}

RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr,
                   VOID const *pubMesgPtr)
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
    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (bufPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
    if (pubMesgPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }
#endif
    ULONG *mrmMesgPtr_ = (ULONG *)bufPtr->mrmData;
    const ULONG *pubMesgPtr_ = (const ULONG *)pubMesgPtr;
    for (UINT i = 0; i < kobj->size; ++i)
    {
        mrmMesgPtr_[i] = pubMesgPtr_[i];
    }
    kobj->currBufPtr = bufPtr;
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr)
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

    if (getMesgPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (NULL);
    }

#endif

    kobj->currBufPtr->nUsers++;
    ULONG *getMesgPtr_ = (ULONG *)getMesgPtr;
    ULONG const *mrmMesgPtr_ = (ULONG const *)kobj->currBufPtr->mrmData;
    for (ULONG i = 0; i < kobj->size; ++i)
    {
        getMesgPtr_[i] = mrmMesgPtr_[i];
    }
    RK_CR_EXIT
    return (kobj->currBufPtr);
}

RK_ERR kMRMUnget(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr)
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

    if (!kobj->init)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (bufPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

    if (kobj->objID != RK_MRM_KOBJ_ID)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

#endif

    RK_ERR err = 0;
    if (bufPtr->nUsers > 0)
        bufPtr->nUsers--;
    /* deallocate if not used and not the most recent buffer */
    if ((bufPtr->nUsers == 0) && (kobj->currBufPtr != bufPtr))
    {
        ULONG *mrmDataPtr = bufPtr->mrmData;
        kMemPartitionFree(&kobj->mrmDataMem, (VOID *)mrmDataPtr);
        kMemPartitionFree(&kobj->mrmMem, (VOID *)bufPtr);
    }

    RK_CR_EXIT
    return (err);
}
#endif
