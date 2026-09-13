/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                     */
/* VERSION: V0.80.1                                                           */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * RK0 adaptation of the Thread-Metric RTOS benchmark suite.
 *
 * Build with RK_THREAD_METRIC_BENCH set to one of RK_TM_BENCH_* below.
 * The benchmark window defaults to the Thread-Metric recommended 30 seconds.
 */

#include <kapi.h>
#include <stdio.h>

#define RK_TM_BENCH_BASIC 1
#define RK_TM_BENCH_COOPERATIVE 2
#define RK_TM_BENCH_PREEMPTIVE 3
#define RK_TM_BENCH_INTERRUPT 4
#define RK_TM_BENCH_INTERRUPT_PREEMPTION 5
#define RK_TM_BENCH_MESSAGE 6
#define RK_TM_BENCH_SYNCHRONIZATION 7
#define RK_TM_BENCH_MEMORY 8

#ifndef RK_THREAD_METRIC_BENCH
#error "Define RK_THREAD_METRIC_BENCH to one RK_TM_BENCH_* value"
#endif

#ifndef RK_THREAD_METRIC_TEST_DURATION_MS
#define RK_THREAD_METRIC_TEST_DURATION_MS (30000UL)
#endif

#ifndef RK_THREAD_METRIC_CYCLES
#define RK_THREAD_METRIC_CYCLES (0UL)
#endif

#define TM_STACKSIZE (192U)
#define TM_WORKER_PRIO (10U)
#define TM_REPORT_PRIO (2U)
#define TM_FLAG RK_EVENT_1

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_BASIC)
#define TM_BENCH_NAME "basic-processing"
#define TM_BENCH_TITLE "Basic Single Thread Processing"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_COOPERATIVE)
#define TM_BENCH_NAME "cooperative-scheduling"
#define TM_BENCH_TITLE "Cooperative Scheduling"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_PREEMPTIVE)
#define TM_BENCH_NAME "preemptive-scheduling"
#define TM_BENCH_TITLE "Preemptive Scheduling"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT)
#define TM_BENCH_NAME "interrupt-processing"
#define TM_BENCH_TITLE "Interrupt Processing"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT_PREEMPTION)
#define TM_BENCH_NAME "interrupt-preemption-processing"
#define TM_BENCH_TITLE "Interrupt Preemption Processing"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_MESSAGE)
#define TM_BENCH_NAME "message-processing"
#define TM_BENCH_TITLE "Message Processing"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_SYNCHRONIZATION)
#define TM_BENCH_NAME "synchronization-processing"
#define TM_BENCH_TITLE "Synchronization Processing"
#elif (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_MEMORY)
#define TM_BENCH_NAME "memory-allocation"
#define TM_BENCH_TITLE "Memory Allocation"
#else
#error "Unknown RK_THREAD_METRIC_BENCH value"
#endif

#if (((RK_THREAD_METRIC_BENCH) == RK_TM_BENCH_INTERRUPT) ||                   \
     ((RK_THREAD_METRIC_BENCH) == RK_TM_BENCH_INTERRUPT_PREEMPTION)) &&       \
    (RK_CONF_TRACE == ON)
#error "IRQ Thread-Metric tests require RK_CONF_TRACE=OFF to keep IRQ timing isolated"
#endif

static volatile ULONG tmReportCycles;
static volatile ULONG tmErrors;

static VOID TmStopFault_(VOID)
{
    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

static VOID TmCheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("TM ERR %s %s err=%d\r\n", TM_BENCH_NAME, wherePtr, err);
        tmErrors++;
        K_ASSERT(err == RK_ERR_SUCCESS);
        TmStopFault_();
    }
}

#if (RK_THREAD_METRIC_CYCLES != 0UL)
static VOID TmStopPass_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000UL));
    }
}
#endif

static RK_TICK TmDurationTicks_(VOID)
{
    RK_TICK ticks = RK_MS_TO_TICKS(RK_THREAD_METRIC_TEST_DURATION_MS);

    if (ticks == 0UL)
    {
        ticks = 1UL;
    }

    return (ticks);
}

