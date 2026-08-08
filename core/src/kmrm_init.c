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

RK_ERR kMRMInitCore(RK_MRM *const kobj, RK_MRM_BUF *const mrmPoolPtr,
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
    RK_ERR err = RK_ERR_ERROR;

    err =
        kMemPartitionInit(&kobj->mrmMem, mrmPoolPtr, sizeof(RK_MRM_BUF), nBufs);
    if (!err)
        err = kMemPartitionInit(&kobj->mrmDataMem, mesgPoolPtr,
                                dataSizeWords * 4, nBufs);
    if (!err)
    {
        /* nobody is using anything yet */
        kobj->currBufPtr = NULL;
        kobj->init = RK_TRUE;
        kobj->size = dataSizeWords;
        kobj->objID = RK_MRM_KOBJ_ID;
        kobj->objName[0] = '\0';
        kTraceRegisterObject(kobj, RK_MRM_KOBJ_ID);
    }

    RK_CR_EXIT
    return (err);
}

#endif
