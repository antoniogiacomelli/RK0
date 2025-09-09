/**
/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.6.6
 * Architecture     :   ARMv6/7m
 *
 * Copyright (C) 2025 Antonio Giacomelli
 *
 * Licensed under the Apache License, Version 2.0 (the “License”);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#include <application.h>
#include <stdarg.h>
#include <kstring.h>

#if (RK_CONF_QUEUE != ON)
#error Mail Queue Service needed for this app example
#endif
#if (RK_CONF_SLEEPQ != ON)
#error Events Service needed for this app example
#endif
#if (RK_CONF_MUTEX != ON)
#error Mutex Service needed for this app example
#endif



/**** LOGGER / PRINTF - THIS IS A STACK EATER ****/
/***  PREFER LOGGING INTERNALLY, NO PRINTF    ****/

#define LOGLEN     64
#define LOGBUFSIZ  6 /* if you are missing prints, consider increasing 
                        the number of buffers */

static UINT logMemErr = 0;
 struct log
 {
     RK_TICK   t;
     CHAR      s[LOGLEN];
 }__RK_ALIGN(4);  /* 68 B*/

typedef struct log Log_t;

/* memory partition pool: 6x68 =  408 Bytes */
/* _user_heap is set to 1024 B */
Log_t  qMemBuf[LOGBUFSIZ] __RK_HEAP;

/* buffer for the mail queue */
 VOID  *qBuf[LOGBUFSIZ];

/* logger mail queue */
 RK_QUEUE    logQ;

/* logger mem allocator */
 RK_MEM      qMem;

/* formatted string input */
 VOID logPost(const char *fmt, ...)
 {
     kSchLock();
     Log_t *logPtr = kMemAlloc(&qMem);
     if (logPtr)
     {
         logPtr->t = kTickGetMs();
         va_list args;
         va_start(args, fmt);
         int len = vsnprintf(logPtr->s, sizeof(logPtr->s), fmt, args);
         va_end(args);
         /* if len >= size of the message it has been truncated */
         if (len >= (int)sizeof(logPtr->s))
         {
             /* add  "..." to replace" where the truncation happend */
             if (len > 4)
                 RK_STRCPY(&logPtr->s[len - (int)4], "...");
         }
         /* if queue post fails, free memory so it doesnt leak */
         if (kQueuePost(&logQ, (VOID*)logPtr, RK_NO_WAIT) != RK_SUCCESS)
             kMemFree(&qMem, logPtr);
     }
     else
     {
        logMemErr ++ ;
     }
     kSchUnlock();
 }

/* _write in apputils.c */
 void kprintf(const char *fmt, ...)
 {
        
         va_list args;
         va_start(args, fmt);
         vfprintf(stderr, fmt, args);
         va_end(args);
 }


VOID LoggerTask(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
 
        Log_t* logPtr;
        if (kQueuePend(&logQ, (VOID**)&logPtr, RK_WAIT_FOREVER) == RK_SUCCESS)
        {
             kSchLock();
             kprintf("%lu ms :: %s \r\n", logPtr->t, logPtr->s);
             kMemFree(&qMem, logPtr);
             kSchUnlock();
        }

    }	
}

#define LOG_INIT \
    kassert(!kMemInit(&qMem, qMemBuf, sizeof(Log_t), LOGBUFSIZ)); \
    kassert(!kQueueInit(&logQ, qBuf, LOGBUFSIZ)); 
    
/******************************************************************************/
/* APPLICATION                                                                */
/******************************************************************************/
#define STACKSIZE       256

/**** Declaring tasks's required data ****/

RK_DECLARE_TASK(task1Handle, Task1, stack1, STACKSIZE)
RK_DECLARE_TASK(task2Handle, Task2, stack2, STACKSIZE)
RK_DECLARE_TASK(task3Handle, Task3, stack3, STACKSIZE)
RK_DECLARE_TASK(logTaskHandle, LoggerTask, logstack, STACKSIZE)


/* Synchronisation Barrier */

typedef struct
{
    RK_MUTEX lock;
    RK_SLEEP_QUEUE allSynch;
    UINT count;        /* number of tasks in the barrier */
    UINT round;        /* increased every time all tasks synch     */
    UINT required;     /* required tasks in the barrier */
} Barrier_t;

VOID BarrierInit(Barrier_t *const barPtr, UINT required)
{
    kMutexInit(&barPtr->lock, RK_INHERIT);
    kSleepQInit(&barPtr->allSynch);
    barPtr->count = 0;
    barPtr->round = 0;
    barPtr->required = required;
}

VOID BarrierWait(Barrier_t *const barPtr)
{
    UINT myRound = 0;
    kMutexLock(&barPtr->lock, RK_WAIT_FOREVER);

    logPost(" %s entered the barrier",  RK_RUNNING_NAME);

    /* save round number */
    myRound = barPtr->round;
    /* increase count on this round */
    barPtr->count++;

    if (barPtr->count == barPtr->required)
    {
        /* reset counter, inc round, broadcast to sleeping tasks */
        barPtr->round++;
        barPtr->count = 0;
        kCondVarBroadcast(&barPtr->allSynch);
    }
    else
    {
        /* a proper wake signal might happen after inc round */
        while ((UINT)(barPtr->round - myRound) == 0U)
        {
            kCondVarWait(&barPtr->allSynch, &barPtr->lock, RK_WAIT_FOREVER);
        }
    }

    kMutexUnlock(&barPtr->lock);
    
}

#define N_BARR_TASKS 3

Barrier_t syncBarrier;

/** Initialise tasks and kernel/application objects */
VOID kApplicationInit(VOID)
{

    kassert(!kCreateTask(&task1Handle, Task1, RK_NO_ARGS, "Task1", stack1, STACKSIZE, 3, RK_PREEMPT));
    
    kassert(!kCreateTask(&task2Handle, Task2, RK_NO_ARGS, "Task2", stack2, STACKSIZE, 2, RK_PREEMPT));
    
    kassert(!kCreateTask(&task3Handle, Task3, RK_NO_ARGS, "Task3", stack3, STACKSIZE, 1, RK_PREEMPT));

    kassert(!kCreateTask(&logTaskHandle, LoggerTask, RK_NO_ARGS, "LogTsk", logstack, STACKSIZE, 4, RK_PREEMPT));

    LOG_INIT
        
    BarrierInit(&syncBarrier, N_BARR_TASKS);
}

VOID Task1(VOID* args)
{
    RK_UNUSEARGS
    
    while (1)   
    {
        BarrierWait(&syncBarrier);
		
        logPost("<-- %s passed the barrier", RK_RUNNING_NAME);

        kSleepPeriodic(800);
        
    }
}

VOID Task2(VOID* args)
{
    RK_UNUSEARGS
    
    while (1)
    {
        BarrierWait(&syncBarrier);
		        
        logPost("<-- %s passed the barrier", RK_RUNNING_NAME);

        kSleepPeriodic(500);


	}
}

VOID Task3(VOID* args)
{
    RK_UNUSEARGS
    while (1)
    {
        BarrierWait(&syncBarrier);
 
        logPost("<-- %s passed the barrier", RK_RUNNING_NAME);

        kSleepPeriodic(300);
	}
}