#if ((RK_THREAD_METRIC_BENCH != RK_TM_BENCH_COOPERATIVE) &&                  \
     (RK_THREAD_METRIC_BENCH != RK_TM_BENCH_PREEMPTIVE))
static VOID TmReportSimple_(ULONG const relativeMs, ULONG const periodTotal,
                            ULONG const total)
{
    printf("\r\n**** RK0 Thread-Metric %s Test **** Relative Time: %lu ms\r\n",
           TM_BENCH_TITLE, relativeMs);
    printf("Time Period Total: %lu\r\n", periodTotal);
    printf("Total: %lu errors=%lu tick_ms=%lu RK_CONF_SYSCORECLK=%lu "
           "RK_gSysCoreClock=%lu\r\n",
           total, tmErrors, RK_TICK_INTERVAL_MS, (ULONG)RK_CONF_SYSCORECLK,
           (ULONG)RK_gSysCoreClock);
}
#endif

static VOID TmAfterReport_(VOID)
{
    tmReportCycles++;

#if (RK_THREAD_METRIC_CYCLES != 0UL)
    if (tmReportCycles >= (ULONG)RK_THREAD_METRIC_CYCLES)
    {
        printf("TM PASS %s cycles=%lu errors=%lu\r\n", TM_BENCH_NAME,
               tmReportCycles, tmErrors);
        TmStopPass_();
    }
#endif
}

int main(void)
{
    kCoreInit();
    kInit();

    TmStopFault_();
}

#if ((RK_THREAD_METRIC_BENCH == RK_TM_BENCH_COOPERATIVE) ||                   \
     (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_PREEMPTIVE))
static ULONG TmAbsDelta_(ULONG const value, ULONG const average)
{
    return ((value >= average) ? (value - average) : (average - value));
}

static VOID TmReportFiveCounters_(ULONG const relativeMs,
                                  ULONG const periodTotal,
                                  ULONG const c0, ULONG const c1,
                                  ULONG const c2, ULONG const c3,
                                  ULONG const c4)
{
    ULONG const total = c0 + c1 + c2 + c3 + c4;
    ULONG const average = total / 5UL;
    ULONG maxDelta = TmAbsDelta_(c0, average);
    ULONG delta = TmAbsDelta_(c1, average);

    if (delta > maxDelta)
    {
        maxDelta = delta;
    }
    delta = TmAbsDelta_(c2, average);
    if (delta > maxDelta)
    {
        maxDelta = delta;
    }
    delta = TmAbsDelta_(c3, average);
    if (delta > maxDelta)
    {
        maxDelta = delta;
    }
    delta = TmAbsDelta_(c4, average);
    if (delta > maxDelta)
    {
        maxDelta = delta;
    }

    if (maxDelta > 1UL)
    {
        printf("TM FAIL %s counter skew average=%lu max_delta=%lu\r\n",
               TM_BENCH_NAME, average, maxDelta);
        tmErrors++;
    }

    printf("\r\n**** RK0 Thread-Metric %s Test **** Relative Time: %lu ms\r\n",
           TM_BENCH_TITLE, relativeMs);
    printf("Time Period Total: %lu\r\n", periodTotal);
    printf("Counters: %lu %lu %lu %lu %lu total=%lu average=%lu "
           "max_delta=%lu errors=%lu tick_ms=%lu RK_CONF_SYSCORECLK=%lu "
           "RK_gSysCoreClock=%lu\r\n",
           c0, c1, c2, c3, c4, total, average, maxDelta, tmErrors,
           RK_TICK_INTERVAL_MS, (ULONG)RK_CONF_SYSCORECLK,
           (ULONG)RK_gSysCoreClock);
}
#endif

#if ((RK_THREAD_METRIC_BENCH == RK_TM_BENCH_PREEMPTIVE) ||                    \
     (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT_PREEMPTION))
static VOID TmSuspendSelf_(VOID)
{
    TmCheckErr_(kEventGet(TM_FLAG, RK_OPT_EVENT_ANY, NULL, RK_WAIT_FOREVER),
                "event get");
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_PREEMPTIVE)
static VOID TmResumeTask_(RK_TASK_HANDLE const taskHandle)
{
    TmCheckErr_(kEventSet(taskHandle, TM_FLAG), "event set");
}
#endif

