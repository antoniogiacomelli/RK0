/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.1
 * Architecture     :   ARMv6/7m
 *
 * Logger implementation isolated from application code.
 *
 ******************************************************************************/

#include <logger.h>

#if (CONF_LOGGER == ON)

#define LOGLEN 64    /* max length of a single log message */
#define LOGPOOLSIZ 8 /* number of buffers in the pool */
#define LOG_STACKSIZE 128

#define LOG_COUNT_ERR 0

#if (LOG_COUNT_ERR == 1)
static UINT logMemErr = 0;   /* number of failing allocs (if missing prints) */
static UINT logQueueErr = 0; /* number of failing queue sends (if missing prints) */
#endif
struct log
{
    RK_TICK t;
    CHAR s[LOGLEN];
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
VOID kprintf(const char *fmt, ...)
{

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
/* formatted string input */
VOID logPost(const char *fmt, ...)
{
#if (LOG_COUNT_ERR == 1)
    RK_CR_AREA
#endif

    Log_t *logPtr = kMemPartitionAlloc(&qMem);
    if (logPtr)
    {
        VOID *p = logPtr;
        logPtr->t = kTickGetMs();

        va_list args;
        va_start(args, fmt);
        vsnprintf(logPtr->s, sizeof logPtr->s, fmt, args);
        va_end(args);

        if (kMesgQueueSend(&logQ, &p, RK_NO_WAIT) != RK_ERR_SUCCESS)
        {
            kMemPartitionFree(&qMem, p);

#if (LOG_COUNT_ERR == 1)
            RK_CR_ENTER;
            logQueueErr++;
            RK_CR_EXIT;
#endif
        }
    }
    else
    {
#if (LOG_COUNT_ERR == 1)
        RK_CR_ENTER;
        logMemErr++;
        RK_CR_EXIT;
#endif
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
            kprintf("%8lu ms :: %s \r\n", logPtr->t, logPtr->s);
            kMemPartitionFree(&qMem, recvPtr);
        }
    }
}

VOID logInit(RK_PRIO priority)
{
    K_ASSERT(!kMemPartitionInit(&qMem, logBufPool, sizeof(Log_t), LOGPOOLSIZ));
    K_ASSERT(!kMesgQueueInit(&logQ, logQBuf, RK_MESGQ_MESG_SIZE(VOID *), LOGPOOLSIZ));
    K_ASSERT(!kCreateTask(&logTaskHandle, LoggerTask, RK_NO_ARGS,
                          "LogTsk", logstack, LOG_STACKSIZE,
                          priority, RK_PREEMPT));
}

#endif
