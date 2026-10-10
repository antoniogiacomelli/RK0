/* SPDX-License-Identifier: Apache-2.0 */
/* Regression coverage for waiter selection and lost pure signals. */

#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U
#define OP_SEMAPHORE 1U
#define OP_SLEEP_QUEUE 2U
#define OP_CONDVAR 3U
#define OP_HOLD_READY 4U
#define OP_SELF_SUSPEND 5U
#define INVALID_ORDER ((RK_OPTION)99U)
#define FINITE_WAIT RK_MS_TO_TICKS(40UL)

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(firstHandle, WorkerTask, firstStack, STACKSIZE)
RK_DECLARE_TASK(secondHandle, WorkerTask, secondStack, STACKSIZE)

typedef struct
{
    UINT request;
    UINT done;
    UINT readyStarted;
    UINT operation;
    VOID *objectPtr;
    RK_TICK timeout;
    RK_ERR result;
    RK_BOOL holdReady;
} WORK;

static volatile WORK work[2];
static UINT workerIds[2] = {0U, 1U};
static RK_SEMAPHORE semaphores[4];
static RK_SLEEP_QUEUE queues[2];
static RK_SLEEP_QUEUE condition;
static RK_MUTEX conditionLock;
static volatile UINT resumeCount;
static volatile UINT resumeOrder[2];

#if defined(RK_QEMU_UNIT_TEST)
volatile unsigned RK_gQemuTestForceIsr = 0U;
#endif

static VOID Stop_(VOID)
{
    while (1)
        (void)kSleep(RK_MS_TO_TICKS(1000UL));
}

static VOID Check_(RK_BOOL const ok, CHAR const *const wherePtr)
{
    if (ok == RK_FALSE)
    {
        printf("WO FAIL %s\r\n", wherePtr);
        Stop_();
    }
}

static VOID Require_(RK_ERR const err)
{
    Check_((RK_BOOL)(err == RK_ERR_SUCCESS), "service result");
}

static VOID Start_(UINT const id, UINT const operation, VOID *const objectPtr,
                    RK_TICK const timeout)
{
    work[id].operation = operation;
    work[id].objectPtr = objectPtr;
    work[id].timeout = timeout;
    work[id].holdReady = RK_TRUE;
    work[id].request += 1U;
}

static VOID WaitDone_(UINT const id, RK_ERR const expected)
{
    for (UINT i = 0U; i < 500U; i++)
    {
        if (work[id].done == work[id].request)
        {
            Check_((RK_BOOL)(work[id].result == expected), "wait result");
            return;
        }
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "worker did not complete");
}

static VOID WaitReady_(UINT const id)
{
    for (UINT i = 0U; i < 500U; i++)
    {
        if (work[id].readyStarted == work[id].request)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "worker did not become ready");
}

static VOID WaitSize_(RK_TCBQ const *const queuePtr, ULONG const size)
{
    for (UINT i = 0U; i < 500U; i++)
    {
        RK_CR_AREA
        RK_CR_ENTER
        RK_BOOL const matches = (RK_BOOL)(queuePtr->size == size);
        RK_CR_EXIT
        if (matches == RK_TRUE)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "queue size");
}

static VOID WaitSuspended_(RK_TASK_HANDLE const taskHandle)
{
    for (UINT i = 0U; i < 500U; i++)
    {
        RK_CR_AREA
        RK_CR_ENTER
        RK_BOOL const suspended =
            (RK_BOOL)(taskHandle->status == RK_SELF_SUSPENDED);
        RK_CR_EXIT
        if (suspended == RK_TRUE)
            return;
        (void)kSleep(1UL);
    }
    Check_(RK_FALSE, "worker did not suspend");
}