#if ((RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT) ||                     \
     (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT_PREEMPTION))
#if defined(STM32F103xB) || defined(RK_MCU_F103RB)
#define TM_IRQ_NUM (38UL)
#define TM_IRQ_HANDLER USART2_IRQHandler
#elif defined(QEMU_MACHINE_MICROBIT)
#define TM_IRQ_NUM (2UL)
#define TM_IRQ_HANDLER UART0_Handler
#else
#define TM_IRQ_NUM (5UL)
#define TM_IRQ_HANDLER UART0_Handler
#endif

#define TM_IRQ_REG_INDEX (TM_IRQ_NUM >> 5U)
#define TM_IRQ_BIT (1UL << (TM_IRQ_NUM & 31UL))
#define TM_NVIC_ISER(index)                                                   \
    (*(volatile ULONG *)(0xE000E100UL + (4UL * (index))))
#define TM_NVIC_ISPR(index)                                                   \
    (*(volatile ULONG *)(0xE000E200UL + (4UL * (index))))
#define TM_NVIC_ICPR(index)                                                   \
    (*(volatile ULONG *)(0xE000E280UL + (4UL * (index))))

static volatile ULONG tmIrqCount;
static volatile ULONG tmIrqErrors;

static VOID TmIrqInit_(VOID)
{
    TM_NVIC_ICPR(TM_IRQ_REG_INDEX) = TM_IRQ_BIT;
    TM_NVIC_ISER(TM_IRQ_REG_INDEX) = TM_IRQ_BIT;
    RK_DSB
    RK_ISB
}

static VOID TmCauseInterrupt_(VOID)
{
    TM_NVIC_ISPR(TM_IRQ_REG_INDEX) = TM_IRQ_BIT;
    RK_DSB
    RK_ISB
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_BASIC)
RK_DECLARE_TASK(tmWorkerHandle, TmBasicWorker, tmWorkerStack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmBasicReport, tmReportStack, TM_STACKSIZE)

static volatile ULONG tmBasicCounter;
static volatile ULONG tmBasicArray[1024];

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kTaskInit(&tmWorkerHandle, TmBasicWorker, RK_NO_ARGS, "TMB0",
                          tmWorkerStack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "worker task");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmBasicReport, RK_NO_ARGS, "TMR",
                          tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmBasicWorker(VOID *args)
{
    RK_UNUSEARGS

    for (UINT i = 0U; i < 1024U; i++)
    {
        tmBasicArray[i] = 0UL;
    }

    while (1)
    {
        for (UINT i = 0U; i < 1024U; i++)
        {
            tmBasicArray[i] =
                (tmBasicArray[i] + tmBasicCounter) ^ tmBasicArray[i];
        }

        tmBasicCounter++;
    }
}

VOID TmBasicReport(VOID *args)
{
    ULONG lastCounter = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const counter = tmBasicCounter;
        if (counter == lastCounter)
        {
            printf("TM FAIL %s worker counter did not move\r\n",
                   TM_BENCH_NAME);
            tmErrors++;
        }

        TmReportSimple_(relativeMs, counter - lastCounter, counter);
        lastCounter = counter;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_COOPERATIVE)
RK_DECLARE_TASK(tmTask0Handle, TmCoop0, tmTask0Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask1Handle, TmCoop1, tmTask1Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask2Handle, TmCoop2, tmTask2Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask3Handle, TmCoop3, tmTask3Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask4Handle, TmCoop4, tmTask4Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmCoopReport, tmReportStack, TM_STACKSIZE)

