/* SPDX-License-Identifier: Apache-2.0 */
/* Retained event messages: Exchange storage, ceiling monitor, selective wait. */
#define RK_SOURCE_CODE
#include <ksharedevents.h>
#include <kexchg.h>
#include <kmutex.h>
#include <ksch.h>
#include <ktimer.h>
#include <ktrace.h>

#if (RK_CONF_SHARED_EVENTS == ON)
/* Referenced only while the task belongs to the service's waiting queue. */
struct RK_SHARED_EVENTS_WAIT
{
    ULONG required;
    RK_OPTION options;
    ULONG *valuePtr;
};

static RK_ERR kSharedEventsLock_(RK_SHARED_EVENTS *const kobj,
                                 RK_TICK const timeout)
{
    if (kobj == NULL)
        return (RK_ERR_OBJ_NULL);
    if (kIsISR())
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    if (kobj->init != RK_TRUE)
        return (RK_ERR_OBJ_NOT_INIT);
    if (kobj->objID != RK_SHARED_EVENTS_KOBJ_ID)
        return (RK_ERR_INVALID_OBJ);
    RK_TASK_HANDLE const caller = kTaskGetRunningHandle();
    if ((caller == NULL) || (kobj->lock.ownerPtr == caller))
        return (RK_ERR_TASK_INVALID_ST);
    if (caller->prioNominal < kobj->lock.mutexPrioCeiling)
        return (RK_ERR_INVALID_PRIO);
    return (kMutexLock(&kobj->lock, timeout));
}

static RK_ERR kSharedEventsUnlock_(RK_SHARED_EVENTS *const kobj,
                                   RK_ERR const result)
{
    RK_ERR const err = kMutexUnlock(&kobj->lock);
    return ((result == RK_ERR_SUCCESS) ? err : result);
}

static RK_BOOL kSharedEventsMatch_(ULONG const value, ULONG const required,
                                   RK_OPTION const options)
{
    ULONG const matched = value & required;
    return ((options == RK_EVENT_ALL) ? (RK_BOOL)(matched == required)
                                      : (RK_BOOL)(matched != 0UL));
}

/* Caller owns the Mutex; release it before ending the completion batch. */
static RK_ERR kSharedEventsPublishLocked_(RK_SHARED_EVENTS *const kobj,
                                          ULONG const value)
{
    kSchLock();
    RK_CR_AREA
    RK_CR_ENTER
    kobj->value = value;
    RK_ERR const err = kExchangeOverwrite(&kobj->exchange, &kobj->value);
    if (err == RK_ERR_SUCCESS)
    {
        /* The tick ISR can remove timed-out tasks from this same queue. */
        RK_NODE *node = kobj->waitingQueue.listDummy.nextPtr;
        RK_TCB *chosen = NULL;
        while (node != &kobj->waitingQueue.listDummy)
        {
            RK_TCB *const task = K_GET_TCB_ADDR(node);
            node = node->nextPtr;
            struct RK_SHARED_EVENTS_WAIT *const request = task->sharedEventsWaitPtr;
            K_ASSERT(request != NULL);
            if (kSharedEventsMatch_(value, request->required, request->options)
                == RK_FALSE)
                continue;

            *request->valuePtr = value;
            task->sharedEventsWaitPtr = NULL;
            kWaitQRemove(&kobj->waitingQueue, task);
            if (task->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING)
            {
                kRemoveTimeoutNode(&task->timeoutNode);
                task->timeoutNode.timeoutType = 0U;
            }
            kReadyNoSwtch(task);
            if ((chosen == NULL) || (task->priority < chosen->priority))
                chosen = task;
            kTraceRecordObject(kobj, RK_TRACE_OP_WAKE, RK_ERR_SUCCESS,
                               kobj->waitingQueue.size);
        }
        if (chosen != NULL)
            kReschedTask(chosen);
        kTraceRecordObject(kobj, RK_TRACE_OP_PUBLISH, RK_ERR_SUCCESS, value);
    }
    RK_CR_EXIT
    RK_ERR const result = kSharedEventsUnlock_(kobj, err);
    kSchUnlock();
    return (result);
}

RK_ERR kSharedEventsInit(RK_SHARED_EVENTS *const kobj, ULONG const initialValue,
                         RK_PRIO const ceiling)
{
    if (kobj == NULL)
        return (RK_ERR_OBJ_NULL);
    if (kIsISR())
        return (RK_ERR_INVALID_ISR_PRIMITIVE);
    if (ceiling > RK_CONF_MIN_PRIO)
        return (RK_ERR_INVALID_PRIO);
    if ((kobj->init == RK_TRUE) || (kobj->lock.init == RK_TRUE) ||
        (kobj->exchange.init == RK_TRUE))
        return (RK_ERR_OBJ_DOUBLE_INIT);
    RK_ERR err = kMutexInit(&kobj->lock, RK_PRIO_CEILING, ceiling);
    if (err != RK_ERR_SUCCESS)
        return (err);
    kobj->value = initialValue;
    err = kExchangeInit(&kobj->exchange, &kobj->value);
    if (err != RK_ERR_SUCCESS)
        return (err);
    kTCBQInit(&kobj->waitingQueue);
    kobj->objID = RK_SHARED_EVENTS_KOBJ_ID;
    kobj->objName[0] = '\0';
    kobj->init = RK_TRUE;
    kTraceRegisterObject(kobj, RK_SHARED_EVENTS_KOBJ_ID);
    return (RK_ERR_SUCCESS);
}

