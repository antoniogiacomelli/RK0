/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************
 *
 *                     RK0 — Real-Time Kernel '0'
 *
 * Version          :   V0.8.0
 * Architecture     :   ARMv6/7m
 *
 * Logger implementation isolated from application code.
 *
 ******************************************************************************/

#include <logger.h>

#if (RK_CONF_LOGGER == ON)

#define LOGLEN 128 
#define LOGBUFSIZ 8 /* if you are missing prints, consider increasing \
                     * the number of buffers */
#define LOG_STACKSIZE 128
#define LOG_PRIORITY 3

static UINT logMemErr = 0;

struct log
{
    RK_TICK t;
    CHAR s[LOGLEN];
} K_ALIGN(4);

typedef struct log Log_t;

/* memory partition pool: 8x32 =  256 Bytes */
Log_t qMemBuf[LOGBUFSIZ] RK_SECTION_HEAP;

/* backing buffer for the logger queue (messages are 1-word pointers) */
RK_DECLARE_MESG_QUEUE_BUF(logQBuf, VOID*, LOGBUFSIZ)

/* logger mail queue */
static RK_MESG_QUEUE logQ;

/* logger mem allocator */
static RK_MEM_PARTITION qMem;

/* logger task handle and stack */
static RK_TASK_HANDLE logTaskHandle;
static RK_STACK logstack[LOG_STACKSIZE] K_ALIGN(8);
VOID kprintf(const char *fmt, ...)
{

    RK_CR_AREA
    RK_CR_ENTER
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    RK_CR_EXIT
}
/* formatted string input */
VOID logPost(const char *fmt, ...)
{
    RK_CR_AREA

    RK_CR_ENTER

    Log_t *logPtr = kMemPartitionAlloc(&qMem);
    if (logPtr)
    {
        VOID *p = logPtr;
        logPtr->t = kTickGetMs();
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(logPtr->s, sizeof(logPtr->s), fmt, args);
        va_end(args);
        if (len >= (int)sizeof(logPtr->s))
        {
            if (len > 4)
            {
                RK_STRCPY(&logPtr->s[len - (int)4], "...");
            }
        }
        if (kMesgQueueSend(&logQ, &p, RK_NO_WAIT) == RK_ERR_SUCCESS)
        {
            (void)kTaskFlagsSet(logTaskHandle, 0x1);
        }
        else
        {
            kMemPartitionFree(&qMem, p);
            logMemErr++;
            (void)kTaskFlagsSet(logTaskHandle, 0x1);
        }

        RK_CR_EXIT
    }
    else
    {
        logMemErr++;
    }
}

    static VOID LoggerTask(VOID * args)
    {
        RK_UNUSEARGS
        while (1)
        {

            ULONG gotFlags = 0;
            kTaskFlagsGet(0x01, RK_FLAGS_ANY, &gotFlags, RK_WAIT_FOREVER);
            if (gotFlags & 0x01)
            {
                VOID *recvPtr = NULL;
                while (kMesgQueueRecv(&logQ, &recvPtr, RK_NO_WAIT) == RK_ERR_SUCCESS)
                {
                    kassert(recvPtr != NULL);
                    Log_t *logPtr = (Log_t *)recvPtr;
                    kprintf("%8lu ms :: %s \r\n", logPtr->t, logPtr->s);
                    kMemPartitionFree(&qMem, recvPtr);
                }
            }
        }
    }

    VOID logInit(VOID)
    {
        kassert(!kMemPartitionInit(&qMem, qMemBuf, sizeof(Log_t), LOGBUFSIZ));
        kassert(!kMesgQueueInit(&logQ, logQBuf, K_MESGQ_MESG_SIZE(VOID*), LOGBUFSIZ));
        kassert(!kCreateTask(&logTaskHandle, LoggerTask, RK_NO_ARGS,
                             "LogTsk", logstack, LOG_STACKSIZE,
                             LOG_PRIORITY, RK_PREEMPT));
    }

#endif
