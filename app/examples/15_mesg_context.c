/* SPDX-License-Identifier: Apache-2.0 */
/* External async-message contexts: attachment, ownership, and task lifetime.
 * Build without RK_QEMU_UNIT_TEST so copied queues really remain disabled.
 * Fault handling is disabled in this bench to inspect expected API errors.
 */
#include <kapi.h>
#include <stdio.h>
#include <string.h>

#if (RK_CONF_MESG_QUEUE != OFF)
#error "This regression must run with copied message queues disabled"
#endif

#define STACKSIZE 256U
#define CONTROL_PRIO 4U
#define WORKER_PRIO 10U
#define PLAIN_PRIO 12U
#define CEILING_PRIO 3U

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(plainHandle, PlainTask, plainStack, STACKSIZE)
RK_DECLARE_MESG_POOL(pool, poolBuf, ULONG, 1U)
RK_DECLARE_MESG_POOL(ceilingPool, ceilingPoolBuf, ULONG, 1U)

static RK_MESG_CONTEXT controlContext;
static RK_MESG_CONTEXT workerContext;
static RK_MESG_CONTEXT spareContext;
static RK_MEM_PARTITION stackPool;
static RK_STACK workerStack[STACKSIZE] K_ALIGN(8);
static RK_TASK_HANDLE workerHandle;
static volatile UINT workerMode;
static volatile UINT workerReceived;
static volatile UINT workerRelease;
static volatile UINT workerDone;
static volatile UINT plainDone;

static VOID Stop_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