RK_ERR kSharedEventsPublish(RK_SHARED_EVENTS *const kobj, ULONG const value)
{
    RK_ERR const err = kSharedEventsLock_(kobj, RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
        return (err);
    return (kSharedEventsPublishLocked_(kobj, value));
}

RK_ERR kSharedEventsUpdate(RK_SHARED_EVENTS *const kobj, ULONG const setMask,
                           ULONG const clearMask)
{
    RK_ERR err = kSharedEventsLock_(kobj, RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
        return (err);
    VOID *message = NULL;
    err = kExchangePeek(&kobj->exchange, &message);
    if (err != RK_ERR_SUCCESS)
        return (kSharedEventsUnlock_(kobj, err));
    ULONG const value = *(ULONG *)message;
    return (kSharedEventsPublishLocked_(kobj, (value & ~clearMask) | setMask));
}

static RK_ERR kSharedEventsRead_(RK_SHARED_EVENTS *const kobj,
                                 ULONG *const value, RK_BOOL const take)
{
    if (value == NULL)
        return (RK_ERR_OBJ_NULL);
    *value = 0UL;
    RK_ERR err = kSharedEventsLock_(kobj, RK_WAIT_FOREVER);
    if (err != RK_ERR_SUCCESS)
        return (err);
    VOID *message = NULL;
    err = (take == RK_TRUE) ? kExchangePend(&kobj->exchange, &message, RK_NO_WAIT)
                            : kExchangePeek(&kobj->exchange, &message);
    if (err == RK_ERR_SUCCESS)
        *value = *(ULONG *)message;
    kTraceRecordObject(kobj, (take == RK_TRUE) ? RK_TRACE_OP_RECV : RK_TRACE_OP_GET,
                       err, *value);
    return (kSharedEventsUnlock_(kobj, err));
}

RK_ERR kSharedEventsRead(RK_SHARED_EVENTS *const kobj, ULONG *const value)
{
    return (kSharedEventsRead_(kobj, value, RK_FALSE));
}

RK_ERR kSharedEventsTake(RK_SHARED_EVENTS *const kobj, ULONG *const value)
{
    return (kSharedEventsRead_(kobj, value, RK_TRUE));
}

RK_ERR kSharedEventsWait(RK_SHARED_EVENTS *const kobj, ULONG const required,
                         RK_OPTION const options, ULONG *const value,
                         RK_TICK const timeout)
{
    if (value == NULL)
        return (RK_ERR_OBJ_NULL);
    *value = 0UL;
    if ((required == 0UL) ||
        ((options != RK_EVENT_ANY) && (options != RK_EVENT_ALL)))
        return (RK_ERR_INVALID_PARAM);
    if ((timeout != RK_WAIT_FOREVER) && (timeout > RK_MAX_PERIOD))
        return (RK_ERR_INVALID_TIMEOUT);
    RK_TICK const deadline = (timeout == RK_WAIT_FOREVER) ? 0UL
                               : K_TICK_ADD(kTickGet(), timeout);
    RK_ERR err = kSharedEventsLock_(kobj, timeout);
    if (err != RK_ERR_SUCCESS)
        return (err);
    VOID *message = NULL;
    err = kExchangePeek(&kobj->exchange, &message);
    if ((err == RK_ERR_SUCCESS) &&
        (kSharedEventsMatch_(*(ULONG *)message, required, options) == RK_TRUE))
    {
        *value = *(ULONG *)message;
        kTraceRecordObject(kobj, RK_TRACE_OP_WAIT, RK_ERR_SUCCESS, *value);
        return (kSharedEventsUnlock_(kobj, RK_ERR_SUCCESS));
    }
    if ((err != RK_ERR_SUCCESS) && (err != RK_ERR_BUFFER_EMPTY))
        return (kSharedEventsUnlock_(kobj, err));
    if (timeout == RK_NO_WAIT)
        return (kSharedEventsUnlock_(kobj, RK_ERR_FLAGS_NOT_MET));

    /* No task may publish between releasing the Mutex and enrolling. */
    kSchLock();
    kMutexUnlock(&kobj->lock);
    struct RK_SHARED_EVENTS_WAIT request = {required, options, value};
    RK_TASK_HANDLE const task = kTaskGetRunningHandle();
    RK_CR_AREA
    RK_CR_ENTER
    if (timeout != RK_WAIT_FOREVER)
    {
        RK_STICK const remaining = K_TICK_DIFF(deadline, kTickGet());
        if (remaining <= 0)
            err = RK_ERR_TIMEOUT;
        else
        {
            task->timeoutNode.timeoutType = RK_TIMEOUT_BLOCKING;
            err = kTimeoutNodeAdd(&task->timeoutNode, (RK_TICK)remaining);
            if (err != RK_ERR_SUCCESS)
                task->timeoutNode.timeoutType = 0U;
        }
    }
    else
        err = RK_ERR_SUCCESS;
    if (err == RK_ERR_SUCCESS)
    {
        task->sharedEventsWaitPtr = &request;
        kWaitQEnqByPrio(&kobj->waitingQueue, task);
        task->status = RK_BLOCKED;
        kTraceRecordObject(kobj, RK_TRACE_OP_WAIT_BLOCK, RK_ERR_SUCCESS,
                           kobj->waitingQueue.size);
        kPendCtxSwtch();
    }
    RK_CR_EXIT

    RK_CR_ENTER
    if ((err == RK_ERR_SUCCESS) && (task->timeOut == RK_TRUE))
    {
        task->timeOut = RK_FALSE;
        err = RK_ERR_TIMEOUT;
    }
    task->sharedEventsWaitPtr = NULL;
    kTraceRecordObject(kobj, (err == RK_ERR_TIMEOUT) ? RK_TRACE_OP_TIMEOUT
                                                   : RK_TRACE_OP_WAIT,
                       err, *value);
    RK_CR_EXIT
    kSchUnlock();
    return (err);
}
#endif /* RK_CONF_SHARED_EVENTS */
