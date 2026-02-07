/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2026 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.9.11                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

#define SYNCHBARR_MESGPASS_APP 0

/* Synch barrier example using Message-Passing */

#include <kapi.h>
/* Configure the application logger faciclity here */
#include <logger.h> 
int main(void)
{
    

    kCoreInit();
    
    kInit();

    while(1)
    {  
        kErrHandler(RK_FAULT_APP_CRASH);   
    }
 
}

#if (SYNCHBARR_MESGPASS_APP == 1)

#define LOG_PRIORITY 5
#define STACKSIZE 128

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(barrierHandle, BarrierServer, stackB, STACKSIZE)

RK_DECLARE_TASK(task4Handle, Task4, stack4, STACKSIZE)


#define N_BARR_TASKS 3    
#define PORT_CAPACITY 3U  
#define PORT_MESG_WORDS 2U  
static RK_PORT barrierPort;
RK_DECLARE_PORT_BUF(barrierBuf, RK_PORT_MESG_2WORD, PORT_CAPACITY)

static inline VOID BarrierWaitPort(VOID)
{
    RK_MAILBOX replyBox = {0};
    kMailboxInit(&replyBox); /* a new obj per call */

    RK_PORT_MESG_2WORD call = {0};
    UINT ack = 0;
    /* send and pend for reply from server */
    RK_ERR err = kPortSendRecv(&barrierPort, (ULONG *)&call, &replyBox, &ack,
                           RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);
    K_UNUSE(ack); /* reply code unused (presence is the sync) */
}

VOID BarrierServer(VOID *args)
{
    RK_UNUSEARGS

    /* Store meta of blocked callers so we can reply later */
    RK_PORT_MESG_2WORD waiters[N_BARR_TASKS];
    UINT arrived = 0;

    while (1)
    {
        RK_PORT_MESG_2WORD mesg; /* meta-only message from a caller */
        RK_ERR err = kPortRecv(&barrierPort, &mesg, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        
 
        if (arrived + 1U == N_BARR_TASKS)
        {
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
            waiters[arrived++] = mesg; /* keep caller meta for later reply */
        }
    }
}

VOID kApplicationInit(VOID)
{
    /*  server adopts callers during service... no mutex needed for prio inheritance */
   RK_ERR err = kCreateTask(&barrierHandle, BarrierServer, RK_NO_ARGS,
                         "Barrier", stackB, STACKSIZE, 4, RK_PREEMPT);
    K_ASSERT(err==RK_ERR_SUCCESS);

    /* create a server : port + owner */
    err = kPortInit(&barrierPort, barrierBuf, PORT_MESG_WORDS, PORT_CAPACITY,
                       barrierHandle);

    K_ASSERT(err==RK_ERR_SUCCESS);

    /* clients */
    err = kCreateTask(&task1Handle, Task1, RK_NO_ARGS,
                         "Task1", stack1, STACKSIZE, 2, RK_PREEMPT);

    K_ASSERT(err==RK_ERR_SUCCESS);

    err = kCreateTask(&task2Handle, Task2, RK_NO_ARGS,
                         "Task2", stack2, STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err==RK_ERR_SUCCESS);
    

    err = kCreateTask(&task4Handle, Task4, RK_NO_ARGS,
                         "Task4", stack4, STACKSIZE, 1, RK_PREEMPT);

    K_ASSERT(err==RK_ERR_SUCCESS);

    err = kCreateTask(&task3Handle, Task3, RK_NO_ARGS,
                         "Task3", stack3, STACKSIZE, 1, RK_PREEMPT);



    K_ASSERT(err == RK_ERR_SUCCESS);

    logInit(LOG_PRIORITY);
}



VOID Task4(VOID* args)
{
    RK_UNUSEARGS
    UINT count = 0;
    while (1)
    {

        logPost("Task4: sleep periodic");
        kSleepRelease(300); /*P=300 ticks */
        /* wake here */
        count += 1U;
        if (count >= 5)
        {
            kDelay(25); /* spin */
            count=0;
            /* every 5 activations there will be a drift */
        }
    }
}


 VOID Task1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 is waiting at the barrier...");
        BarrierWaitPort();
        logPost("Task 1 passed the barrier!");
        kSleep(800);
    }
}

VOID Task2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 is waiting at the barrier...");
        BarrierWaitPort();
        logPost("Task 2 passed the barrier!");
        kSleep(500);
    }
}

VOID Task3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 is waiting at the barrier...");
        BarrierWaitPort();
        logPost("Task 3 passed the barrier!");
        kSleep(300);
    }
}

#else

/* Synch Barrier Example using Cond Vars 
In this case total number of tasks is 4,
lowest priority is 4.
Modify kconfig.h */

/* set the logger priority to the lowest priority amongst user tasks */
#define LOG_PRIORITY 4

#define STACKSIZE 256

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)


/* Synchronisation Barrier */

typedef struct
{
    RK_MUTEX            lock;
    RK_SLEEP_QUEUE      cond;
    UINT count;        /* number of tasks in the barrier */
    UINT round;        /* increased every time all tasks synch     */
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
    logPost("[BARRIER: %u/%u]: %s ENTERED ",  barPtr->count, nTasks, RK_RUNNING_NAME);

    if (barPtr->count == nTasks)
    {
         logPost("[BARRIER: %u/%u]: %s WAKING ALL TASKS",  barPtr->count, nTasks, RK_RUNNING_NAME);


        /* reset counter, inc round, broadcast to sleeping tasks */
        barPtr->round++;
        barPtr->count = 0;
        kCondVarBroadcast(&barPtr->cond);
    }
    else
    {
         logPost("[BARRIER: %u/%u]: %s BLOCKED ",  barPtr->count, nTasks, RK_RUNNING_NAME);
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
    K_ASSERT(err==RK_ERR_SUCCESS);

    err = kCreateTask(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2, STACKSIZE, 2, RK_PREEMPT);
    K_ASSERT(err==RK_ERR_SUCCESS);

    err = kCreateTask(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3, STACKSIZE, 3, RK_PREEMPT);
    K_ASSERT(err==RK_ERR_SUCCESS);

	BarrierInit(&syncBarrier);
    logInit(LOG_PRIORITY);

}
VOID Task1(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 1 dispatched. Working...");
        kBusyDelay(100); /* simulate work */
         BarrierWait(&syncBarrier, N_BARR_TASKS);
        logPost("Task 1 left the barrier!");
        kSleep(1); /* suspend so other task can run */
    }
}

VOID Task2(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 2 dispatched. Working...");
        kBusyDelay(200); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        logPost("Task 2 left the barrier!");
        kSleep(1); /* suspend so other task can run */
    }
}

VOID Task3(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        logPost("Task 3 dispatched. Working...");
        kBusyDelay(300); /* simulate work */
        BarrierWait(&syncBarrier, N_BARR_TASKS);
        logPost("Task 3 left the barrier!");
        kSleep(1); /* suspend so other task can run */
    }
}

#endif