static volatile ULONG tmCoopCounter0;
static volatile ULONG tmCoopCounter1;
static volatile ULONG tmCoopCounter2;
static volatile ULONG tmCoopCounter3;
static volatile ULONG tmCoopCounter4;

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kTaskInit(&tmTask0Handle, TmCoop0, RK_NO_ARGS, "TMC0",
                          tmTask0Stack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "task 0");
    TmCheckErr_(kTaskInit(&tmTask1Handle, TmCoop1, RK_NO_ARGS, "TMC1",
                          tmTask1Stack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "task 1");
    TmCheckErr_(kTaskInit(&tmTask2Handle, TmCoop2, RK_NO_ARGS, "TMC2",
                          tmTask2Stack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "task 2");
    TmCheckErr_(kTaskInit(&tmTask3Handle, TmCoop3, RK_NO_ARGS, "TMC3",
                          tmTask3Stack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "task 3");
    TmCheckErr_(kTaskInit(&tmTask4Handle, TmCoop4, RK_NO_ARGS, "TMC4",
                          tmTask4Stack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "task 4");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmCoopReport, RK_NO_ARGS, "TMR",
                          tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmCoop0(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        tmCoopCounter0++;
        kYield();
    }
}

VOID TmCoop1(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        tmCoopCounter1++;
        kYield();
    }
}

VOID TmCoop2(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        tmCoopCounter2++;
        kYield();
    }
}

VOID TmCoop3(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        tmCoopCounter3++;
        kYield();
    }
}

VOID TmCoop4(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        tmCoopCounter4++;
        kYield();
    }
}

VOID TmCoopReport(VOID *args)
{
    ULONG lastTotal = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const c0 = tmCoopCounter0;
        ULONG const c1 = tmCoopCounter1;
        ULONG const c2 = tmCoopCounter2;
        ULONG const c3 = tmCoopCounter3;
        ULONG const c4 = tmCoopCounter4;
        ULONG const total = c0 + c1 + c2 + c3 + c4;

        TmReportFiveCounters_(relativeMs, total - lastTotal, c0, c1, c2, c3,
                              c4);
        lastTotal = total;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_PREEMPTIVE)
RK_DECLARE_TASK(tmTask0Handle, TmPreempt0, tmTask0Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask1Handle, TmPreempt1, tmTask1Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask2Handle, TmPreempt2, tmTask2Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask3Handle, TmPreempt3, tmTask3Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmTask4Handle, TmPreempt4, tmTask4Stack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmPreemptReport, tmReportStack, TM_STACKSIZE)

static volatile ULONG tmPreemptCounter0;
static volatile ULONG tmPreemptCounter1;
static volatile ULONG tmPreemptCounter2;
static volatile ULONG tmPreemptCounter3;
static volatile ULONG tmPreemptCounter4;

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kTaskInit(&tmTask0Handle, TmPreempt0, RK_NO_ARGS, "TMP0",
                          tmTask0Stack, TM_STACKSIZE, 6U, RK_PREEMPT),
                "task 0");
    TmCheckErr_(kTaskInit(&tmTask1Handle, TmPreempt1, RK_NO_ARGS, "TMP1",
                          tmTask1Stack, TM_STACKSIZE, 5U, RK_PREEMPT),
                "task 1");
    TmCheckErr_(kTaskInit(&tmTask2Handle, TmPreempt2, RK_NO_ARGS, "TMP2",
                          tmTask2Stack, TM_STACKSIZE, 4U, RK_PREEMPT),
                "task 2");
    TmCheckErr_(kTaskInit(&tmTask3Handle, TmPreempt3, RK_NO_ARGS, "TMP3",
                          tmTask3Stack, TM_STACKSIZE, 3U, RK_PREEMPT),
                "task 3");
    TmCheckErr_(kTaskInit(&tmTask4Handle, TmPreempt4, RK_NO_ARGS, "TMP4",
                          tmTask4Stack, TM_STACKSIZE, 2U, RK_PREEMPT),
                "task 4");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmPreemptReport, RK_NO_ARGS, "TMR",
                          tmReportStack, TM_STACKSIZE, 1U, RK_PREEMPT),
                "report task");
}

VOID TmPreempt0(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        TmResumeTask_(tmTask1Handle);
        tmPreemptCounter0++;
    }
}