static VOID ExpectOrder_(RK_TCBQ *const queuePtr, UINT const firstId)
{
    RK_CR_AREA
    RK_CR_ENTER
    RK_NODE *const firstNode = queuePtr->listDummy.nextPtr;
    RK_TASK_HANDLE const head = K_GET_TCB_ADDR(firstNode);
    RK_TASK_HANDLE const tail = K_GET_TCB_ADDR(firstNode->nextPtr);
    RK_TASK_HANDLE const expectedHead =
        (firstId == 0U) ? firstHandle : secondHandle;
    RK_TASK_HANDLE const expectedTail =
        (firstId == 0U) ? secondHandle : firstHandle;
    RK_BOOL const matches = (RK_BOOL)((queuePtr->size == 2UL) &&
                                      (head == expectedHead) &&
                                      (tail == expectedTail));
    RK_CR_EXIT
    Check_(matches, "waiter order");
}

static VOID StartPair_(UINT const operation, VOID *const objectPtr,
                        RK_TCBQ *const queuePtr, RK_TICK const firstTimeout)
{
    resumeCount = 0U;
    Start_(0U, operation, objectPtr, firstTimeout);
    WaitSize_(queuePtr, 1UL);
    Start_(1U, operation, objectPtr, RK_WAIT_FOREVER);
    WaitSize_(queuePtr, 2UL);
}

static VOID SemaCase_(RK_SEMAPHORE *const semaPtr)
{
    UINT const firstId = (semaPtr->waitOrder == RK_WAIT_FIFO) ? 0U : 1U;
    StartPair_(OP_SEMAPHORE, semaPtr, &semaPtr->waitingQueue, RK_WAIT_FOREVER);
    ExpectOrder_(&semaPtr->waitingQueue, firstId);
    Require_(kSemaphorePost(semaPtr));
    WaitDone_(firstId, RK_ERR_SUCCESS);
    Check_((RK_BOOL)(work[1U - firstId].done != work[1U - firstId].request),
           "post released wrong waiter");
    Require_(kSemaphorePost(semaPtr));
    WaitDone_(1U - firstId, RK_ERR_SUCCESS);
    Check_((RK_BOOL)((semaPtr->waitingQueue.size == 0UL) &&
                     (semaPtr->value == 0U)), "permit handoff");

    StartPair_(OP_SEMAPHORE, semaPtr, &semaPtr->waitingQueue, FINITE_WAIT);
    WaitDone_(0U, RK_ERR_TIMEOUT);
    Check_((RK_BOOL)((semaPtr->waitingQueue.size == 1UL) &&
                     (firstHandle->waitingQueuePtr == NULL)),
           "semaphore timeout removal");
    Require_(kSemaphorePost(semaPtr));
    WaitDone_(1U, RK_ERR_SUCCESS);
}

