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

RK_ERR kMRMUngetCore(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr)
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

    kTraceRecordObject(kobj, RK_TRACE_OP_UNGET, err, bufPtr->nUsers);
    RK_CR_EXIT
    return (err);
}

#endif