VOID TmPreempt1(VOID *args)
{
    RK_UNUSEARGS

    TmSuspendSelf_();

    while (1)
    {
        TmResumeTask_(tmTask2Handle);
        tmPreemptCounter1++;
        TmSuspendSelf_();
    }
}

VOID TmPreempt2(VOID *args)
{
    RK_UNUSEARGS

    TmSuspendSelf_();

    while (1)
    {
        TmResumeTask_(tmTask3Handle);
        tmPreemptCounter2++;
        TmSuspendSelf_();
    }
}

VOID TmPreempt3(VOID *args)
{
    RK_UNUSEARGS

    TmSuspendSelf_();

    while (1)
    {
        TmResumeTask_(tmTask4Handle);
        tmPreemptCounter3++;
        TmSuspendSelf_();
    }
}

VOID TmPreempt4(VOID *args)
{
    RK_UNUSEARGS

    TmSuspendSelf_();

    while (1)
    {
        tmPreemptCounter4++;
        TmSuspendSelf_();
    }
}

VOID TmPreemptReport(VOID *args)
{
    ULONG lastTotal = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const c0 = tmPreemptCounter0;
        ULONG const c1 = tmPreemptCounter1;
        ULONG const c2 = tmPreemptCounter2;
        ULONG const c3 = tmPreemptCounter3;
        ULONG const c4 = tmPreemptCounter4;
        ULONG const total = c0 + c1 + c2 + c3 + c4;

        TmReportFiveCounters_(relativeMs, total - lastTotal, c0, c1, c2, c3,
                              c4);
        lastTotal = total;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT)
RK_DECLARE_TASK(tmWorkerHandle, TmInterruptWorker, tmWorkerStack,
                TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmInterruptReport, tmReportStack,
                TM_STACKSIZE)

static RK_SEMAPHORE tmInterruptSema;
static volatile ULONG tmInterruptCounter;

VOID TM_IRQ_HANDLER(void)
{
    TM_NVIC_ICPR(TM_IRQ_REG_INDEX) = TM_IRQ_BIT;
    tmIrqCount++;
    if (kSemaphorePost(&tmInterruptSema) != RK_ERR_SUCCESS)
    {
        tmIrqErrors++;
    }
}

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kSemaCountInit(&tmInterruptSema, 0U), "interrupt sema");
    TmIrqInit_();
    TmCheckErr_(kTaskInit(&tmWorkerHandle, TmInterruptWorker, RK_NO_ARGS,
                          "TMI0", tmWorkerStack, TM_STACKSIZE,
                          TM_WORKER_PRIO, RK_PREEMPT),
                "worker task");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmInterruptReport, RK_NO_ARGS,
                          "TMR", tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmInterruptWorker(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        TmCauseInterrupt_();
        TmCheckErr_(kSemaphorePend(&tmInterruptSema, RK_WAIT_FOREVER),
                    "interrupt sema pend");
        tmInterruptCounter++;
    }
}

VOID TmInterruptReport(VOID *args)
{
    ULONG lastCounter = 0UL;
    ULONG lastIrq = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const counter = tmInterruptCounter;
        ULONG const irqCounter = tmIrqCount;
        if ((counter == lastCounter) || (irqCounter == lastIrq) ||
            (tmIrqErrors != 0UL))
        {
            printf("TM FAIL %s no progress or irq error irq_errors=%lu\r\n",
                   TM_BENCH_NAME, tmIrqErrors);
            tmErrors++;
        }

        TmReportSimple_(relativeMs, counter - lastCounter, counter);
        printf("Interrupts: total=%lu period=%lu irq_errors=%lu\r\n",
               irqCounter, irqCounter - lastIrq, tmIrqErrors);
        lastCounter = counter;
        lastIrq = irqCounter;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_INTERRUPT_PREEMPTION)
RK_DECLARE_TASK(tmLowHandle, TmIrqPreemptLow, tmLowStack, TM_STACKSIZE)
RK_DECLARE_TASK(tmHighHandle, TmIrqPreemptHigh, tmHighStack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmIrqPreemptReport, tmReportStack,
                TM_STACKSIZE)

static volatile ULONG tmIrqPreemptLowCounter;
static volatile ULONG tmIrqPreemptHighCounter;

