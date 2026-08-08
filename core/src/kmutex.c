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
/* COMPONENT: MUTEX LOCK                                                      */
/******************************************************************************/

#define RK_SOURCE_CODE

#include "kmutex_private.h"

#if (RK_CONF_MUTEX == ON)

/******************************************************************************/
/* MUTEX SEMAPHORE                                                            */
/******************************************************************************/
/* There is no recursive lock. Unlocking a mutex you do not own hard-faults. */

VOID kMutexTimeoutWaiter(RK_TCB *const waiterPtr)
{
    if ((waiterPtr == NULL) || (waiterPtr->waitingForMutexPtr == NULL))
    {
        return;
    }

    RK_MUTEX *const mtxPtr = waiterPtr->waitingForMutexPtr;
    if ((mtxPtr->protocol == RK_PRIO_INHERITANCE) &&
        (mtxPtr->ownerPtr != NULL))
    {
        kMutexUpdateOwnerPrio_(mtxPtr->ownerPtr);
    }
}




#endif /* mutex */