static VOID SleepCases_(RK_SLEEP_QUEUE *const queuePtr)
{
    Check_((RK_BOOL)(queuePtr->lostSignals == 0UL), "sleep loss initialisation");
    Check_((RK_BOOL)((kSleepQueueSignal(queuePtr) == RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (queuePtr->lostSignals == 1UL)), "empty signal loss");
    UINT remaining = 99U;
    Check_((RK_BOOL)((kSleepQueueWake(queuePtr, 1U, &remaining) ==
                      RK_ERR_EMPTY_WAITING_QUEUE) && (remaining == 0U) &&
                     (queuePtr->lostSignals == 2UL)), "empty limited wake loss");
    Check_((RK_BOOL)((kSleepQueueFlush(queuePtr) == RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (queuePtr->lostSignals == 3UL)), "empty broadcast loss");
    Check_((RK_BOOL)((kSleepQueueReady(queuePtr, firstHandle) ==
                      RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (queuePtr->lostSignals == 4UL)), "empty named wake loss");
#if defined(RK_QEMU_UNIT_TEST)
    RK_gQemuTestForceIsr = 1U;
    RK_ERR const signalErr = kSleepQueueSignal(queuePtr);
    RK_ERR const broadcastErr = kSleepQueueFlush(queuePtr);
    RK_gQemuTestForceIsr = 0U;
    Check_((RK_BOOL)((signalErr == RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (broadcastErr == RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (queuePtr->lostSignals == 6UL)), "empty ISR wake loss");
#endif
    ULONG const lostSignals = queuePtr->lostSignals;
    Check_((RK_BOOL)(kSleepQueueSleep(queuePtr, RK_NO_WAIT) == RK_ERR_NOWAIT),
           "lost signals do not become tokens");

    UINT const firstId = (queuePtr->waitOrder == RK_WAIT_FIFO) ? 0U : 1U;
    StartPair_(OP_SLEEP_QUEUE, queuePtr, &queuePtr->waitingQueue,
               RK_WAIT_FOREVER);
    ExpectOrder_(&queuePtr->waitingQueue, firstId);

    /* A queued task's priority can change through another protocol. */
    Check_(kTaskRaiseEffectivePrio(secondHandle, 3U), "raise priority");
    ExpectOrder_(&queuePtr->waitingQueue, firstId);
    Check_(kTaskUpdateEffectivePrio(secondHandle), "restore priority");
    ExpectOrder_(&queuePtr->waitingQueue, firstId);
    Check_(kTaskRaiseEffectivePrio(firstHandle, 3U), "raise first priority");
    ExpectOrder_(&queuePtr->waitingQueue, 0U);
    Check_(kTaskUpdateEffectivePrio(firstHandle), "restore first priority");
    ExpectOrder_(&queuePtr->waitingQueue, firstId);
    Check_(kTaskRaiseEffectivePrio(firstHandle, 5U), "equal priorities");
    ExpectOrder_(&queuePtr->waitingQueue, firstId);
    Check_(kTaskUpdateEffectivePrio(firstHandle), "restore equal priorities");
    ExpectOrder_(&queuePtr->waitingQueue, firstId);

    Require_(kSleepQueueSignal(queuePtr));
    WaitDone_(firstId, RK_ERR_SUCCESS);
    Require_(kSleepQueueSignal(queuePtr));
    WaitDone_(1U - firstId, RK_ERR_SUCCESS);

    StartPair_(OP_SLEEP_QUEUE, queuePtr, &queuePtr->waitingQueue,
               RK_WAIT_FOREVER);
    remaining = 99U;
    Require_(kSleepQueueWake(queuePtr, 1U, &remaining));
    Check_((RK_BOOL)(remaining == 1U), "limited wake remaining count");
    WaitDone_(firstId, RK_ERR_SUCCESS);
    Require_(kSleepQueueFlush(queuePtr));
    WaitDone_(1U - firstId, RK_ERR_SUCCESS);

    StartPair_(OP_SLEEP_QUEUE, queuePtr, &queuePtr->waitingQueue,
               RK_WAIT_FOREVER);
    Require_(kSleepQueueWake(queuePtr, 0U, &remaining));
    WaitDone_(0U, RK_ERR_SUCCESS);
    WaitDone_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)((remaining == 0U) && (resumeCount == 2U) &&
                     (resumeOrder[0] == 1U) && (resumeOrder[1] == 0U)),
           "broadcast execution follows priority");

    StartPair_(OP_SLEEP_QUEUE, queuePtr, &queuePtr->waitingQueue, FINITE_WAIT);
    WaitDone_(0U, RK_ERR_TIMEOUT);
    Check_((RK_BOOL)((queuePtr->waitingQueue.size == 1UL) &&
                     (firstHandle->waitingQueuePtr == NULL) &&
                     (secondHandle->waitingQueuePtr == &queuePtr->waitingQueue)),
           "sleep timeout removal");
    Require_(kSleepQueueSignal(queuePtr));
    WaitDone_(1U, RK_ERR_SUCCESS);

    /* Unready() must use the same policy as Sleep(). */
    for (UINT id = 0U; id < 2U; id++)
    {
        RK_TASK_HANDLE const taskHandle = (id == 0U) ? firstHandle : secondHandle;
        Start_(id, OP_HOLD_READY, NULL, RK_WAIT_FOREVER);
        WaitReady_(id);
        Require_(kSleepQueueUnready(queuePtr, taskHandle));
        work[id].holdReady = RK_FALSE;
    }
    ExpectOrder_(&queuePtr->waitingQueue, firstId);
    Require_(kSleepQueueReady(queuePtr, firstHandle));
    Require_(kSleepQueueFlush(queuePtr));
    WaitSize_(&queuePtr->waitingQueue, 0UL);
    WaitDone_(0U, RK_ERR_SUCCESS);
    WaitDone_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)(queuePtr->lostSignals == lostSignals),
           "successful wakes preserve loss count");
}

static VOID ResumeCases_(VOID)
{
    Check_((RK_BOOL)(RK_ERR_TASK_NOT_SUSPENDED > 0), "resume loss is positive");
    Check_((RK_BOOL)((controlHandle->lostSignals == 0UL) &&
                     (firstHandle->lostSignals == 0UL) &&
                     (secondHandle->lostSignals == 0UL)),
           "task loss initialisation");
#if (RK_CONF_ERR_CHECK == ON)
    RK_TCB invalidTask = {0};
    invalidTask.lostSignals = 17UL;
    Check_((RK_BOOL)(kTaskResume(NULL) == RK_ERR_OBJ_NULL), "null resume");
    Check_((RK_BOOL)((kTaskResume(&invalidTask) == RK_ERR_INVALID_OBJ) &&
                     (invalidTask.lostSignals == 17UL)),
           "invalid tasks do not count loss");
#endif
    Check_((RK_BOOL)((kTaskResume(controlHandle) == RK_ERR_TASK_NOT_SUSPENDED) &&
                     (controlHandle->status == RK_RUNNING) &&
                     (controlHandle->lostSignals == 1UL)), "running resume loss");

    Start_(0U, OP_SLEEP_QUEUE, &queues[0], FINITE_WAIT);
    WaitSize_(&queues[0].waitingQueue, 1UL);
    Check_((RK_BOOL)((kTaskResume(firstHandle) == RK_ERR_TASK_NOT_SUSPENDED) &&
                     (firstHandle->status == RK_SLEEPING) &&
                     (firstHandle->waitingQueuePtr == &queues[0].waitingQueue) &&
                     (firstHandle->timeoutNode.timeoutType == RK_TIMEOUT_BLOCKING) &&
                     (queues[0].waitingQueue.size == 1UL) &&
                     (firstHandle->lostSignals == 1UL)), "sleeping resume loss");
    Require_(kSleepQueueSignal(&queues[0]));
    WaitDone_(0U, RK_ERR_SUCCESS);

    Start_(0U, OP_HOLD_READY, NULL, RK_WAIT_FOREVER);
    WaitReady_(0U);
    Check_((RK_BOOL)((kTaskResume(firstHandle) == RK_ERR_TASK_NOT_SUSPENDED) &&
                     (firstHandle->status == RK_READY) &&
                     (firstHandle->lostSignals == 2UL)), "ready resume loss");
    work[0].holdReady = RK_FALSE;
    WaitDone_(0U, RK_ERR_SUCCESS);

    Start_(0U, OP_SELF_SUSPEND, NULL, RK_WAIT_FOREVER);
    WaitSuspended_(firstHandle);
    Require_(kTaskResume(firstHandle));
    Check_((RK_BOOL)((firstHandle->status == RK_READY) &&
                     (firstHandle->lostSignals == 2UL)), "successful resume");
    Check_((RK_BOOL)((kTaskResume(firstHandle) == RK_ERR_TASK_NOT_SUSPENDED) &&
                     (firstHandle->status == RK_READY) &&
                     (firstHandle->lostSignals == 3UL)), "duplicate resume loss");
#if defined(RK_QEMU_UNIT_TEST)
    RK_gQemuTestForceIsr = 1U;
    RK_ERR const resumeErr = kTaskResume(firstHandle);
    RK_ERR const runningErr = kTaskResume(controlHandle);
    RK_gQemuTestForceIsr = 0U;
    Check_((RK_BOOL)((resumeErr == RK_ERR_TASK_NOT_SUSPENDED) &&
                     (runningErr == RK_ERR_TASK_NOT_SUSPENDED) &&
                     (firstHandle->status == RK_READY) &&
                     (controlHandle->status == RK_RUNNING) &&
                     (firstHandle->lostSignals == 4UL) &&
                     (controlHandle->lostSignals == 2UL)), "ISR resume loss");
#endif
    WaitDone_(0U, RK_ERR_SUCCESS);
#if (RK_CONF_TRACE == ON)
    static RK_TRACE_TASK_INFO taskInfo[RK_NTHREADS];
    UINT const count = kTraceTaskSnapshot(taskInfo, RK_NTHREADS);
    RK_BOOL found = RK_FALSE;
    for (UINT i = 0U; i < count; i++)
    {
        if (taskInfo[i].taskHandle == firstHandle)
        {
            found = RK_TRUE;
            Check_((RK_BOOL)(taskInfo[i].lostSignals == firstHandle->lostSignals),
                   "trace resume loss snapshot");
        }
    }
    Check_(found, "trace task snapshot");
#endif
}

static VOID DynamicCases_(VOID)
{
    RK_SEMAPHORE_HANDLE semaPtr = NULL;
    RK_SLEEP_QUEUE_HANDLE queuePtr = NULL;
    for (UINT i = 0U; i < 6U; i++)
    {
        Check_((RK_BOOL)((kSemaphoreCreate(&semaPtr, 0U, 1U,
                                          INVALID_ORDER) ==
                          RK_ERR_INVALID_PARAM) && (semaPtr == NULL)),
               "invalid dynamic semaphore order");
        Check_((RK_BOOL)((kSleepQueueCreate(&queuePtr, INVALID_ORDER) ==
                          RK_ERR_INVALID_PARAM) && (queuePtr == NULL)),
               "invalid dynamic sleep order");
    }
    Require_(kSemaphoreCreate(&semaPtr, 0U, 1U, RK_WAIT_FIFO));
    SemaCase_(semaPtr);
    Require_(kSemaphoreDestroy(&semaPtr));
    Require_(kSemaphoreCreate(&semaPtr, 0U, 2U, RK_WAIT_PRIORITY));
    Check_((RK_BOOL)(semaPtr->waitOrder == RK_WAIT_PRIORITY), "dynamic priority");
    Require_(kSemaphoreDestroy(&semaPtr));
    Require_(kSleepQueueCreate(&queuePtr, RK_WAIT_FIFO));
    SleepCases_(queuePtr);
    Require_(kSleepQueueDestroy(&queuePtr));
    Require_(kSleepQueueCreate(&queuePtr, RK_WAIT_PRIORITY));
    Check_((RK_BOOL)((queuePtr->waitOrder == RK_WAIT_PRIORITY) &&
                     (queuePtr->lostSignals == 0UL)), "dynamic loss reset");
    Require_(kSleepQueueDestroy(&queuePtr));
}

int main(void)
{
    kCoreInit();
    kInit();
    while (1) { }
}

VOID kApplicationInit(VOID)
{
    /* Creation failures are reported after scheduling starts. */
    (void)kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "WOctrl",
                    controlStack, STACKSIZE, 2U, RK_PREEMPT);
    (void)kTaskInit(&firstHandle, WorkerTask, &workerIds[0], "WOfirst",
                    firstStack, STACKSIZE, 10U, RK_PREEMPT);
    (void)kTaskInit(&secondHandle, WorkerTask, &workerIds[1], "WOsecond",
                    secondStack, STACKSIZE, 5U, RK_PREEMPT);
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    printf("WO start\r\n");
    Require_(kSemaBinInit(&semaphores[0], 0U, RK_WAIT_PRIORITY));
    Require_(kSemaCountInit(&semaphores[1], 0U, RK_WAIT_PRIORITY));
    Require_(kSemaBinInit(&semaphores[2], 0U, RK_WAIT_FIFO));
    Require_(kSemaCountInit(&semaphores[3], 0U, RK_WAIT_FIFO));
    Require_(kSleepQueueInit(&queues[0], RK_WAIT_PRIORITY));
    Require_(kSleepQueueInit(&queues[1], RK_WAIT_FIFO));
    Require_(kCondVarInit(&condition, &conditionLock, RK_WAIT_FIFO));
    Require_(kObjPartitionsInit());
    printf("WO resume loss\r\n");
    ResumeCases_();

    RK_SEMAPHORE invalidSema = {0};
    RK_SLEEP_QUEUE invalidQueue = {0};
    Check_((RK_BOOL)((kSemaphoreInit(&invalidSema, 0U, 1U,
                                    INVALID_ORDER) ==
                      RK_ERR_INVALID_PARAM) && (invalidSema.init == RK_FALSE)),
           "invalid semaphore order");
    Check_((RK_BOOL)((kSleepQueueInit(&invalidQueue, INVALID_ORDER) ==
                      RK_ERR_INVALID_PARAM) && (invalidQueue.init == RK_FALSE)),
           "invalid sleep order");
    for (UINT i = 0U; i < 4U; i++)
    {
        printf("WO semaphore=%u\r\n", i);
        SemaCase_(&semaphores[i]);
    }
    for (UINT i = 0U; i < 2U; i++)
    {
        printf("WO sleep queue=%u\r\n", i);
        SleepCases_(&queues[i]);
    }

    Check_((RK_BOOL)(condition.lostSignals == 0UL), "condition loss initialisation");
    Check_((RK_BOOL)((kCondVarSignal(&condition) == RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (condition.lostSignals == 1UL)), "condition signal loss");
    Check_((RK_BOOL)((kCondVarBroadcast(&condition) == RK_ERR_EMPTY_WAITING_QUEUE) &&
                     (condition.lostSignals == 2UL)), "condition broadcast loss");
    StartPair_(OP_CONDVAR, &condition, &condition.waitingQueue, RK_WAIT_FOREVER);
    ExpectOrder_(&condition.waitingQueue, 0U);
    Require_(kCondVarBroadcast(&condition));
    WaitDone_(0U, RK_ERR_SUCCESS);
    WaitDone_(1U, RK_ERR_SUCCESS);
    Check_((RK_BOOL)(condition.lostSignals == 2UL), "condition loss preserved");
    printf("WO dynamic objects\r\n");
    DynamicCases_();
    printf("WO PASS semaphore and sleep queue order\r\n");
    Stop_();
}

VOID WorkerTask(VOID *args)
{
    UINT const id = *(UINT *)args;
    UINT seen = 0U;
    while (1)
    {
        while (work[id].request == seen)
            (void)kSleep(1UL);
        seen = work[id].request;
        UINT const operation = work[id].operation;
        VOID *const objectPtr = work[id].objectPtr;
        RK_ERR err;
        if (operation == OP_HOLD_READY)
        {
            work[id].readyStarted = seen;
            while (work[id].holdReady == RK_TRUE)
                kYield();
            work[id].result = RK_ERR_SUCCESS;
            work[id].done = seen;
            continue;
        }
        if (operation == OP_SEMAPHORE)
            err = kSemaphorePend((RK_SEMAPHORE *)objectPtr, work[id].timeout);
        else if (operation == OP_SELF_SUSPEND)
            err = kTaskSelfSuspend();
        else if (operation == OP_CONDVAR)
        {
            Require_(kMutexLock(&conditionLock, RK_WAIT_FOREVER));
            err = kCondVarWait((RK_SLEEP_QUEUE *)objectPtr, &conditionLock,
                               work[id].timeout);
            Require_(kMutexUnlock(&conditionLock));
        }
        else
            err = kSleepQueueSleep((RK_SLEEP_QUEUE *)objectPtr, work[id].timeout);
        if (resumeCount < 2U)
            resumeOrder[resumeCount++] = id;
        work[id].result = err;
        work[id].done = seen;
    }
}
