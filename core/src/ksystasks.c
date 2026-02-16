/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.9.18                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: SYSTEM TASKS                                                    */
/******************************************************************************/

#define RK_SOURCE_CODE
#include <ksystasks.h>
#include <kerr.h>
#include <ktaskevents.h>
#include <ksema.h>
#include <ksleepq.h>
#include <kmesgq.h>

UINT RK_gIdleStack[RK_CONF_IDLE_STACKSIZE] K_ALIGN(8);
UINT RK_gPostProcStack[RK_CONF_POSTPROC_STACKSIZE] K_ALIGN(8);

#define RK_POSTPROC_Q_LEN ((UINT)RK_NTHREADS)

/* deferred post-proc routines */
/* mainly lenghty loops that need keeping irqs disabled */
typedef struct RK_POSTPROC_JOB_ENTRY
{
    UINT jobType;
    VOID *objPtr;
    UINT nTasks;
} RK_POSTPROC_JOB_ENTRY;

static RK_POSTPROC_JOB_ENTRY RK_gPostProcQ[RK_POSTPROC_Q_LEN];
static volatile UINT RK_gPostProcHead = 0U;
static volatile UINT RK_gPostProcTail = 0U;
static volatile UINT RK_gPostProcCount = 0U;

static RK_ERR kPostProcJobDeq_(RK_POSTPROC_JOB_ENTRY *const jobPtr)
{
    RK_CR_AREA
    RK_CR_ENTER

    if ((jobPtr == NULL) || (RK_gPostProcCount == 0U))
    {
        RK_CR_EXIT
        return (RK_ERR_LIST_EMPTY);
    }

    *jobPtr = RK_gPostProcQ[RK_gPostProcHead];
    RK_gPostProcHead = (RK_gPostProcHead + 1U) % RK_POSTPROC_Q_LEN;
    RK_gPostProcCount -= 1U;

    RK_CR_EXIT
    return (RK_ERR_SUCCESS);
}

static VOID kRunPostProcJobs_(VOID)
{
    UINT budget = RK_POSTPROC_Q_LEN;

    while (budget > 0U)
    {
        RK_POSTPROC_JOB_ENTRY job;
        if (kPostProcJobDeq_(&job) != RK_ERR_SUCCESS)
        {
            break;
        }

        switch (job.jobType)
        {
#if (RK_CONF_SEMAPHORE == ON)
            case RK_POSTPROC_JOB_SEMA_FLUSH:
                kSemaphoreFlush((RK_SEMAPHORE *)job.objPtr);
                break;
#endif
#if (RK_CONF_SLEEP_QUEUE == ON)
            case RK_POSTPROC_JOB_SLEEPQ_WAKE:
                kSleepQueueWake((RK_SLEEP_QUEUE *)job.objPtr, job.nTasks, NULL);
                break;
#endif
#if (RK_CONF_MESG_QUEUE == ON)
            case RK_POSTPROC_JOB_MESGQ_RESET:
                kMesgQueueReset((RK_MESG_QUEUE *)job.objPtr);
                break;
#endif
            default:
                break;
        }
        budget -= 1U;
    }
}

RK_ERR kPostProcJobEnq(UINT jobType, VOID *const objPtr, UINT nTasks)
{
#if (RK_CONF_ERR_CHECK == ON)
    if (objPtr == NULL)
    {
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
        return (RK_ERR_OBJ_NULL);
    }

    UINT validType = RK_FALSE;
#if (RK_CONF_SEMAPHORE == ON)
    if (jobType == RK_POSTPROC_JOB_SEMA_FLUSH)
    {
        validType = RK_TRUE;
    }
#endif
#if (RK_CONF_SLEEP_QUEUE == ON)
    if (jobType == RK_POSTPROC_JOB_SLEEPQ_WAKE)
    {
        validType = RK_TRUE;
    }
#endif
#if (RK_CONF_MESG_QUEUE == ON)
    if (jobType == RK_POSTPROC_JOB_MESGQ_RESET)
    {
        validType = RK_TRUE;
    }
#endif
    if (validType == RK_FALSE)
    {
        K_ERR_HANDLER(RK_FAULT_INVALID_PARAM);
        return (RK_ERR_INVALID_PARAM);
    }
#endif

    RK_CR_AREA
    RK_CR_ENTER

    if (RK_gPostProcCount >= RK_POSTPROC_Q_LEN)
    {
        RK_CR_EXIT
        return (RK_ERR_NOWAIT);
    }

    RK_gPostProcQ[RK_gPostProcTail].jobType = jobType;
    RK_gPostProcQ[RK_gPostProcTail].objPtr = objPtr;
    RK_gPostProcQ[RK_gPostProcTail].nTasks = nTasks;

    RK_gPostProcTail = (RK_gPostProcTail + 1U) % RK_POSTPROC_Q_LEN;
    RK_gPostProcCount += 1U;

    RK_CR_EXIT
    return (kTaskEventSet(RK_gPostProcTaskHandle, RK_POSTPROC_SIG));
}

VOID IdleTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_ISB

        RK_WFI 

        RK_DSB
    }
}

VOID PostProcSysTask(VOID *args)
{
    RK_UNUSEARGS

    RK_REG_SYSTICK_CTRL |= 0x01;
    
    while (1)
    {
        ULONG gotFlags = 0;

        kTaskEventGet((RK_POSTPROC_SIG | RK_POSTPROC_TIMER_SIG),
                      RK_EVENT_FLAGS_ANY,
                      &gotFlags,
                      RK_WAIT_FOREVER);

        if ((gotFlags & RK_POSTPROC_SIG) != 0U)
        {
            kRunPostProcJobs_();
        }

#if (RK_CONF_CALLOUT_TIMER == ON)
        if ((gotFlags & RK_POSTPROC_TIMER_SIG) != 0U)
        {
            while (RK_gTimerListHeadPtr != NULL && RK_gTimerListHeadPtr->dtick == 0)
            {
                RK_TIMEOUT_NODE *node = (RK_TIMEOUT_NODE *)RK_gTimerListHeadPtr;
                RK_gTimerListHeadPtr = node->nextPtr;
                kRemoveTimerNode(node);

                RK_TIMER *timer = K_GET_CONTAINER_ADDR(node, RK_TIMER,
                                                       timeoutNode);
                if (timer->funPtr != NULL)
                {
                    timer->funPtr(timer->argsPtr);
                }
                if (timer->reload)
                {
                    RK_TICK now = kTickGet();
                    RK_TICK base = timer->nextTime;
                    RK_TICK elapsed = K_TICK_DELTA(now, base);
                    RK_TICK skips = ((elapsed / timer->period) + 1);
                    RK_TICK offset = (RK_TICK)(skips * timer->period);
                    timer->nextTime = K_TICK_ADD(base, offset);
                    RK_TICK delay = K_TICK_DELTA(timer->nextTime, now);
                    if (delay == 0)
                        K_PANIC("0 DELAY TIMER");
                    kTimerReload(timer, delay);
                }   
            }
        }
#endif
    }
}
