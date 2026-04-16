/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.18.1                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/* This file demonstrates synchronisation barriers using shared-state */
/* and message-passing paradigms. */

/* Set to 0 to use message-passing version, 1 for shared-memory version */
#define SYNCHBARR_MESGPASS_APP 1

#include <kapi.h>
/* Configure the application logger facility here */
#include <logger.h>
#include <qemu_uart.h>
#include <stdio.h>
int main(void)
{

    kCoreInit();

    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

#define LOG_BARRIER_ENTER(c, t, name)                                          \
    logPost("[BARRIER: %u/%u]: %s ENTERED  ", (c), (t), (name))
#define LOG_BARRIER_BLOCK(c, t, name)                                          \
    logPost("[BARRIER: %u/%u]: %s BLOCKED  ", (c), (t), (name))
#define LOG_BARRIER_WAKE(c, t, name)                                           \
    logPost("[BARRIER: %u/%u]: %s WAKING ALL TASKS ", (c), (t), (name))

#if (SYNCHBARR_MESGPASS_APP == 0)
/*** SYNCH BARRIER USING PROCEDURE CALL CHANNELS ***/

#define LOG_PRIORITY 4
#define STACKSIZE 256
#define BARRIER_TASK_COUNT 3U
#define BARRIER_CHANNEL_DEPTH 3
#define BARRIER_RELEASE_CODE 1U

typedef struct
{
    UINT releaseCode;
} BarrierResp;

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(barrierHandle, BarrierServer, stackB, STACKSIZE)

static RK_CHANNEL barrierChannel;
RK_DECLARE_CHANNEL_BUF(barrierBuf, BARRIER_CHANNEL_DEPTH)
static RK_MEM_PARTITION barrierReqPart;
static RK_REQ_BUF barrierReqPool[BARRIER_TASK_COUNT] K_ALIGN(4);

static inline VOID BarrierWaitChannel(RK_TICK timeout)
{
    BarrierResp resp = {0U};
    RK_REQ_BUF *reqBuf = (RK_REQ_BUF *)kMemPartitionAlloc(&barrierReqPart);
    K_ASSERT(reqBuf != NULL);

    reqBuf->size = 0U;
    reqBuf->reqPtr = NULL;
    reqBuf->respPtr = &resp;

    RK_ERR err = kChannelCall(barrierHandle, reqBuf, timeout);
    if (err != RK_ERR_SUCCESS)
    {

        if (err == RK_ERR_TIMEOUT)
        {
            logError("%s TIMEOUT", RK_RUNNING_NAME);
        }
        else
        {
            logError("%s CALL ERROR %d", RK_RUNNING_NAME, err);
        }
    }

    K_ASSERT(resp.releaseCode == BARRIER_RELEASE_CODE);
}

static VOID BarrierReplyWaiters(RK_REQ_BUF *const *const waiters,
                                UINT const nWaiters)
{
    for (UINT i = 0U; i < nWaiters; ++i)
    {
        RK_REQ_BUF *reqBuf = waiters[i];
        BarrierResp *respPtr = (BarrierResp *)reqBuf->respPtr;
        K_ASSERT(respPtr != NULL);
        respPtr->releaseCode = BARRIER_RELEASE_CODE;

        RK_ERR err = kChannelDone(reqBuf);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

VOID BarrierServer(VOID *args)
{
    RK_UNUSEARGS

    /* Only callers from the current round are stored while they wait. */
    RK_REQ_BUF *waiters[BARRIER_TASK_COUNT - 1U];
    UINT waitingCount = 0U;
    static CHAR name[RK_OBJ_MAX_NAME_LEN] = {0};

    while (1)
    {
        RK_REQ_BUF *reqBuf = NULL;
        RK_ERR err = kChannelAccept(&barrierChannel, &reqBuf, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        const UINT arrived = waitingCount + 1U;
        kTaskGetName(reqBuf->sender, name);
        LOG_BARRIER_ENTER(arrived, BARRIER_TASK_COUNT, name);

        if (arrived == BARRIER_TASK_COUNT)
        {
            LOG_BARRIER_WAKE(arrived, BARRIER_TASK_COUNT, name);
            BarrierReplyWaiters(waiters, waitingCount);
            BarrierResp *respPtr = (BarrierResp *)reqBuf->respPtr;
            K_ASSERT(respPtr != NULL);
            respPtr->releaseCode = BARRIER_RELEASE_CODE;
            err = kChannelDone(reqBuf);
            K_ASSERT(err == RK_ERR_SUCCESS);
            waitingCount = 0U;
        }
        else
        {
            LOG_BARRIER_BLOCK(arrived, BARRIER_TASK_COUNT, name);
            K_ASSERT(waitingCount < (BARRIER_TASK_COUNT - 1U));
            waiters[waitingCount++] = reqBuf;
        }
    }
}

VOID kApplicationInit(VOID)
{
    /* Server adopts client priority while servicing synchronous requests. */
    RK_ERR err = kCreateTask(&barrierHandle, BarrierServer, RK_NO_ARGS,
                             "Barrier", stackB, STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kMemPartitionInit(&barrierReqPart, barrierReqPool, sizeof(RK_REQ_BUF),
                            BARRIER_TASK_COUNT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kChannelInit(&barrierChannel, barrierBuf, BARRIER_CHANNEL_DEPTH,
                       barrierHandle, &barrierReqPart);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1,
                      STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2,
                      STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3,
                      STACKSIZE, 3, RK_PREEMPT);

    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 running");
        kBusyDelay(RK_MS_TO_TICKS(100)); /* simulate work */
        BarrierWaitChannel(RK_MS_TO_TICKS(600));
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 running");
        kBusyDelay(RK_MS_TO_TICKS(200)  );
        BarrierWaitChannel(RK_MS_TO_TICKS(400));
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 running");
        kBusyDelay(RK_MS_TO_TICKS(300));
        BarrierWaitChannel(RK_MS_TO_TICKS(100));
        kSleep(RK_MS_TO_TICKS(50));
    }
}

#else

/*** SYNCH BARRIER USING MONITORS ***/


#define STACKSIZE 256

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)

/* Synchronisation Barrier */

typedef struct
{
    RK_MUTEX lock;
    RK_SLEEP_QUEUE cond;
    UINT count; /* number of tasks in the barrier */
    UINT round; /* increased every time all tasks synch     */
} Barrier_t;

VOID BarrierInit(Barrier_t *const barPtr)
{
    kMutexInit(&barPtr->lock, RK_INHERIT);
    kSleepQueueInit(&barPtr->cond);
    barPtr->count = 0;
    barPtr->round = 0;
}

VOID BarrierWait(Barrier_t *const barPtr, UINT const nTasks, RK_TICK timeout)
{
    UINT myRound = 0;
    kMutexLock(&barPtr->lock, RK_WAIT_FOREVER);

    /* save round number */
    myRound = barPtr->round;
    /* increase count on this round */
    barPtr->count++;
    LOG_BARRIER_ENTER(barPtr->count, nTasks, RK_RUNNING_NAME);

    if (barPtr->count == nTasks)
    {
        LOG_BARRIER_WAKE(barPtr->count, nTasks, RK_RUNNING_NAME);

        /* reset counter, inc round, broadcast to sleeping tasks */
        barPtr->round++;
        barPtr->count = 0;
        kCondVarBroadcast(&barPtr->cond);
    }
    else
    {
        LOG_BARRIER_BLOCK(barPtr->count, nTasks, RK_RUNNING_NAME);
        /* a proper wake signal might happen after inc round */
        while ((UINT)(barPtr->round - myRound) == 0U)
        {
            RK_ERR err = kCondVarWait(&barPtr->cond, &barPtr->lock, timeout);
            if (err != RK_ERR_SUCCESS)
            {
                logError("Wait failed with error code %d", err);
            }
        }
    }
    kMutexUnlock(&barPtr->lock);
}

#define N_BARR_TASKS 3

Barrier_t syncBarrier;

/* Note expressions within K_ASSERT brackets are supressed if compiling
with NDEBUG */
VOID kApplicationInit(VOID)
{
    RK_ERR err = kTaskInit(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1,
                             STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2,
                      STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kTaskInit(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3,
                      STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    BarrierInit(&syncBarrier);

    logInit(3); /* same as task 3 */
}
VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 running");
        kBusyDelay(RK_MS_TO_TICKS(100)); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS, RK_MS_TO_TICKS(600));
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 running");
        kBusyDelay(RK_MS_TO_TICKS(200)); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS, RK_MS_TO_TICKS(400));
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 running");
        kBusyDelay(RK_MS_TO_TICKS(300)); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS, RK_MS_TO_TICKS(100));
        kSleep(RK_MS_TO_TICKS(50)); /* let logger runs */
    }
}

#endif
