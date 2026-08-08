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
/* COMPONENT: SLEEP QUEUE                                                     */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <ksleepq.h>
#include <ksystasks.h>
#include <ktimer.h>
#include <ktrace.h>
#include <kmutex.h>

#if (RK_CONF_SLEEP_QUEUE == ON)

RK_ERR kCondVarWaitCore(RK_SLEEP_QUEUE *const cv, RK_MUTEX *const mutex,
                    RK_TICK timeout)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (kIsISR())
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }
#endif


    kPreemptDisable();
    RK_ERR err = kMutexUnlock(mutex);
    RK_BOOL const mutexReleased = (err == RK_ERR_SUCCESS) ? RK_TRUE : RK_FALSE;
    if (mutexReleased == RK_TRUE)
    {
        RK_TICK remaining = timeout;

        err = kSleepQueueWait(cv, remaining);
    }
    kPreemptEnable();

    if (mutexReleased == RK_FALSE)
    {
        return (err);
    }

    RK_ERR const waitErr = err;
    RK_ERR const lockErr = kMutexLock(mutex, RK_WAIT_FOREVER);

    return ((lockErr != RK_ERR_SUCCESS) ? lockErr : waitErr);
}

#endif