VOID TM_IRQ_HANDLER(void)
{
    TM_NVIC_ICPR(TM_IRQ_REG_INDEX) = TM_IRQ_BIT;
    tmIrqCount++;
    if (kEventSet(tmHighHandle, TM_FLAG) != RK_ERR_SUCCESS)
    {
        tmIrqErrors++;
    }
}

VOID kApplicationInit(VOID)
{
    TmIrqInit_();
    TmCheckErr_(kTaskInit(&tmLowHandle, TmIrqPreemptLow, RK_NO_ARGS, "TMIL",
                          tmLowStack, TM_STACKSIZE, 10U, RK_PREEMPT),
                "low task");
    TmCheckErr_(kTaskInit(&tmHighHandle, TmIrqPreemptHigh, RK_NO_ARGS, "TMIH",
                          tmHighStack, TM_STACKSIZE, 5U, RK_PREEMPT),
                "high task");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmIrqPreemptReport, RK_NO_ARGS,
                          "TMR", tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmIrqPreemptLow(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        TmCauseInterrupt_();
        tmIrqPreemptLowCounter++;
    }
}

VOID TmIrqPreemptHigh(VOID *args)
{
    RK_UNUSEARGS

    TmSuspendSelf_();

    while (1)
    {
        tmIrqPreemptHighCounter++;
        TmSuspendSelf_();
    }
}

VOID TmIrqPreemptReport(VOID *args)
{
    ULONG lastHigh = 0UL;
    ULONG lastLow = 0UL;
    ULONG lastIrq = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const high = tmIrqPreemptHighCounter;
        ULONG const low = tmIrqPreemptLowCounter;
        ULONG const irqCounter = tmIrqCount;
        if ((high == lastHigh) || (low == lastLow) ||
            (irqCounter == lastIrq) || (tmIrqErrors != 0UL))
        {
            printf("TM FAIL %s no progress or irq error irq_errors=%lu\r\n",
                   TM_BENCH_NAME, tmIrqErrors);
            tmErrors++;
        }

        TmReportSimple_(relativeMs, high - lastHigh, high);
        printf("Low thread: total=%lu period=%lu\r\n", low, low - lastLow);
        printf("Interrupts: total=%lu period=%lu irq_errors=%lu\r\n",
               irqCounter, irqCounter - lastIrq, tmIrqErrors);
        lastHigh = high;
        lastLow = low;
        lastIrq = irqCounter;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_MESSAGE)
RK_DECLARE_TASK(tmWorkerHandle, TmMessageWorker, tmWorkerStack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmMessageReport, tmReportStack, TM_STACKSIZE)

static RK_MESG_QUEUE tmMessageQueue;
static ULONG tmMessageQueueBuf[4] K_ALIGN(4);
static ULONG tmMessageSend[4] = {0x01020304UL, 0x11121314UL, 0x21222324UL,
                                 0x31323334UL};
static ULONG tmMessageRecv[4];
static volatile ULONG tmMessageCounter;

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kMesgQueueInit(&tmMessageQueue, tmMessageQueueBuf, 4UL, 1UL),
                "message queue");
    TmCheckErr_(kTaskInit(&tmWorkerHandle, TmMessageWorker, RK_NO_ARGS,
                          "TMM0", tmWorkerStack, TM_STACKSIZE,
                          TM_WORKER_PRIO, RK_PREEMPT),
                "worker task");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmMessageReport, RK_NO_ARGS, "TMR",
                          tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmMessageWorker(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        tmMessageSend[0] = tmMessageCounter;
        TmCheckErr_(kMesgQueueSend(&tmMessageQueue, tmMessageSend,
                                   RK_NO_WAIT),
                    "message send");
        TmCheckErr_(kMesgQueueRecv(&tmMessageQueue, tmMessageRecv,
                                   RK_NO_WAIT),
                    "message receive");
        tmMessageCounter++;
    }
}

