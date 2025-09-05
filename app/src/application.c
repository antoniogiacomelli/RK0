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
 Log_t  *qBuf[LOGBUFSIZ];

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
    kassert(!kQueueInit(&logQ, (VOID**)&qBuf, LOGBUFSIZ)); 
    
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


#if 0
#include <application.h>
#include <stdarg.h>
#include <string.h>
#include <kstring.h>
#include <ctype.h>
#define RK_WORD ULONG

static INT parse_sp_line(const CHAR *s, INT *out)
{
    // expect "SP=" then optional sign then digits
    if (!s || s[0] != 'S' || s[1] != 'P' || s[2] != '=')
        return 0;
    INT sign = 1, v = 0;
    UINT i = 3;
    if (s[i] == '-')
    {
        sign = -1;
        i++;
    }
    if (!isdigit((unsigned char)s[i]))
        return 0;
    while (isdigit((unsigned char)s[i]))
    {
        v = (v * 10) + (s[i] - '0');
        i++;
    }
    *out = sign * v;
    return 1; // parsed OK
}

/**** LOGGER / PRINTF - THIS IS A STACK EATER ****/
/***  PREFER LOGGING INTERNALLY, NO PRINTF    ****/

#define LOGLEN 64
#define LOGBUFSIZ 20 /* if you are missing prints, consider increasing \
                       the number of buffers */

static UINT logMemErr = 0;
struct log
{
    RK_TICK t;
    CHAR s[LOGLEN];
} __RK_ALIGN(4); /* 68 B*/

typedef struct log Log_t;

/* memory partition pool: 6x68 =  408 Bytes */
/* _user_heap is set to 1024 B */
Log_t qMemBuf[LOGBUFSIZ] __RK_HEAP;

/* buffer for the mail queue */
Log_t *qBuf[LOGBUFSIZ];

/* logger mail queue */
RK_QUEUE logQ;

/* logger mem allocator */
RK_MEM qMem;

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
        if (kQueuePost(&logQ, (VOID *)logPtr, RK_NO_WAIT) != RK_SUCCESS)
            kMemFree(&qMem, logPtr);
    }
    else
    {
        logMemErr++;
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

VOID LoggerTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {

        Log_t *logPtr;
        if (kQueuePend(&logQ, (VOID **)&logPtr, RK_WAIT_FOREVER) == RK_SUCCESS)
        {
            kSchLock();
            kprintf("%lu ms :: %s \r\n", logPtr->t, logPtr->s);
            kMemFree(&qMem, logPtr);
            kSchUnlock();
        }
    }
}

#define LOG_INIT                                                  \
    kassert(!kMemInit(&qMem, qMemBuf, sizeof(Log_t), LOGBUFSIZ)); \
    kassert(!kQueueInit(&logQ, (VOID **)&qBuf, LOGBUFSIZ));

/** APPLICATION */

#if (RK_CONF_CALLOUT_TIMER != ON)
#error Callout Timer Service needed for this app example
#endif
#if (RK_CONF_MRM != ON)
#error MRM Service needed for this app example
#endif
/* SPDX-License-Identifier: Apache-2.0 */
#include <application.h>
#include <kstring.h>
#include <stdarg.h>
#include <stdio.h>

/* ===== Guards ===== */
#if (RK_CONF_CALLOUT_TIMER != ON)
#error Callout Timer Service needed for this app example
#endif
#if (RK_CONF_MRM != ON)
#error MRM Service needed for this app example
#endif

/* ===== Helpers ===== */
__RK_INLINE static inline INT iabs(INT x) { return (x < 0) ? -x : x; }

/* ===== Tasks / priorities ===== */
#define STACKSIZE 512
RK_DECLARE_TASK(ctrlTaskHandle, ControlTask, ctrlStack, STACKSIZE) /* prio 0 */
RK_DECLARE_TASK(alarmTaskHandle, AlarmTask, almStack, STACKSIZE)   /* prio 1 */
RK_DECLARE_TASK(adcTaskHandle, AdcWorker, adcStack, STACKSIZE)     /* prio 2 */
RK_DECLARE_TASK(uartTaskHandle, UartTask, uartStack, STACKSIZE)    /* prio 4 */

