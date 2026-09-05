/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.73.2                                                          */
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
/******************************************************************************/
/* MRM Buffers                                                                */
/******************************************************************************/
#if (RK_CONF_ERR_CHECK == ON)
static RK_BOOL kMRMPartValid_(RK_MEM_PARTITION const *const partPtr)
{
    return (((partPtr != NULL) &&
             (partPtr->objID == RK_MEMALLOC_KOBJ_ID) &&
             (partPtr->init == RK_TRUE))
                ? RK_TRUE
                : RK_FALSE);
}

static RK_BOOL kMRMPartOwnsBlock_(RK_MEM_PARTITION const *const partPtr,
                                  VOID const *const blockPtr,
                                  ULONG const minSize)
{
    if ((kMRMPartValid_(partPtr) == RK_FALSE) || (blockPtr == NULL) ||
        (partPtr->blkSize < minSize))
    {
        return (RK_FALSE);
    }

    BYTE const *const poolStartPtr = partPtr->poolPtr;
    BYTE const *const poolEndPtr =
        poolStartPtr + (partPtr->blkSize * partPtr->nMaxBlocks);
    BYTE const *const blockBytePtr = (BYTE const *)blockPtr;
    if ((blockBytePtr < poolStartPtr) || (blockBytePtr >= poolEndPtr))
    {
        return (RK_FALSE);
    }

    ULONG const diff = (ULONG)(blockBytePtr - poolStartPtr);
    return (((diff % partPtr->blkSize) == 0UL) ? RK_TRUE : RK_FALSE);
}

static RK_BOOL kMRMPartFreeListContains_(RK_MEM_PARTITION const *const partPtr,
                                         VOID const *const blockPtr)
{
    BYTE *freeBlockPtr = partPtr->freeListPtr;

    for (ULONG i = 0UL; (i < partPtr->nFreeBlocks) && (freeBlockPtr != NULL);
         i++)
    {
        if ((VOID const *)freeBlockPtr == blockPtr)
        {
            return (RK_TRUE);
        }

        freeBlockPtr = *(BYTE **)freeBlockPtr;
    }

    return (RK_FALSE);
}

static RK_BOOL kMRMPartOwnsAllocatedBlock_(RK_MEM_PARTITION const *const partPtr,
                                           VOID const *const blockPtr,
                                           ULONG const minSize)
{
    if (kMRMPartOwnsBlock_(partPtr, blockPtr, minSize) == RK_FALSE)
    {
        return (RK_FALSE);
    }

    return ((kMRMPartFreeListContains_(partPtr, blockPtr) == RK_FALSE)
                ? RK_TRUE
                : RK_FALSE);
}

static RK_BOOL kMRMBufferValid_(RK_MRM const *const kobj,
                                RK_MRM_BUF const *const bufPtr)
{
    if ((kobj == NULL) || (bufPtr == NULL) ||
        (kobj->size > (RK_ULONG_MAX / (ULONG)RK_WORD_SIZE)))
    {
        return (RK_FALSE);
    }

    ULONG const dataSizeBytes = kobj->size * (ULONG)RK_WORD_SIZE;

    if (kMRMPartOwnsAllocatedBlock_(&kobj->mrmMem, bufPtr,
                                    sizeof(RK_MRM_BUF)) == RK_FALSE)
    {
        return (RK_FALSE);
    }

    return (kMRMPartOwnsAllocatedBlock_(&kobj->mrmDataMem, bufPtr->mrmData,
                                        dataSizeBytes));
}

static RK_ERR kMRMBadPoolBlock_(VOID)
{
    K_ERR_HANDLER(RK_FAULT_MEM_FREE);
    return (RK_ERR_MEM_FREE);
}
#endif

static RK_ERR kMRMReleaseBuffer_(RK_MRM *const kobj,
                                 RK_MRM_BUF *const bufPtr)
{
    VOID *const mrmDataPtr = bufPtr->mrmData;
    bufPtr->reserved = RK_FALSE;
    RK_ERR err = kMemPartitionFree(&kobj->mrmDataMem, mrmDataPtr);
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

    return (kMemPartitionFree(&kobj->mrmMem, (VOID *)bufPtr));
}

static RK_MRM_BUF *kMRMBufferAlloc_(RK_MRM *const kobj)
{
    RK_MRM_BUF *const allocPtr = kMemPartitionAlloc(&kobj->mrmMem);
    if (allocPtr == NULL)
    {
        return (NULL);
    }

    allocPtr->nUsers = 0UL;
    allocPtr->reserved = RK_FALSE;
    allocPtr->mrmData = (ULONG *)kMemPartitionAlloc(&kobj->mrmDataMem);
    if (allocPtr->mrmData == NULL)
    {
        (VOID)kMemPartitionFree(&kobj->mrmMem, allocPtr);
        return (NULL);
    }

    allocPtr->reserved = RK_TRUE;
    return (allocPtr);
}

static VOID kMRMPayloadCopy_(VOID *const dstPtr, VOID const *const srcPtr,
                             ULONG const dataSizeWords)
{
    RK_MEMCPY(dstPtr, srcPtr, dataSizeWords * (ULONG)RK_WORD_SIZE);
}

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

    if (mrmPoolPtr == NULL || mesgPoolPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        RK_CR_EXIT
        return (RK_ERR_OBJ_NULL);
    }