VOID TmMessageReport(VOID *args)
{
    ULONG lastCounter = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const counter = tmMessageCounter;
        if (counter == lastCounter)
        {
            printf("TM FAIL %s worker counter did not move\r\n",
                   TM_BENCH_NAME);
            tmErrors++;
        }

        TmReportSimple_(relativeMs, counter - lastCounter, counter);
        lastCounter = counter;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_SYNCHRONIZATION)
RK_DECLARE_TASK(tmWorkerHandle, TmSyncWorker, tmWorkerStack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmSyncReport, tmReportStack, TM_STACKSIZE)

static RK_SEMAPHORE tmSyncSema;
static volatile ULONG tmSyncCounter;

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kSemaBinInit(&tmSyncSema, 1U), "sync sema");
    TmCheckErr_(kTaskInit(&tmWorkerHandle, TmSyncWorker, RK_NO_ARGS, "TMS0",
                          tmWorkerStack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "worker task");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmSyncReport, RK_NO_ARGS, "TMR",
                          tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmSyncWorker(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        TmCheckErr_(kSemaphorePend(&tmSyncSema, RK_WAIT_FOREVER),
                    "sync sema pend");
        TmCheckErr_(kSemaphorePost(&tmSyncSema), "sync sema post");
        tmSyncCounter++;
    }
}

VOID TmSyncReport(VOID *args)
{
    ULONG lastCounter = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const counter = tmSyncCounter;
        if (counter == lastCounter)
        {
            printf("TM FAIL %s worker counter did not move\r\n",
                   TM_BENCH_NAME);
            tmErrors++;
        }

        TmReportSimple_(relativeMs, counter - lastCounter, counter);
        lastCounter = counter;
        TmAfterReport_();
    }
}
#endif

#if (RK_THREAD_METRIC_BENCH == RK_TM_BENCH_MEMORY)
RK_DECLARE_TASK(tmWorkerHandle, TmMemoryWorker, tmWorkerStack, TM_STACKSIZE)
RK_DECLARE_TASK(tmReportHandle, TmMemoryReport, tmReportStack, TM_STACKSIZE)

static RK_MEM_PARTITION tmMemoryPool;
static BYTE tmMemoryPoolBuf[128] K_ALIGN(4);
static volatile ULONG tmMemoryCounter;

VOID kApplicationInit(VOID)
{
    TmCheckErr_(kMemPartitionInit(&tmMemoryPool, tmMemoryPoolBuf, 128UL, 1UL),
                "memory pool");
    TmCheckErr_(kTaskInit(&tmWorkerHandle, TmMemoryWorker, RK_NO_ARGS, "TMA0",
                          tmWorkerStack, TM_STACKSIZE, TM_WORKER_PRIO,
                          RK_PREEMPT),
                "worker task");
    TmCheckErr_(kTaskInit(&tmReportHandle, TmMemoryReport, RK_NO_ARGS, "TMR",
                          tmReportStack, TM_STACKSIZE, TM_REPORT_PRIO,
                          RK_PREEMPT),
                "report task");
}

VOID TmMemoryWorker(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        VOID *const blockPtr = kMemPartitionAlloc(&tmMemoryPool);

        if (blockPtr == NULL)
        {
            printf("TM ERR %s memory allocation returned NULL\r\n",
                   TM_BENCH_NAME);
            tmErrors++;
            TmStopFault_();
        }
        TmCheckErr_(kMemPartitionFree(&tmMemoryPool, blockPtr),
                    "memory free");
        tmMemoryCounter++;
    }
}

VOID TmMemoryReport(VOID *args)
{
    ULONG lastCounter = 0UL;
    ULONG relativeMs = 0UL;

    RK_UNUSEARGS

    while (1)
    {
        kSleep(TmDurationTicks_());
        relativeMs += RK_THREAD_METRIC_TEST_DURATION_MS;

        ULONG const counter = tmMemoryCounter;
        if (counter == lastCounter)
        {
            printf("TM FAIL %s worker counter did not move\r\n",
                   TM_BENCH_NAME);
            tmErrors++;
        }

        TmReportSimple_(relativeMs, counter - lastCounter, counter);
        lastCounter = counter;
        TmAfterReport_();
    }
}
#endif
