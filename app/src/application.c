/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: 0.13.3                                                           */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/* This file demonstrates synchronisation barriers using both shared-states */
/* and message-passing paradigms. */

/* Set to 1 to use message-passing version, 0 for shared-memory version */
#define SYNCHBARR_MESGPASS_APP 1


#include <kapi.h>
/* Configure the application logger faciclity here */
#include <logger.h>
#include <qemu_uart.h>

#define LOG_BARRIER_ENTER(c, t, name) logPost("[BARRIER: %u/%u]: %s ENTERED  ", (c), (t), (name))
#define LOG_BARRIER_BLOCK(c, t, name) logPost("[BARRIER: %u/%u]: %s BLOCKED  ", (c), (t), (name))
#define LOG_BARRIER_WAKE(c, t, name)  logPost("[BARRIER: %u/%u]: %s WAKING ALL TASKS ", (c), (t), (name))
int main(void)
{

    kCoreInit();

    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

#if (SYNCHBARR_MESGPASS_APP == 1)
/*** SYNCH BARRIER USING MESSSAGE PORTS ***/
/* 
In kconfig.h set:
RK_CONF_N_USRTASKS 5
*/

#define LOG_PRIORITY 5
#define STACKSIZE 256

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(barrierHandle, BarrierServer, stackB, STACKSIZE)

#define N_BARR_TASKS 3
#define PORT_CAPACITY 3U
#define PORT_MESG_WORDS RK_PORT_META_WORDS
static RK_PORT barrierPort;
RK_DECLARE_PORT_BUF(barrierBuf, RK_PORT_MESG_0WORD, PORT_CAPACITY)
static RK_MAILBOX task1ReplyBox;
static RK_MAILBOX task2ReplyBox;
static RK_MAILBOX task3ReplyBox;

static inline VOID BarrierWaitPort(RK_MAILBOX *const replyBox)
{
    RK_PORT_MESG_0WORD call = {0};
    UINT ack = 0;
    /* send and pend for reply from server */
    RK_ERR err = kPortSendRecv(&barrierPort, (ULONG *)&call, replyBox, &ack,
                               RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    K_UNUSE(ack); /* reply code unused (presence is the sync) */
}

VOID BarrierServer(VOID *args)
{
    RK_UNUSEARGS

    /* Store meta of blocked callers so we can reply later */
    RK_PORT_MESG_0WORD waiters[N_BARR_TASKS];
    UINT arrived = 0;

    while (1)
    {
        RK_PORT_MESG_0WORD mesg; /* meta-only message from a caller */
        RK_ERR err = kPortRecv(&barrierPort, &mesg, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        const CHAR *name = kTaskGetNamePtr(mesg.meta.senderHandle);
        const UINT curr = arrived + 1U;
        LOG_BARRIER_ENTER(curr, N_BARR_TASKS, name);

        if (arrived + 1U == N_BARR_TASKS)
        {
            LOG_BARRIER_WAKE(curr, N_BARR_TASKS, name);
            /*  reply to all previous waiters ... */
            for (UINT i = 0; i < arrived; ++i)
            {
                err = kPortReply(&barrierPort, (ULONG const *)&waiters[i], 1U);
                K_ASSERT(err == RK_ERR_SUCCESS);
            }

            /* ... and reply to the last one, ending the server transaction */
            err = kPortReplyDone(&barrierPort, (ULONG const *)&mesg, 1U);
            K_ASSERT(err == RK_ERR_SUCCESS);

            arrived = 0;
        }
        else
        {
            LOG_BARRIER_BLOCK(curr, N_BARR_TASKS, name);
            waiters[arrived++] = mesg; /* keep caller meta for later reply */
        }
    }
}

VOID kApplicationInit(VOID)
{
    /*  server adopts callers during service... no mutex needed for prio inheritance */
    RK_ERR err = kCreateTask(&barrierHandle, BarrierServer, RK_NO_ARGS,
                             "Barrier", stackB, STACKSIZE, 4, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    /* create a server : port + owner */
    err = kPortInit(&barrierPort, barrierBuf, PORT_MESG_WORDS, PORT_CAPACITY,
                    barrierHandle);

    K_ASSERT(err == RK_ERR_SUCCESS);

    /* clients */
    err = kCreateTask(&task1Handle, Task1, RK_NO_ARGS,
                      "Task1", stack1, STACKSIZE, 1, RK_PREEMPT);

    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task2Handle, Task2, RK_NO_ARGS,
                      "Task2", stack2, STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task3Handle, Task3, RK_NO_ARGS,
                      "Task3", stack3, STACKSIZE, 3, RK_PREEMPT);

    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kMailboxInit(&task1ReplyBox);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMailboxInit(&task2ReplyBox);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMailboxInit(&task3ReplyBox);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kMailboxSetOwner(&task1ReplyBox, task1Handle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMailboxSetOwner(&task2ReplyBox, task2Handle);
    K_ASSERT(err == RK_ERR_SUCCESS);
    err = kMailboxSetOwner(&task3ReplyBox, task3Handle);
    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
}

VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 running");
        kBusyDelay(10);
        BarrierWaitPort(&task1ReplyBox);
        kSleep(5);
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 running");
        kBusyDelay(20);
        BarrierWaitPort(&task2ReplyBox);
        kSleep(5);
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 running");
        kBusyDelay(30);
        BarrierWaitPort(&task3ReplyBox);
        kSleep(5);
    }
}

#else 

/*** SYNCH BARRIER USING MONITORS ***/

/* 
in kconfig.h set:
RK_CONF_N_USRTASKS  4  
*/

/* set the logger priority to the lowest priority amongst user tasks */
#define LOG_PRIORITY 4

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

VOID BarrierWait(Barrier_t *const barPtr, UINT const nTasks)
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
            kCondVarWait(&barPtr->cond, &barPtr->lock, RK_WAIT_FOREVER);
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

    RK_ERR err = kCreateTask(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1, STACKSIZE, 1, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2, STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kCreateTask(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3, STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);


    BarrierInit(&syncBarrier);

    logInit(LOG_PRIORITY);
}
VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 running");
        kBusyDelay(10); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        kSleep(5); /* suspend so other task can run */
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 running");
        kBusyDelay(20); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        kSleep(5); /* suspend so other task can run */
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 running");
        kBusyDelay(30); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        kSleep(5);
    }
}

#endif
