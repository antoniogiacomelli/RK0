/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/**                                                                           */
/** RK0 - The Embedded Real-Time Kernel '0'                                   */
/** (C) 2026 Antonio Giacomelli <dev@kernel0.org>                             */
/**                                                                           */
/** VERSION: V0.92.0                                                          */
/**                                                                           */
/** You may obtain a copy of the License at :                                 */
/** http://www.apache.org/licenses/LICENSE-2.0                                */
/**                                                                           */
/******************************************************************************/

/******************************************************************************/

#include <logger.h>

#if (CONF_LOGGER == 1)
#include <qemu_uart.h>
#include <stdio.h>
#include <kstring.h>
#include <kmem.h>
#include <ktrace.h>
#include <ksch.h>
#include <ktaskevents.h>
#include <ktimer.h>
#endif
#include <stdarg.h>

#if (CONF_LOGGER == 0)
VOID logInit(RK_PRIO priority)
{
    K_UNUSE(priority);
}
VOID logEnqueue(UINT level, const char *fmt, ...)
{
    (void)level;
    (void)(fmt);
}
#else


/* standard log structure */
struct log
{
    RK_TICK t;      /* timestamp */
    CHAR s[LOGLEN]; /*formatted string */
    UINT level;     /* level 0=message, 1=fault */
} K_ALIGN(4);

typedef struct log Log_t;

/* Multiple task/ISR producers, one consumer. The fixed pool owns records;
 * the private circular buffer transfers only pointers. Formatting and UART
 * output remain outside its short critical sections.
 */
#define LOG_WAKE RK_EVENT_1
static Log_t logBufPool[LOGPOOLSIZ] K_ALIGN(4);
static RK_MEM_PARTITION qMem;
static Log_t *logRing[LOGPOOLSIZ];
static UINT logRead;
static UINT logWrite;
static UINT logCount;
static ULONG errLogSend;
static ULONG errLogPool;
static volatile RK_BOOL logReady;

static RK_TASK_HANDLE logTaskHandle;
static RK_STACK logstack[LOG_STACKSIZE] K_ALIGN(8);

static VOID logPrintf_(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static RK_BOOL logPush_(Log_t *const record)
{
    RK_CR_AREA
    RK_CR_ENTER
    if (logCount == LOGPOOLSIZ)
    {
        errLogSend++;
        RK_CR_EXIT
        return (RK_FALSE);
    }
    logRing[logWrite] = record;
    logWrite = (logWrite + 1U) % LOGPOOLSIZ;
    logCount++;
    RK_CR_EXIT
    return (RK_TRUE);
}

static Log_t *logPop_(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    Log_t *record = NULL;
    if (logCount != 0U)
    {
        record = logRing[logRead];
        logRing[logRead] = NULL;
        logRead = (logRead + 1U) % LOGPOOLSIZ;
        logCount--;
    }
    RK_CR_EXIT
    return (record);
}

VOID logEnqueue(UINT level, const char *fmt, ...)
{
    if (level == LOG_LEVEL_FAULT)
    {
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "@%lu ms ", kTickGetMs());
        vfprintf(stderr, fmt, args);
        va_end(args);
        RK_ABORT
        return;
    }
    if (logReady == RK_FALSE)
        return;

    Log_t *const record = kMemPartitionAlloc(&qMem);
    if (record == NULL)
    {
        RK_CR_AREA
        RK_CR_ENTER
        errLogPool++;
        RK_CR_EXIT
#if (CONF_LOG_ERROR == ON)
        kPuts("E\r\n");
#endif
        return;
    }
    record->level = level;
    record->t = kTickGetMs();
    va_list args;
    va_start(args, fmt);
    int const fmtLen = vsnprintf(record->s, sizeof(record->s), fmt, args);
    va_end(args);
    if (fmtLen < 0)
    {
        RK_STRCPY(record->s, "[LOG FORMAT ERROR]");
    }
    else if ((ULONG)fmtLen >= sizeof(record->s))
    {
        record->s[sizeof(record->s) - 2U] = '?';
        record->s[sizeof(record->s) - 1U] = '\0';
    }
    if (logPush_(record) == RK_FALSE)
    {
        RK_ERR const err = kMemPartitionFree(&qMem, record);
        K_ASSERT(err == RK_ERR_SUCCESS);
        return;
    }
    /* A sticky event is a wake hint, not a count. Publication between the
     * consumer's empty check and wait cannot lose the notification.
     * The consumer may run immediately and free record: do not use it again.
     */
    RK_ERR const err = kEventSet(logTaskHandle, LOG_WAKE);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

static VOID LoggerTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        Log_t *record;
        while ((record = logPop_()) != NULL)
        {
            logPrintf_("%8lu ms :: %s \r\n", record->t, record->s);
            RK_ERR const err = kMemPartitionFree(&qMem, record);
            K_ASSERT(err == RK_ERR_SUCCESS);
        }
        RK_ERR const err = kEventGet(LOG_WAKE, RK_EVENT_ANY, NULL, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

VOID logInit(RK_PRIO priority)
{
    RK_ERR err = kMemPartitionInit(&qMem, logBufPool, sizeof(Log_t), LOGPOOLSIZ);
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
        return;
    err = kTraceNameObject(&qMem, "LogMem");
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err != RK_ERR_SUCCESS)
        return;
    err = kTaskInit(&logTaskHandle, LoggerTask, RK_NO_ARGS, "LogTsk",
                    logstack, LOG_STACKSIZE, priority, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
    if (err == RK_ERR_SUCCESS)
    {
        RK_COMPILER_BARRIER
        logReady = RK_TRUE;
    }
}
#endif
