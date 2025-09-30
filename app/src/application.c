/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/**                     RK0 — Real-Time Kernel '0'                            */
/** Copyright (C) 2025 Antonio Giacomelli <dev@kernel0.org>                   */
/**                                                                           */
/** VERSION          :   V0.8.0                                               */
/** ARCHITECTURE     :   ARMv7m                                               */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/
/******************************************************************************/

/* Synch barrier example using Ports */

#include <application.h>
#include <logger.h>

#define LOG_PRIORITY 5
#define STACKSIZE 256

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(barrierHandle, BarrierServer, stackB, STACKSIZE)

#define N_BARR_TASKS 3    /* logical barrier size */
#define PORT_CAPACITY 4U  /* ring must be power-of-two */
#define PORT_MSG_WORDS 2U /* meta-only (no payload) */

_Static_assert((PORT_CAPACITY & (PORT_CAPACITY - 1U)) == 0U,
               "PORT_CAPACITY must be power-of-two");

static RK_PORT barrierPort;
RK_DECLARE_PORT_BUF(barrierBuf, RK_PORT_MESG_2WORD, PORT_CAPACITY)

static inline VOID BarrierWaitPort(VOID)
{
    RK_MAILBOX replyBox;
    kMailboxInit(&replyBox); /* per-call reply route */

    RK_PORT_MESG_2WORD call = {0};
    UINT ack = 0;
    /* Send and pend for reply from server */
    kassert(!kPortSendRecv(&barrierPort, (ULONG *)&call, &replyBox, &ack,
                           RK_WAIT_FOREVER));
    K_UNUSE(ack); /* reply code unused (presence is the sync) */
}

VOID BarrierServer(VOID *args)
{
    RK_UNUSEARGS;

    /* Store meta of blocked callers so we can reply later */
    RK_PORT_MESG_2WORD waiters[N_BARR_TASKS];
    UINT arrived = 0;

    while (1)
    {
        RK_PORT_MESG_2WORD msg; /* meta-only message from a caller */
        kassert(!kPortRecv(&barrierPort, &msg, RK_WAIT_FOREVER));
        
 
        if (arrived + 1U == N_BARR_TASKS)
        {
            /*  reply to all previous waiters ... */
            for (UINT i = 0; i < arrived; ++i)
                kassert(!kPortReply(&barrierPort, (ULONG const *)&waiters[i], 1U));

            /* ... and reply to the last one, ending the server transaction */
            kassert(!kPortReplyDone(&barrierPort, (ULONG const *)&msg, 1U));

            arrived = 0;
         }
        else
        {
            waiters[arrived++] = msg; /* keep caller meta for later reply */
        }
    }
}

VOID kApplicationInit(VOID)
{
    /*  server adopts callers during service... no mutex needed for prio inheritance */
    kassert(!kCreateTask(&barrierHandle, BarrierServer, RK_NO_ARGS,
                         "Barrier", stackB, STACKSIZE, 4, RK_PREEMPT));

    /* create a server : port + owner */
    kassert(!kPortInit(&barrierPort, barrierBuf, PORT_MSG_WORDS, PORT_CAPACITY,
                       barrierHandle));

    /* clients */
    kassert(!kCreateTask(&task1Handle, Task1, RK_NO_ARGS,
                         "Task1", stack1, STACKSIZE, 2, RK_PREEMPT));
    kassert(!kCreateTask(&task2Handle, Task2, RK_NO_ARGS,
                         "Task2", stack2, STACKSIZE, 3, RK_PREEMPT));
    kassert(!kCreateTask(&task3Handle, Task3, RK_NO_ARGS,
                         "Task3", stack3, STACKSIZE, 1, RK_PREEMPT));

    logInit(LOG_PRIORITY);
}

 VOID Task1(VOID *args)
{
    RK_UNUSEARGS;
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
    RK_UNUSEARGS;
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
    RK_UNUSEARGS;
    while (1)
    {
        logPost("Task 3 is waiting at the barrier...");
        BarrierWaitPort();
        logPost("Task 3 passed the barrier!");
        kSleep(300);
    }
}
