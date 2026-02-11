/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.9.13
 * Architecture     :   ARMv6/7m
 *
 * Logger implementation isolated from application code.
 *
 ******************************************************************************/

#include <logger.h>


#if (CONF_LOGGER == 1)
#include <qemu_uart.h>
#include <stdio.h>
#include <kstring.h>
#include <kmem.h>
#include <kmesgq.h>
#include <ksch.h>
#endif
#include <stdarg.h>

#if (CONF_LOGGER == 0)
VOID logInit(RK_PRIO priority)
{
    K_UNUSE(priority);

}
VOID longEnqueue(UINT level, const char *fmt, ...)
{

    (void)level;
    va_list args;
    va_start(args, fmt);
    (void)(args);
    (void)(fmt);
    va_end(args);

}
#else

/* standard log structure */
struct log
{
    RK_TICK t; /* timestamp */
    CHAR s[LOGLEN]; /*formatted string */
    UINT    level; /* level 0=message, 1=fault */
} K_ALIGN(4);

typedef struct log Log_t;

static Log_t logBufPool[LOGPOOLSIZ] K_ALIGN(4);

/* backing buffer for the logger queue (messages are 1-word pointers) */
RK_DECLARE_MESG_QUEUE_BUF(logQBuf, VOID *, LOGPOOLSIZ)

/* logger mail queue */
static RK_MESG_QUEUE logQ;

/* logger mem allocator */
static RK_MEM_PARTITION qMem;

/* logger task handle and stack */
static RK_TASK_HANDLE logTaskHandle;
static RK_STACK logstack[LOG_STACKSIZE] K_ALIGN(8);
static inline VOID logPrintf_(const char *fmt, ...)
{

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

/* formatted string input */
VOID logEnqueue(UINT level, const char *fmt, ...)
{

    Log_t *logPtr = (Log_t*)kMemPartitionAlloc(&qMem);
    RK_BARRIER
    if (logPtr)
    {
      
        Log_t* p = logPtr;
        p->level = level;
        p->t = kTickGetMs();
        /* this is reentrant */
        va_list args;
        va_start(args, fmt);
        vsnprintf(p->s, sizeof(p->s), fmt, args);
        va_end(args);

        if (kMesgQueueSend(&logQ, &p, RK_NO_WAIT) != RK_ERR_SUCCESS)
        {
            RK_ERR err = kMemPartitionFree(&qMem, &p);
            K_ASSERT(err == RK_ERR_SUCCESS);
        }

    }
    else
    {
        if (level == LOG_LEVEL_FAULT)
        {
            va_list args;
            va_start(args, fmt);
            fprintf(stderr, "@%lu ms ", kTickGetMs());
            vfprintf(stderr, fmt, args);
            va_end(args);
            RK_ABORT
        }
    }
}


static VOID LoggerTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {

        VOID *recvPtr = NULL;
        while (kMesgQueueRecv(&logQ, &recvPtr, RK_WAIT_FOREVER) == RK_ERR_SUCCESS)
        {
            K_ASSERT(recvPtr != NULL);
            Log_t *logPtr = (Log_t *)recvPtr;

            if (logPtr->level == LOG_LEVEL_FAULT)
            {
                logPrintf_("%8lu ms ERROR :: %s \r\n", logPtr->t, logPtr->s);

                RK_ABORT
            }
            else
            {
                logPrintf_("%8lu ms :: %s \r\n", logPtr->t, logPtr->s);

            }
            RK_ERR err = kMemPartitionFree(&qMem, recvPtr);
            K_ASSERT(err == RK_ERR_SUCCESS);

        }
    }
}

VOID logInit(RK_PRIO priority)
{
    RK_ERR err = kMemPartitionInit(&qMem, logBufPool, sizeof(Log_t), LOGPOOLSIZ);
    K_ASSERT(err==RK_ERR_SUCCESS);

    err = kMesgQueueInit(&logQ, logQBuf, RK_MESGQ_MESG_SIZE(VOID *), LOGPOOLSIZ);

    K_ASSERT(err==RK_ERR_SUCCESS);

    err = kCreateTask(&logTaskHandle, LoggerTask, RK_NO_ARGS,
                          "LogTsk", logstack, LOG_STACKSIZE,
                          priority, RK_PREEMPT);
    K_ASSERT(err==RK_ERR_SUCCESS);

}

#endif