RK_DECLARE_TASK(logTasHandle, LoggerTask, loggerStack, STACKSIZE) /* prio 4 */


/* Globals provided elsewhere:
   - mrmSetpoint, mrmMeasure  (RK_MRM channels of sample_t{INT value;})
   - g_limit_rpm, g_tol_rpm   (volatile INT tunables)
   - g_alarm_mask, g_trip_latched, alarm_set(), alarm_clear_all()
   - g_last_sp_ms (volatile RK_TICK; set in UartTask when SP changes)
   - g_duty_obs   (volatile INT; updated in ControlTask after computing duty)
   - ALM_POLL_MS, UNDER_MS, STUCK_MS, iabs()
   - ALM_* bit masks
*/

static volatile RK_TICK g_last_sp_ms = 0;
static volatile INT     g_duty_obs   = 0;


/* ===== Timer “ISRs” ===== */
static RK_TIMER tUartRx;  /* feeds RX chars + signals UartTask  */
static RK_TIMER tAdcTick; /* fills one ADC half + signals AdcW  */

/* ===== UART RX ring + signaling ===== */
#define RX_SZ 128
static volatile CHAR rxbuf[RX_SZ];
static volatile USHORT rx_w = 0, rx_r = 0;
enum
{
    SIG_UART_RX = 1u << 0
};

/* Script of commands to feed */
static const CHAR *script[] = {"SP=900\n", "SP=1200\n", "SP=500\n", "SP=1000\n"};
static UINT script_idx = 0, script_pos = 0;

/* ===== ADC ping-pong + signaling ===== */
#define ADC_CHUNK 64
static volatile USHORT adc_pp[2][ADC_CHUNK];
static UINT adc_phase = 0;
enum
{
    SIG_ADC_H0 = 1u << 1,
    SIG_ADC_H1 = 1u << 2
};

static UINT adc_seed = 0x12345678u;
__RK_INLINE static inline USHORT prng16(void)
{
    adc_seed = adc_seed * 1664525u + 1013904223u;
    return (USHORT)(adc_seed >> 16);
}

/* ===== Meaning mapping: RAW->RPM =====
   We model raw≈1000±100 as ≈1000±100 RPM for clarity. */
#define RAW_BASE 1000
#define RPM_BASE 1000
#define RPM_PER_COUNT 1
__RK_INLINE static inline INT raw_to_rpm(INT raw)
{
    return RPM_BASE + (raw - RAW_BASE) * RPM_PER_COUNT;
}

/* ===== Latest-only channels (MRM) ===== */
typedef struct
{
    INT value;
} sample_t;

#define MRM_BUFS 2
static RK_MRM mrmSetpoint, mrmMeasure;
static RK_MRM_BUF spBufPool[MRM_BUFS], meBufPool[MRM_BUFS];
static sample_t spMsgPool[MRM_BUFS], meMsgPool[MRM_BUFS];

/* ===== Alarms / trip ===== */
enum
{
    ALM_OVERSPEED = 1u << 0,
    ALM_UNDERSPEED = 1u << 1,
    ALM_STUCK = 1u << 2,
};

static volatile ULONG g_alarm_mask = 0;
static volatile UINT g_trip_latched = 0;

/* Tunables (runtime-changeable via UART) */
static volatile INT g_limit_rpm = 1300; /* absolute overspeed limit */
static volatile INT g_tol_rpm = 150;    /* underspeed tolerance */

/* Durations (ms) for debounce) — adjust to your tick */
#define UNDER_MS 150u
#define STUCK_MS 300u
#define ALM_POLL_MS 10u /* AlarmTask cadence */

__RK_INLINE static inline VOID alarm_set(ULONG bits)
{
    g_alarm_mask |= bits;
    g_trip_latched = 1;
}
__RK_INLINE static inline VOID alarm_clear_all(VOID)
{
    g_alarm_mask = 0;
    g_trip_latched = 0;
}

