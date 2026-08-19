/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.71.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/
/* COMPONENT: DYNAMIC KERNEL OBJECT PARTITIONS                                */
/******************************************************************************/

#define RK_SOURCE_CODE

#include <kdynobjs.h>
#include <kerr.h>
#include <kmem.h>
#include <kmesgq.h>
#include <kmrm.h>
#include <kmutex.h>
#include <ksema.h>
#include <ksleepq.h>
#include <kstring.h>
#include <ktimer.h>
#include <ktrace.h>

#if (RK_CONF_DYNAMIC_OBJECTS == ON)

static RK_BOOL dynObjPartitionsInit;

/* declare the partitions */
#if ((RK_CONF_SEMAPHORE == ON) && (RK_CONF_DYNAMIC_SEMAPHORES_MAX > 0U))
static RK_MEM_PARTITION dynSemaPart;
static ULONG dynSemaPool[RK_CONF_DYNAMIC_SEMAPHORES_MAX]
                         [RK_TYPE_WORD_COUNT(RK_SEMAPHORE)] K_ALIGN(4);
#endif

#if ((RK_CONF_MUTEX == ON) && (RK_CONF_DYNAMIC_MUTEXES_MAX > 0U))
static RK_MEM_PARTITION dynMutexPart;
static ULONG dynMutexPool[RK_CONF_DYNAMIC_MUTEXES_MAX]
                          [RK_TYPE_WORD_COUNT(RK_MUTEX)] K_ALIGN(4);
#endif

#if ((RK_CONF_SLEEP_QUEUE == ON) && (RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX > 0U))
static RK_MEM_PARTITION dynSleepqPart;
static ULONG dynSleepqPool[RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX]
                           [RK_TYPE_WORD_COUNT(RK_SLEEP_QUEUE)] K_ALIGN(4);
#endif

#if ((RK_CONF_MESG_QUEUE == ON) && (RK_CONF_DYNAMIC_MESG_QUEUES_MAX > 0U))
static RK_MEM_PARTITION dynMesgqPart;
static ULONG dynMesgqPool[RK_CONF_DYNAMIC_MESG_QUEUES_MAX]
                          [RK_TYPE_WORD_COUNT(RK_MESG_QUEUE)] K_ALIGN(4);
#endif

#if ((RK_CONF_CALLOUT_TIMER == ON) && (RK_CONF_DYNAMIC_TIMERS_MAX > 0U))
static RK_MEM_PARTITION dynTimerPart;
static ULONG dynTimerPool[RK_CONF_DYNAMIC_TIMERS_MAX]
                          [RK_TYPE_WORD_COUNT(RK_TIMER)] K_ALIGN(4);
#endif

#if ((RK_CONF_MRM == ON) && (RK_CONF_DYNAMIC_MRMS_MAX > 0U))
static RK_MEM_PARTITION dynMrmPart;
static ULONG dynMrmPool[RK_CONF_DYNAMIC_MRMS_MAX]
                        [RK_TYPE_WORD_COUNT(RK_MRM)] K_ALIGN(4);
#endif


/* helpers for checking Ready, Valid/Range, Init, Put, Get */
static RK_ERR kDynObjCheckReady_(VOID)
{
    if (dynObjPartitionsInit != RK_TRUE)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NOT_INIT);
#endif
        return (RK_ERR_OBJ_NOT_INIT);
    }

    if (kIsISR())
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
#endif
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    return (RK_ERR_SUCCESS);
}

static RK_BOOL kDynObjPartValid_(RK_MEM_PARTITION const *const partPtr)
{
    return (((partPtr != NULL) &&
             (partPtr->objID == RK_MEMALLOC_KOBJ_ID) &&
             (partPtr->init == RK_TRUE))
                ? RK_TRUE
                : RK_FALSE);
}