static VOID Check_(RK_BOOL const condition, CHAR const *const wherePtr)
{
    if (condition == RK_FALSE)
    {
        printf("MC FAIL %s\r\n", wherePtr);
        Stop_();
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected,
                    CHAR const *const wherePtr)
{
    if (actual != expected)
    {
        printf("MC FAIL %s err=%d expected=%d\r\n", wherePtr, actual, expected);
        Stop_();
    }
}

static VOID Wait_(volatile UINT const *const flagPtr)
{
    RK_TICK const start = kTickGet();
    while (*flagPtr == 0U)
    {
        Check_((RK_BOOL)((kTickGet() - start) < RK_MS_TO_TICKS(1000)),
               "handshake timeout");
        kSleep(1U);
    }
}

static VOID WorkerTask(VOID *args)
{
    RK_UNUSEARGS
    RK_MESG *mesgPtr = NULL;
    if (workerMode == 1U)
    {
        Expect_(kMesgWait(controlHandle, &mesgPtr, RK_WAIT_FOREVER),
                RK_ERR_SUCCESS, "worker receive");
        workerReceived = 1U;
        Wait_(&workerRelease);
        Expect_(kMesgFree(mesgPtr), RK_ERR_SUCCESS, "worker free");
    }
    else
    {
        Expect_(kMesgAlloc(&pool, &mesgPtr, RK_NO_WAIT),
                RK_ERR_SUCCESS, "sync receiver async allocation");
        Expect_(kMesgSend(controlHandle, mesgPtr),
                RK_ERR_SUCCESS, "sync receiver async send");
    }
    workerDone = 1U;
    Stop_();
}

static VOID Spawn_(VOID)
{
    RK_DYNAMIC_TASK_ATTR const attr = {
        .taskFunc = WorkerTask,
        .argsPtr = RK_NO_ARGS,
        .taskName = "MCwork",
        .priority = WORKER_PRIO,
        .preempt = RK_PREEMPT,
        .stackMemPtr = &stackPool
    };
    Expect_(kTaskSpawn(&attr, &workerHandle), RK_ERR_SUCCESS, "spawn");
    Check_((RK_BOOL)(workerHandle->asynchMesgPtr == NULL), "new task detached");
}

int main(void)
{
    kCoreInit();
    kInit();
    while (1) {}
}

VOID kApplicationInit(VOID)
{
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "MCctrl",
                     controlStack, STACKSIZE, CONTROL_PRIO, RK_PREEMPT),
            RK_ERR_SUCCESS, "controller init");
    Expect_(kTaskInit(&plainHandle, PlainTask, RK_NO_ARGS, "MCplain",
                     plainStack, STACKSIZE, PLAIN_PRIO, RK_PREEMPT),
            RK_ERR_SUCCESS, "plain init");
    Expect_(kMesgPoolInit(&pool, poolBuf, sizeof(ULONG), 1U,
                         RK_MESG_PRIO_CEILING_NONE), RK_ERR_SUCCESS, "pool init");
    Expect_(kMesgPoolInit(&ceilingPool, ceilingPoolBuf, sizeof(ULONG), 1U,
                         CEILING_PRIO), RK_ERR_SUCCESS, "ceiling pool init");
    Expect_(kMemPartitionInit(&stackPool, workerStack, sizeof(workerStack), 1U),
            RK_ERR_SUCCESS, "stack pool init");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    RK_MESG *mesgPtr = NULL;
    RK_MESG *receivedPtr = NULL;

    Check_((RK_BOOL)(controlHandle->asynchMesgPtr == NULL), "initially detached");
    Expect_(kMesgAlloc(&pool, &mesgPtr, RK_NO_WAIT), RK_ERR_OBJ_NOT_INIT,
            "allocation needs context");
    Check_((RK_BOOL)((mesgPtr == NULL) && (pool.nFreeBlocks == 1U)),
           "rejected allocation leaves pool intact");
    Expect_(kMesgWait(RK_ANY_TASK, &mesgPtr, RK_NO_WAIT), RK_ERR_OBJ_NOT_INIT,
            "wait needs endpoint");
    Expect_(kMesgContextInit(controlHandle, NULL), RK_ERR_OBJ_NULL, "null storage");
    Expect_(kMesgEndpointInit(NULL, &spareContext), RK_ERR_OBJ_NULL, "null task");
    Expect_(kMesgContextInit(RK_ANY_TASK, &spareContext), RK_ERR_INVALID_PARAM,
            "wildcard task");

    memset(&controlContext, 0xa5, sizeof(controlContext));
    Expect_(kMesgContextInit(controlHandle, &controlContext), RK_ERR_SUCCESS,
            "attach uninitialized storage");
    Expect_(kMesgContextInit(controlHandle, &controlContext), RK_ERR_OBJ_DOUBLE_INIT,
            "duplicate attachment");
    Expect_(kMesgContextInit(controlHandle, &spareContext), RK_ERR_HAS_OWNER,
            "replacement rejected");
    Expect_(kMesgEndpointInit(plainHandle, &controlContext), RK_ERR_HAS_OWNER,
            "shared context rejected");
    Check_((RK_BOOL)(plainHandle->asynchMesgPtr == NULL), "plain still detached");
    Expect_(kMesgWait(RK_ANY_TASK, &mesgPtr, RK_NO_WAIT), RK_ERR_OBJ_NOT_INIT,
            "sender context cannot receive");

    Expect_(kMesgAlloc(&ceilingPool, &mesgPtr, RK_NO_WAIT), RK_ERR_SUCCESS,
            "sender owns message");
    Expect_(kMesgEndpointInit(controlHandle, &controlContext), RK_ERR_SUCCESS,
            "upgrade sender to receiver");
    Check_((RK_BOOL)((controlContext.ownedList.size == 1U) &&
                     (mesgPtr->owner == controlHandle) &&
                     (RK_TASK_PRIO(controlHandle) == CEILING_PRIO)),
           "upgrade preserves ownership and ceiling");
    Expect_(kMesgEndpointInit(controlHandle, &controlContext), RK_ERR_OBJ_DOUBLE_INIT,
            "duplicate endpoint");
    Expect_(kMesgFree(mesgPtr), RK_ERR_SUCCESS, "free after upgrade");
    Check_((RK_BOOL)((controlContext.ownedList.size == 0U) &&
                     (RK_TASK_PRIO(controlHandle) == CONTROL_PRIO)),
           "ceiling restored");
    Expect_(kSynchMesgInit(controlHandle, sizeof(ULONG)), RK_ERR_HAS_OWNER,
            "async receive excludes sync receive");
    Expect_(kMesgWait(RK_ANY_TASK, &receivedPtr, RK_MS_TO_TICKS(10)),
            RK_ERR_TIMEOUT, "external receive timeout");
    Check_((RK_BOOL)((controlContext.waiters.size == 0U) &&
                     (controlContext.waitDestPtr == NULL) &&
                     (controlContext.waitSenderPtr == NULL)), "receive wait cleared");

    /* Sender-only context and synchronous receive can be attached in either order. */
    Spawn_();
    Expect_(kMesgContextInit(workerHandle, &workerContext), RK_ERR_SUCCESS,
            "worker sender context");
    Expect_(kSynchMesgInit(workerHandle, sizeof(ULONG)), RK_ERR_SUCCESS,
            "sync endpoint after sender context");
    Expect_(kMesgEndpointInit(workerHandle, &workerContext), RK_ERR_HAS_OWNER,
            "sync receive excludes async receive");
    RK_TASK_HANDLE const oldTask = workerHandle;
    Expect_(kTaskTerminate(&workerHandle), RK_ERR_SUCCESS, "terminate empty context");
    Check_((RK_BOOL)(oldTask->asynchMesgPtr == NULL), "termination detaches storage");

    Spawn_();
    Expect_(kMesgEndpointInit(workerHandle, &workerContext), RK_ERR_SUCCESS,
            "reuse context after termination");
    workerMode = 1U;
    Expect_(kMesgAlloc(&pool, &mesgPtr, RK_NO_WAIT), RK_ERR_SUCCESS, "queued alloc");
    Expect_(kMesgSend(workerHandle, mesgPtr), RK_ERR_SUCCESS, "queued send");
    Expect_(kTaskTerminate(&workerHandle), RK_ERR_TASK_INVALID_ST,
            "queued message prevents termination");
    Wait_(&workerReceived);
    Expect_(kTaskTerminate(&workerHandle), RK_ERR_TASK_INVALID_ST,
            "received ownership prevents termination");
    workerRelease = 1U;
    Wait_(&workerDone);
    Expect_(kTaskTerminate(&workerHandle), RK_ERR_SUCCESS, "terminate after free");

    workerMode = 2U;
    workerDone = 0U;
    Spawn_();
    Expect_(kSynchMesgInit(workerHandle, sizeof(ULONG)), RK_ERR_SUCCESS,
            "sync endpoint before sender context");
    Expect_(kMesgContextInit(workerHandle, &workerContext), RK_ERR_SUCCESS,
            "sender context after sync endpoint");
    Wait_(&workerDone);
    Expect_(kTaskTerminate(&workerHandle), RK_ERR_TASK_INVALID_ST,
            "queued sender reference prevents termination");
    Expect_(kMesgWait(workerHandle, &receivedPtr, RK_NO_WAIT), RK_ERR_SUCCESS,
            "filtered receive from sync task");
    Expect_(kMesgFree(receivedPtr), RK_ERR_SUCCESS, "free sync task message");
    Expect_(kTaskTerminate(&workerHandle), RK_ERR_SUCCESS, "terminate former sender");
    Wait_(&plainDone);
    printf("MC PASS external message context lifecycle\r\n");
    Stop_();
}

VOID PlainTask(VOID *args)
{
    RK_UNUSEARGS
    Check_((RK_BOOL)(plainHandle->asynchMesgPtr == NULL), "nonparticipant has no context");
    plainDone = 1U;
    Stop_();
}