/* ===== Timer callouts (ISR-like, no blocking/alloc) ===== */
VOID uart_rx_callout(VOID *args)
{
    RK_UNUSEARGS;
    for (UINT k = 0; k < 2; ++k)
    {
        const CHAR *s = script[script_idx];
        CHAR c = s[script_pos++];
        if (c == '\0')
        {
            script_idx = (script_idx + 1u) % (sizeof(script) / sizeof(script[0]));
            script_pos = 0u;
            continue;
        }
        USHORT n = (USHORT)((rx_w + 1u) % RX_SZ);
        if (n != rx_r)
        {
            rxbuf[rx_w] = (CHAR)c;
            rx_w = n;
        } /* drop on overflow */
    }
    (void)kSignalSet(uartTaskHandle, SIG_UART_RX);
}

VOID adc_tick_callout(VOID *args)
{
    RK_UNUSEARGS;
    UINT h = adc_phase & 1u;
    for (UINT i = 0; i < ADC_CHUNK; ++i)
        adc_pp[h][i] = (USHORT)(1000u + (prng16() % 200u)); /* ~1000 ±100 raw */
    (void)kSignalSet(adcTaskHandle, (h == 0u) ? SIG_ADC_H0 : SIG_ADC_H1);
    adc_phase++;
}

/* ===== Tiny parsers (no scanf) ===== */
static INT parse_sp(const CHAR *s, INT *out)
{
    if (!s || s[0] != 'S' || s[1] != 'P' || s[2] != '=')
        return 0;
    INT sign = 1, v = 0;
    UINT i = 3;
    if (s[i] == '-')
    {
        sign = -1;
        i++;
    }
    if (s[i] < '0' || s[i] > '9')
        return 0;
    while (s[i] >= '0' && s[i] <= '9')
    {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    *out = sign * v;
    return 1;
}
static INT parse_lim(const CHAR *s, INT *out)
{
    if (!s || s[0] != 'L' || s[1] != 'I' || s[2] != 'M' || s[3] != '=')
        return 0;
    INT v = 0;
    UINT i = 4;
    if (s[i] < '0' || s[i] > '9')
        return 0;
    while (s[i] >= '0' && s[i] <= '9')
    {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    *out = v;
    return 1;
}
static INT parse_tol(const CHAR *s, INT *out)
{
    if (!s || s[0] != 'T' || s[1] != 'O' || s[2] != 'L' || s[3] != '=')
        return 0;
    INT v = 0;
    UINT i = 4;
    if (s[i] < '0' || s[i] > '9')
        return 0;
    while (s[i] >= '0' && s[i] < '9' + 1)
    {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    *out = v;
    return 1;
}

/* ===== UART task ===== */
VOID UartTask(VOID *args)
{
    RK_UNUSEARGS;

    CHAR line[64];
    UINT ll = 0;

    for (;;)
    {
        ULONG f;
        kSignalGet(0xFFFFFFFFUL, RK_FLAGS_ANY, &f, RK_WAIT_FOREVER);

        if (f & SIG_UART_RX)
        {
            while (rx_r != rx_w)
            {
                CHAR c = rxbuf[rx_r];
                rx_r = (USHORT)((rx_r + 1u) % RX_SZ);

                if (c == '\n' || c == '\r')
                {
                    line[ll] = 0;
                    ll = 0;
                    
                    INT v;
                    if (parse_sp(line, &v))
                    {
                        RK_MRM_BUF *b = kMRMReserve(&mrmSetpoint);
                        if (b)
                        {
                            sample_t msg = {.value = v};
                            (void)kMRMPublish(&mrmSetpoint, b, &msg);
                            g_last_sp_ms = kTickGetMs();

                        }
                        logPost("[UART] SP=%d rpm\r\n", v);
                    }
                    else if (!strcmp(line, "RST"))
                    {
                        alarm_clear_all();
                        logPost("[ALM] reset\r\n");
                    }
                    else if (parse_lim(line, &v))
                    {
                        g_limit_rpm = v;
                        logPost("[CFG] LIM=%d rpm\r\n", v);
                    }
                    else if (parse_tol(line, &v))
                    {
                        g_tol_rpm = v;
                        logPost("[CFG] TOL=%d rpm\r\n", v);
                    }
                    else if (line[0])
                    {
                        logPost("[UART] ? '%s'\r\n", line);
                    }
                }
                else if (ll + 1u < sizeof line)
                {
                    line[ll++] = (CHAR)c;
                }
                else
                {
                    ll = 0; /* overflow: drop */
                }
            }
        }
    }
}

/* ===== ADC worker: publish meas RPM via MRM ===== */
__RK_INLINE static inline INT avg16(volatile const USHORT *p, UINT n)
{
    ULONG s = 0;
    for (UINT i = 0; i < n; i++)
        s += p[i];
    return (INT)(s / n);
}

VOID AdcWorker(VOID *args)
{
    RK_UNUSEARGS;

    for (;;)
    {
        ULONG f;
        kSignalGet(0xFFFFFFFFUL, RK_FLAGS_ANY, &f, RK_WAIT_FOREVER);

        if (f & SIG_ADC_H0)
        {
            INT raw = avg16(adc_pp[0], ADC_CHUNK);
            INT rpm = raw_to_rpm(raw);
            RK_MRM_BUF *b = kMRMReserve(&mrmMeasure);
            if (b)
            {
                sample_t m = {.value = rpm};
                (void)kMRMPublish(&mrmMeasure, b, &m);
            }
        }
        if (f & SIG_ADC_H1)
        {
            INT raw = avg16(adc_pp[1], ADC_CHUNK);
            INT rpm = raw_to_rpm(raw);
            RK_MRM_BUF *b = kMRMReserve(&mrmMeasure);
            if (b)
            {
                sample_t m = {.value = rpm};
                (void)kMRMPublish(&mrmMeasure, b, &m);
            }
        }
    }
}

/* ===== Alarm task (latched trip) ===== */
VOID AlarmTask(VOID *args)
{
    RK_UNUSEARGS;

    /* Cadence for this checker */
    const RK_TICK period = ALM_POLL_MS;   /* ticks */

    /* Arm & debounce windows (ms) */
    #define ARM_MS       200u   /* arm relative checks this long after SP change */
    #define OVER_REL_MS  100u   /* relative overspeed must persist this long    */

    /* Debounce state */
    static UINT over_rel_ms = 0;
    static UINT under_ms    = 0;
    static UINT stuck_ms    = 0;
    static INT  last_meas   = 0;

    for (;;)
    {
        /* --- Get latest SP and MEAS from MRMs (non-blocking) --- */
        INT sp = 0, meas = 0;
        sample_t tmp;
        RK_MRM_BUF *b;

        b = kMRMGet(&mrmSetpoint, &tmp);
        if (b) { sp = tmp.value; (void)kMRMUnget(&mrmSetpoint, b); }

        b = kMRMGet(&mrmMeasure, &tmp);
        if (b) { meas = tmp.value; (void)kMRMUnget(&mrmMeasure, b); }

        /* Relative checks only after an arming window post-SP change */
        RK_TICK now   = kTickGetMs();
        UINT    armed = (g_last_sp_ms != 0) && ((now - g_last_sp_ms) >= ARM_MS);

        /* --- Absolute overspeed: immediate --- */
        if (meas > (INT)g_limit_rpm)
        {
            if (!(g_alarm_mask & ALM_OVERSPEED))
            {
                alarm_set(ALM_OVERSPEED);
                kprintf("[ALM] OVERSPEED(abs) meas=%d lim=%d\r\n", meas, g_limit_rpm);
            }
        }

        /* --- Relative overspeed: only when armed and we're actually pushing --- */
        if (armed && sp > 0 && g_duty_obs > 50 && (meas > sp + (INT)g_tol_rpm))
        {
            over_rel_ms += (UINT)period;
            if (over_rel_ms >= OVER_REL_MS && !(g_alarm_mask & ALM_OVERSPEED))
            {
                alarm_set(ALM_OVERSPEED);
                kprintf("[ALM] OVERSPEED(rel) meas=%d sp=%d tol=+%d\r\n",
                        meas, sp, g_tol_rpm);
            }
        }
        else
        {
            over_rel_ms = 0;
        }

        /* --- Underspeed: only when armed and commanding positive duty --- */
        if (armed && sp > 0 && g_duty_obs > 50 && (sp - meas) > (INT)g_tol_rpm)
        {
            under_ms += (UINT)period;
            if (under_ms >= UNDER_MS && !(g_alarm_mask & ALM_UNDERSPEED))
            {
                alarm_set(ALM_UNDERSPEED);
                kprintf("[ALM] UNDERSPEED meas=%d sp=%d tol=-%d\r\n",
                        meas, sp, g_tol_rpm);
            }
        }
        else
        {
            under_ms = 0;
        }

        /* --- Sensor stuck --- */
        if (iabs(meas - last_meas) <= 2)
        {
            stuck_ms += (UINT)period;
            if (stuck_ms >= STUCK_MS && !(g_alarm_mask & ALM_STUCK))
            {
                alarm_set(ALM_STUCK);
                kprintf("[ALM] SENSOR STUCK meas=%d (≥%u ms)\r\n", meas, STUCK_MS);
            }
        }
        else
        {
            stuck_ms = 0;
        }
        last_meas = meas;

        (void)kSleepPeriodic(period);
    }
}

/* ===== Control task (highest prio) ===== */
VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS;

    const RK_TICK period = 1; /* 1 tick cadence (adjust to your tick base) */
    INT sp = 0, meas = 0, duty = 0;
    UINT prn = 0, last_trip = 0;

    for (;;)
    {
        /* Latest SP & MEAS (MRM) */
        sample_t tmp;
        RK_MRM_BUF *b;
        b = kMRMGet(&mrmSetpoint, &tmp);
        if (b)
        {
            sp = tmp.value;
            (void)kMRMUnget(&mrmSetpoint, b);
        }
        b = kMRMGet(&mrmMeasure, &tmp);
        if (b)
        {
            meas = tmp.value;
            (void)kMRMUnget(&mrmMeasure, b);
        }

        if (g_trip_latched)
        {
            duty = 0;
            if (!last_trip)
            {
                logPost("[CTL] TRIP! duty forced to 0 (alarms=0x%lx)\r\n", (unsigned long)g_alarm_mask);
                last_trip = 1;
            }
        }
        else
        {
            last_trip = 0;
            /* crude PI-ish step; clamp 0..1000 */
            INT err = sp - meas;
            duty += (err >> 3);
            if (duty < 0)
                duty = 0;
            else if (duty > 1000)
                duty = 1000;
        }

        if ((prn++ % 100u) == 0u)
        {
            logPost("[CTL] sp=%d rpm meas=%d rpm duty=%d %s\r\n",
                    sp, meas, duty, g_trip_latched ? "[TRIP]" : "");
        }
        g_duty_obs = duty;

        (void)kSleepPeriodic(period);
    }
}

/* ===== App init ===== */
VOID kApplicationInit(VOID)
{
    /* MRMs: data size in WORDS */
    const ULONG words = (ULONG)((sizeof(sample_t) + sizeof(RK_WORD) - 1u) / sizeof(RK_WORD));
    kassert(!kMRMInit(&mrmSetpoint, spBufPool, spMsgPool, MRM_BUFS, words));
    kassert(!kMRMInit(&mrmMeasure, meBufPool, meMsgPool, MRM_BUFS, words));

    /* Tasks (names ≤ RK_OBJ_MAX_NAME_LEN) */
    kassert(!kCreateTask(&ctrlTaskHandle, ControlTask, RK_NO_ARGS, "CTRL", ctrlStack, STACKSIZE, 1, RK_PREEMPT));
    kassert(!kCreateTask(&alarmTaskHandle, AlarmTask, RK_NO_ARGS, "ALARM", almStack, STACKSIZE, 2, RK_PREEMPT));
    kassert(!kCreateTask(&adcTaskHandle, AdcWorker, RK_NO_ARGS, "ADCW", adcStack, STACKSIZE, 3, RK_PREEMPT));
    kassert(!kCreateTask(&uartTaskHandle, UartTask, RK_NO_ARGS, "UART", uartStack, STACKSIZE, 4, RK_PREEMPT));

    kassert(!kCreateTask(&logTasHandle, LoggerTask, RK_NO_ARGS, "LOG", loggerStack, STACKSIZE, 5, RK_PREEMPT));

    /* Timers: (phase, period, callout, args, reload=TRUE) */
    kassert(!kTimerInit(&tUartRx, 0, 25, uart_rx_callout, RK_NO_ARGS, TRUE));  /* slower SP changes */
    kassert(!kTimerInit(&tAdcTick, 0, 1, adc_tick_callout, RK_NO_ARGS, TRUE)); /* ~1 kHz meas */

    LOG_INIT
}

#endif