static RK_BOOL kDynObjPartOwnsBlock_(RK_MEM_PARTITION const *const partPtr,
                                     VOID const *const blockPtr,
                                     ULONG const minSize)
{
    if ((kDynObjPartValid_(partPtr) == RK_FALSE) || (blockPtr == NULL) ||
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

static RK_ERR kDynObjInitPart_(RK_MEM_PARTITION *const partPtr,
                               VOID *const poolPtr,
                               ULONG const objSize,
                               ULONG const maxObjects,
                               CHAR const *const namePtr)
{
    if (partPtr->init == RK_TRUE)
    {
        return (RK_ERR_SUCCESS);
    }

    RK_ERR err = kMemPartitionInit(partPtr, poolPtr, objSize, maxObjects);
    if (err == RK_ERR_SUCCESS)
    {
        kTraceNameObject(partPtr, namePtr);
    }
    return (err);
}

static RK_ERR kDynObjReleaseBlock_(RK_MEM_PARTITION *const partPtr,
                                   VOID *const objPtr,
                                   ULONG const objSize)
{
    RK_MEMSET(objPtr, 0, objSize);

    return (kMemPartitionFree(partPtr, objPtr));
}

static RK_ERR kDynObjInvalidState_(VOID)
{
#if (RK_CONF_ERR_CHECK == ON)
    K_ERR_HANDLER(RK_FAULT_TASK_INVALID_STATE);
#endif
    return (RK_ERR_TASK_INVALID_ST);
}

static RK_ERR kDynObjBadPoolBlock_(VOID)
{
#if (RK_CONF_ERR_CHECK == ON)
    K_ERR_HANDLER(RK_FAULT_MEM_FREE);
#endif
    return (RK_ERR_MEM_FREE);
}

/* init all partitions that must be init */
RK_ERR kObjPartitionsInit(VOID)
{
    if (kIsISR())
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_ISR_PRIMITIVE);
#endif
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    }

    RK_ERR err = RK_ERR_SUCCESS;

#if ((RK_CONF_SEMAPHORE == ON) && (RK_CONF_DYNAMIC_SEMAPHORES_MAX > 0U))
    err = kDynObjInitPart_(&dynSemaPart, dynSemaPool, sizeof(RK_SEMAPHORE),
                           RK_CONF_DYNAMIC_SEMAPHORES_MAX, "DynSem");
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
#endif

#if ((RK_CONF_MUTEX == ON) && (RK_CONF_DYNAMIC_MUTEXES_MAX > 0U))
    err = kDynObjInitPart_(&dynMutexPart, dynMutexPool, sizeof(RK_MUTEX),
                           RK_CONF_DYNAMIC_MUTEXES_MAX, "DynMut");
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
#endif

#if ((RK_CONF_SLEEP_QUEUE == ON) && (RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX > 0U))
    err = kDynObjInitPart_(&dynSleepqPart, dynSleepqPool,
                           sizeof(RK_SLEEP_QUEUE),
                           RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX, "DynSlp");
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
#endif

#if ((RK_CONF_MESG_QUEUE == ON) && (RK_CONF_DYNAMIC_MESG_QUEUES_MAX > 0U))
    err = kDynObjInitPart_(&dynMesgqPart, dynMesgqPool,
                           sizeof(RK_MESG_QUEUE),
                           RK_CONF_DYNAMIC_MESG_QUEUES_MAX, "DynMsg");
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
#endif

#if ((RK_CONF_CALLOUT_TIMER == ON) && (RK_CONF_DYNAMIC_TIMERS_MAX > 0U))
    err = kDynObjInitPart_(&dynTimerPart, dynTimerPool, sizeof(RK_TIMER),
                           RK_CONF_DYNAMIC_TIMERS_MAX, "DynTmr");
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
#endif

#if ((RK_CONF_MRM == ON) && (RK_CONF_DYNAMIC_MRMS_MAX > 0U))
    err = kDynObjInitPart_(&dynMrmPart, dynMrmPool, sizeof(RK_MRM),
                           RK_CONF_DYNAMIC_MRMS_MAX, "DynMRM");
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }
#endif

    dynObjPartitionsInit = RK_TRUE;
    return (RK_ERR_SUCCESS);
}

/* create/destroy methods per object */
/* note _HANDLE is already a pointer */