#endif
    if ((nBufs == 0UL) || (dataSizeWords == 0UL) ||
        (dataSizeWords > (RK_ULONG_MAX / (ULONG)RK_WORD_SIZE)))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_PARAM);
    }

    RK_ERR err = RK_ERR_ERROR;
    RK_BOOL mrmMemInit = RK_FALSE;

    err =
        kMemPartitionInit(&kobj->mrmMem, mrmPoolPtr, sizeof(RK_MRM_BUF), nBufs);
    if (err == RK_ERR_SUCCESS)
    {
        mrmMemInit = RK_TRUE;
        err = kMemPartitionInit(&kobj->mrmDataMem, mesgPoolPtr,
                                dataSizeWords * (ULONG)RK_WORD_SIZE, nBufs);
    }
    if (err == RK_ERR_SUCCESS)
    {
        /* nobody is using anything yet */
        kobj->currBufPtr = NULL;
        kobj->init = RK_TRUE;
        kobj->size = dataSizeWords;
        kobj->objID = RK_MRM_KOBJ_ID;
        kobj->objName[0] = '\0';
        kTraceRegisterObject(kobj, RK_MRM_KOBJ_ID);
    }
    else if (mrmMemInit == RK_TRUE)
    {
        kTraceUnregisterObject(&kobj->mrmMem);
        RK_MEMSET(&kobj->mrmMem, 0, sizeof(kobj->mrmMem));
    }

    RK_CR_EXIT
    return (err);
}

RK_MRM_BUF*kMRMReserve(RK_MRM *const kobj)
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
    if ((kobj->currBufPtr != NULL) && (kobj->currBufPtr->nUsers == 0UL) &&
        (kobj->currBufPtr->reserved != RK_TRUE))
    {
        allocPtr = kobj->currBufPtr;
        allocPtr->nUsers = 0UL;
        allocPtr->reserved = RK_TRUE;
    }
    else
    {
        allocPtr = kMRMBufferAlloc_(kobj);
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_RESERVE,
                       (allocPtr != NULL) ? RK_ERR_SUCCESS : RK_ERR_BUFFER_EMPTY,
                       kobj->mrmMem.nFreeBlocks);
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
    if (kMRMBufferValid_(kobj, bufPtr) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kMRMBadPoolBlock_());
    }
#endif
    if ((bufPtr->nUsers != 0UL) || (bufPtr->reserved != RK_TRUE))
    {
        RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
        return (kMRMBadPoolBlock_());
#else
        return (RK_ERR_MEM_FREE);
#endif
    }

    ULONG const dataSizeWords = kobj->size;
    VOID *const mrmDataPtr = bufPtr->mrmData;
    RK_CR_EXIT

    kMRMPayloadCopy_(mrmDataPtr, pubMesgPtr, dataSizeWords);

    RK_CR_ENTER
#if (RK_CONF_ERR_CHECK == ON)
    if ((kobj->init != RK_TRUE) || (kobj->objID != RK_MRM_KOBJ_ID) ||
        (kMRMBufferValid_(kobj, bufPtr) == RK_FALSE))
    {
        RK_CR_EXIT
        return (kMRMBadPoolBlock_());
    }
#endif
    if ((bufPtr->nUsers != 0UL) || (bufPtr->reserved != RK_TRUE))
    {
        RK_CR_EXIT
#if (RK_CONF_ERR_CHECK == ON)
        return (kMRMBadPoolBlock_());
#else
        return (RK_ERR_MEM_FREE);
#endif
    }

    if ((kobj->currBufPtr != NULL) && (kobj->currBufPtr != bufPtr) &&
        (kobj->currBufPtr->nUsers == 0UL) &&
        (kobj->currBufPtr->reserved != RK_TRUE))
    {
        RK_ERR const err = kMRMReleaseBuffer_(kobj, kobj->currBufPtr);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (err);
        }
    }

    bufPtr->reserved = RK_FALSE;
    kobj->currBufPtr = bufPtr;
    kTraceRecordObject(kobj, RK_TRACE_OP_PUBLISH, RK_ERR_SUCCESS,
                       bufPtr->nUsers);
    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

RK_MRM_BUF*kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr)
{
    RK_MRM_BUF *getBufPtr = NULL;
    VOID const *mrmDataPtr = NULL;
    ULONG dataSizeWords = 0UL;

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
    if ((kobj->currBufPtr == NULL) ||
        (kobj->currBufPtr->reserved == RK_TRUE))
    {
        RK_CR_EXIT
        return (NULL);
    }
    getBufPtr = kobj->currBufPtr;
    getBufPtr->nUsers++;
    mrmDataPtr = getBufPtr->mrmData;
    dataSizeWords = kobj->size;
    kTraceRecordObject(kobj, RK_TRACE_OP_GET, RK_ERR_SUCCESS,
                       getBufPtr->nUsers);
    RK_CR_EXIT

    kMRMPayloadCopy_(getMesgPtr, mrmDataPtr, dataSizeWords);
    return (getBufPtr);
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
    if (kMRMBufferValid_(kobj, bufPtr) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kMRMBadPoolBlock_());
    }

#endif

    if (bufPtr->nUsers == 0UL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        RK_CR_EXIT
        return (kMRMBadPoolBlock_());
#else
        RK_CR_EXIT
        return (RK_ERR_MEM_FREE);
#endif
    }

    RK_ERR err = RK_ERR_SUCCESS;
    bufPtr->nUsers--;
    ULONG const remainingUsers = bufPtr->nUsers;

    /* deallocate if not used and not the most recent buffer */
    if ((bufPtr->nUsers == 0) && (kobj->currBufPtr != bufPtr))
    {
        err = kMRMReleaseBuffer_(kobj, bufPtr);
        if (err != RK_ERR_SUCCESS)
        {
            kTraceRecordObject(kobj, RK_TRACE_OP_UNGET, err, remainingUsers);
            RK_CR_EXIT
            return (err);
        }
    }

    kTraceRecordObject(kobj, RK_TRACE_OP_UNGET, err, remainingUsers);
    RK_CR_EXIT
    return (err);
}
#endif
