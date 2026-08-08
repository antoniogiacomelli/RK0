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

RK_MRM_BUF*kMRMGetCore(RK_MRM *const kobj, VOID *const getMesgPtr)
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
    if (kobj->currBufPtr == NULL)
    {
        RK_CR_EXIT
        return (NULL);
    }
    kobj->currBufPtr->nUsers++;
    ULONG *getMesgPtr_ = (ULONG *)getMesgPtr;
    ULONG const *mrmMesgPtr_ = (ULONG const *)kobj->currBufPtr->mrmData;
    for (ULONG i = 0; i < kobj->size; ++i)
    {
        getMesgPtr_[i] = mrmMesgPtr_[i];
    }
    kTraceRecordObject(kobj, RK_TRACE_OP_GET, RK_ERR_SUCCESS,
                       kobj->currBufPtr->nUsers);
    RK_CR_EXIT
    return (kobj->currBufPtr);
}

#endif