#if (RK_CONF_SEMAPHORE == ON)
RK_ERR kSemaphoreCreate(RK_SEMAPHORE_HANDLE *const semaHandlePtr,
                        UINT const initValue,
                        UINT const maxValue)
{
    if (semaHandlePtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }
    *semaHandlePtr = NULL;

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_SEMAPHORES_MAX > 0U)
    RK_SEMAPHORE_HANDLE semaPtr =
        (RK_SEMAPHORE_HANDLE)kMemPartitionAlloc(&dynSemaPart);
    if (semaPtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_MEMSET(semaPtr, 0, sizeof(RK_SEMAPHORE));
    err = kSemaphoreInit(semaPtr, initValue, maxValue);
    if (err != RK_ERR_SUCCESS)
    {
        RK_MEMSET(semaPtr, 0, sizeof(RK_SEMAPHORE));
        kMemPartitionFree(&dynSemaPart, semaPtr);
        return (err);
    }

    *semaHandlePtr = semaPtr;
    return (RK_ERR_SUCCESS);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}

RK_ERR kSemaphoreDestroy(RK_SEMAPHORE_HANDLE *const semaHandlePtr)
{
    if ((semaHandlePtr == NULL) || (*semaHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_SEMAPHORES_MAX > 0U)
    RK_CR_AREA
    RK_CR_ENTER

    RK_SEMAPHORE_HANDLE const semaPtr = *semaHandlePtr;
    if (kDynObjPartOwnsBlock_(&dynSemaPart, semaPtr,
                              sizeof(RK_SEMAPHORE)) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kDynObjBadPoolBlock_());
    }

    if ((semaPtr->objID != RK_SEMAPHORE_KOBJ_ID) ||
        (semaPtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (semaPtr->waitingQueue.size > 0UL)
    {
        RK_CR_EXIT
        return (kDynObjInvalidState_());
    }

    kTraceRecordObject(semaPtr, RK_TRACE_OP_FREE, RK_ERR_SUCCESS,
                       semaPtr->value);
    kTraceUnregisterObject(semaPtr);
    err = kDynObjReleaseBlock_(&dynSemaPart, semaPtr,
                               sizeof(RK_SEMAPHORE));
    if (err == RK_ERR_SUCCESS)
    {
        *semaHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}
#endif

#if (RK_CONF_MUTEX == ON)
RK_ERR kMutexCreate(RK_MUTEX_HANDLE *const mutexHandlePtr,
                    UINT const protocol)
{
    if (mutexHandlePtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }
    *mutexHandlePtr = NULL;

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_MUTEXES_MAX > 0U)
    RK_MUTEX_HANDLE mutexPtr =
        (RK_MUTEX_HANDLE)kMemPartitionAlloc(&dynMutexPart);
    if (mutexPtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_MEMSET(mutexPtr, 0, sizeof(RK_MUTEX));
    err = kMutexInit(mutexPtr, protocol);
    if (err != RK_ERR_SUCCESS)
    {
        RK_MEMSET(mutexPtr, 0, sizeof(RK_MUTEX));
        kMemPartitionFree(&dynMutexPart, mutexPtr);
        return (err);
    }

    *mutexHandlePtr = mutexPtr;
    return (RK_ERR_SUCCESS);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}

RK_ERR kMutexDestroy(RK_MUTEX_HANDLE *const mutexHandlePtr)
{
    if ((mutexHandlePtr == NULL) || (*mutexHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_MUTEXES_MAX > 0U)
    RK_CR_AREA
    RK_CR_ENTER

    RK_MUTEX_HANDLE const mutexPtr = *mutexHandlePtr;
    if (kDynObjPartOwnsBlock_(&dynMutexPart, mutexPtr,
                              sizeof(RK_MUTEX)) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kDynObjBadPoolBlock_());
    }

    if ((mutexPtr->objID != RK_MUTEX_KOBJ_ID) ||
        (mutexPtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (mutexPtr->lock != RK_FALSE)
    {
        RK_CR_EXIT
        return (RK_ERR_MUTEX_LOCKED);
    }

    if ((mutexPtr->ownerPtr != NULL) || (mutexPtr->waitingQueue.size > 0UL))
    {
        RK_CR_EXIT
        return (kDynObjInvalidState_());
    }

    kTraceRecordObject(mutexPtr, RK_TRACE_OP_FREE, RK_ERR_SUCCESS, 0UL);
    kTraceUnregisterObject(mutexPtr);
    err = kDynObjReleaseBlock_(&dynMutexPart, mutexPtr,
                               sizeof(RK_MUTEX));
    if (err == RK_ERR_SUCCESS)
    {
        *mutexHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}
#endif

#if (RK_CONF_SLEEP_QUEUE == ON)
RK_ERR kSleepQueueCreate(RK_SLEEP_QUEUE_HANDLE *const sleepqHandlePtr)
{
    if (sleepqHandlePtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }
    *sleepqHandlePtr = NULL;

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX > 0U)
    RK_SLEEP_QUEUE_HANDLE sleepqPtr =
        (RK_SLEEP_QUEUE_HANDLE)kMemPartitionAlloc(&dynSleepqPart);
    if (sleepqPtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_MEMSET(sleepqPtr, 0, sizeof(RK_SLEEP_QUEUE));
    err = kSleepQueueInit(sleepqPtr);
    if (err != RK_ERR_SUCCESS)
    {
        RK_MEMSET(sleepqPtr, 0, sizeof(RK_SLEEP_QUEUE));
        kMemPartitionFree(&dynSleepqPart, sleepqPtr);
        return (err);
    }

    *sleepqHandlePtr = sleepqPtr;
    return (RK_ERR_SUCCESS);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}

RK_ERR kSleepQueueDestroy(RK_SLEEP_QUEUE_HANDLE *const sleepqHandlePtr)
{
    if ((sleepqHandlePtr == NULL) || (*sleepqHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_SLEEP_QUEUES_MAX > 0U)
    RK_CR_AREA
    RK_CR_ENTER

    RK_SLEEP_QUEUE_HANDLE const sleepqPtr = *sleepqHandlePtr;
    if (kDynObjPartOwnsBlock_(&dynSleepqPart, sleepqPtr,
                              sizeof(RK_SLEEP_QUEUE)) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kDynObjBadPoolBlock_());
    }

    if ((sleepqPtr->objID != RK_SLEEPQ_KOBJ_ID) ||
        (sleepqPtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if (sleepqPtr->waitingQueue.size > 0UL)
    {
        RK_CR_EXIT
        return (kDynObjInvalidState_());
    }

    kTraceRecordObject(sleepqPtr, RK_TRACE_OP_FREE, RK_ERR_SUCCESS, 0UL);
    kTraceUnregisterObject(sleepqPtr);
    err = kDynObjReleaseBlock_(&dynSleepqPart, sleepqPtr,
                               sizeof(RK_SLEEP_QUEUE));
    if (err == RK_ERR_SUCCESS)
    {
        *sleepqHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}
#endif

#if (RK_CONF_MESG_QUEUE == ON)
RK_ERR kMesgQueueCreate(RK_MESG_QUEUE_HANDLE *const queueHandlePtr,
                        VOID *const bufPtr,
                        ULONG const mesgWords,
                        ULONG const depth)
{
    if (queueHandlePtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }
    *queueHandlePtr = NULL;

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_MESG_QUEUES_MAX > 0U)
    RK_MESG_QUEUE_HANDLE queuePtr =
        (RK_MESG_QUEUE_HANDLE)kMemPartitionAlloc(&dynMesgqPart);
    if (queuePtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_MEMSET(queuePtr, 0, sizeof(RK_MESG_QUEUE));
    err = kMesgQueueInit(queuePtr, bufPtr, mesgWords, depth);
    if (err != RK_ERR_SUCCESS)
    {
        RK_MEMSET(queuePtr, 0, sizeof(RK_MESG_QUEUE));
        kMemPartitionFree(&dynMesgqPart, queuePtr);
        return (err);
    }

    *queueHandlePtr = queuePtr;
    return (RK_ERR_SUCCESS);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}

RK_ERR kMesgQueueDestroy(RK_MESG_QUEUE_HANDLE *const queueHandlePtr)
{
    if ((queueHandlePtr == NULL) || (*queueHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_MESG_QUEUES_MAX > 0U)
    RK_CR_AREA
    RK_CR_ENTER

    RK_MESG_QUEUE_HANDLE const queuePtr = *queueHandlePtr;
    if (kDynObjPartOwnsBlock_(&dynMesgqPart, queuePtr,
                              sizeof(RK_MESG_QUEUE)) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kDynObjBadPoolBlock_());
    }

    if ((queuePtr->objID != RK_MESGQQUEUE_KOBJ_ID) ||
        (queuePtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    if ((queuePtr->waitingReceivers.size > 0UL) ||
        (queuePtr->waitingSenders.size > 0UL) ||
        (queuePtr->ringBuf.nFull > 0UL) ||
        (queuePtr->broadcastReceivers > 0UL))
    {
        RK_CR_EXIT
        return (kDynObjInvalidState_());
    }

    kTraceRecordObject(queuePtr, RK_TRACE_OP_FREE, RK_ERR_SUCCESS, 0UL);
    kTraceUnregisterObject(queuePtr);
    err = kDynObjReleaseBlock_(&dynMesgqPart, queuePtr,
                               sizeof(RK_MESG_QUEUE));
    if (err == RK_ERR_SUCCESS)
    {
        *queueHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}
#endif

/* timers are specially useful to be dynamic if using one-shot */

#if (RK_CONF_CALLOUT_TIMER == ON)
RK_ERR kTimerCreate(RK_TIMER_HANDLE *const timerHandlePtr,
                    RK_TICK const phase,
                    RK_TICK const countTicks,
                    RK_TIMER_CALLOUT const funPtr,
                    VOID *const argsPtr,
                    RK_OPTION const reload)
{
    if (timerHandlePtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }
    *timerHandlePtr = NULL;

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_TIMERS_MAX > 0U)
    RK_TIMER_HANDLE timerPtr =
        (RK_TIMER_HANDLE)kMemPartitionAlloc(&dynTimerPart);
    if (timerPtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_MEMSET(timerPtr, 0, sizeof(RK_TIMER));
    err = kTimerInit(timerPtr, phase, countTicks, funPtr, argsPtr, reload);
    if (err != RK_ERR_SUCCESS)
    {
        RK_MEMSET(timerPtr, 0, sizeof(RK_TIMER));
        kMemPartitionFree(&dynTimerPart, timerPtr);
        return (err);
    }

    *timerHandlePtr = timerPtr;
    return (RK_ERR_SUCCESS);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}

RK_ERR kTimerDestroy(RK_TIMER_HANDLE *const timerHandlePtr)
{
    if ((timerHandlePtr == NULL) || (*timerHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_TIMERS_MAX > 0U)
    RK_CR_AREA
    RK_CR_ENTER

    RK_TIMER_HANDLE const timerPtr = *timerHandlePtr;
    if (kDynObjPartOwnsBlock_(&dynTimerPart, timerPtr,
                              sizeof(RK_TIMER)) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kDynObjBadPoolBlock_());
    }

    if ((timerPtr->objID != RK_TIMER_KOBJ_ID) || (timerPtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    err = kTimerCancel(timerPtr);
    if (err != RK_ERR_SUCCESS)
    {
        RK_CR_EXIT
        return (err);
    }

    kTraceRecordObject(timerPtr, RK_TRACE_OP_FREE, RK_ERR_SUCCESS, 0UL);
    kTraceUnregisterObject(timerPtr);
    err = kDynObjReleaseBlock_(&dynTimerPart, timerPtr,
                               sizeof(RK_TIMER));
    if (err == RK_ERR_SUCCESS)
    {
        *timerHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}
#endif

#if (RK_CONF_MRM == ON)
RK_ERR kMRMCreate(RK_MRM_HANDLE *const mrmHandlePtr,
                  RK_MRM_BUF *const mrmPoolPtr,
                  VOID *mesgPoolPtr,
                  ULONG const nBufs,
                  ULONG const dataSizeWords)
{
    if (mrmHandlePtr == NULL)
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }
    *mrmHandlePtr = NULL;

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_MRMS_MAX > 0U)
    RK_MRM_HANDLE mrmPtr = (RK_MRM_HANDLE)kMemPartitionAlloc(&dynMrmPart);
    if (mrmPtr == NULL)
    {
        return (RK_ERR_BUFFER_EMPTY);
    }

    RK_MEMSET(mrmPtr, 0, sizeof(RK_MRM));
    err = kMRMInit(mrmPtr, mrmPoolPtr, mesgPoolPtr, nBufs, dataSizeWords);
    if (err != RK_ERR_SUCCESS)
    {
        RK_MEMSET(mrmPtr, 0, sizeof(RK_MRM));
        kMemPartitionFree(&dynMrmPart, mrmPtr);
        return (err);
    }

    *mrmHandlePtr = mrmPtr;
    return (RK_ERR_SUCCESS);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}

RK_ERR kMRMDestroy(RK_MRM_HANDLE *const mrmHandlePtr)
{
    if ((mrmHandlePtr == NULL) || (*mrmHandlePtr == NULL))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_OBJ_NULL);
#endif
        return (RK_ERR_OBJ_NULL);
    }

    RK_ERR err = kDynObjCheckReady_();
    if (err != RK_ERR_SUCCESS)
    {
        return (err);
    }

#if (RK_CONF_DYNAMIC_MRMS_MAX > 0U)
    RK_CR_AREA
    RK_CR_ENTER

    RK_MRM_HANDLE const mrmPtr = *mrmHandlePtr;
    if (kDynObjPartOwnsBlock_(&dynMrmPart, mrmPtr,
                              sizeof(RK_MRM)) == RK_FALSE)
    {
        RK_CR_EXIT
        return (kDynObjBadPoolBlock_());
    }

    if ((mrmPtr->objID != RK_MRM_KOBJ_ID) || (mrmPtr->init != RK_TRUE))
    {
#if (RK_CONF_ERR_CHECK == ON)
        K_ERR_HANDLER(RK_FAULT_INVALID_OBJ);
#endif
        RK_CR_EXIT
        return (RK_ERR_INVALID_OBJ);
    }

    ULONG const currentHeld = (mrmPtr->currBufPtr != NULL) ? 1UL : 0UL;
    if (((mrmPtr->currBufPtr != NULL) &&
         (mrmPtr->currBufPtr->nUsers > 0UL)) ||
        ((mrmPtr->mrmMem.nMaxBlocks - mrmPtr->mrmMem.nFreeBlocks) !=
         currentHeld) ||
        ((mrmPtr->mrmDataMem.nMaxBlocks - mrmPtr->mrmDataMem.nFreeBlocks) !=
         currentHeld))
    {
        RK_CR_EXIT
        return (kDynObjInvalidState_());
    }

    if (mrmPtr->currBufPtr != NULL)
    {
        VOID *const dataPtr = mrmPtr->currBufPtr->mrmData;
        err = kMemPartitionFree(&mrmPtr->mrmDataMem, dataPtr);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (err);
        }
        err = kMemPartitionFree(&mrmPtr->mrmMem, mrmPtr->currBufPtr);
        if (err != RK_ERR_SUCCESS)
        {
            RK_CR_EXIT
            return (err);
        }
        mrmPtr->currBufPtr = NULL;
    }

    kTraceUnregisterObject(&mrmPtr->mrmDataMem);
    kTraceUnregisterObject(&mrmPtr->mrmMem);
    kTraceRecordObject(mrmPtr, RK_TRACE_OP_FREE, RK_ERR_SUCCESS, 0UL);
    kTraceUnregisterObject(mrmPtr);
    err = kDynObjReleaseBlock_(&dynMrmPart, mrmPtr,
                               sizeof(RK_MRM));
    if (err == RK_ERR_SUCCESS)
    {
        *mrmHandlePtr = NULL;
    }
    RK_CR_EXIT
    return (err);
#else
    return (RK_ERR_BUFFER_EMPTY);
#endif
}
#endif

#endif /* RK_CONF_DYNAMIC_OBJECTS */
