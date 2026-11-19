/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_RESOURCE_BENCH_COMMON_H
#define RK_RESOURCE_BENCH_COMMON_H

#include <kapi.h>
#include <ksch.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(RK_MCU_F103RB)
#error "Resource-sharing cycle measurements require PLATFORM=stm32f103rb"
#endif

#ifndef RS_SAMPLES
#define RS_SAMPLES 5U
#endif
#ifndef RS_BATCH
#define RS_BATCH 256U
#endif
#ifndef RS_WORK
#define RS_WORK 4000U
#endif
#ifndef RS_MEDIUM_WORK
#define RS_MEDIUM_WORK 40000U
#endif
#ifndef RS_REVISION
#define RS_REVISION "unknown"
#endif
#ifndef RS_BUILD_NAME
#define RS_BUILD_NAME "unspecified"
#endif
#ifndef RS_OPT_NAME
#define RS_OPT_NAME "unspecified"
#endif
#if (RS_SAMPLES < 1) || (RS_BATCH < 1) || (RS_WORK < 1) || (RS_MEDIUM_WORK < 1)
#error "Sample count, batch length and workloads must be positive"
#endif

#define RS_STACK 256U
#define RS_SETUP_TIMEOUT RK_MS_TO_TICKS(2000UL)
#define RS_WAIT_TIMEOUT RK_MS_TO_TICKS(50UL)
#define RS_REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define RS_DEMCR RS_REG32(0xE000EDFCUL)
#define RS_DWT_CTRL RS_REG32(0xE0001000UL)
#define RS_DWT_CYCCNT RS_REG32(0xE0001004UL)

static char const *rsExperiment;
static uint32_t rsReadPairMin;

static inline uint32_t BenchNow(void)
{
    uint32_t value;
    __asm__ volatile ("" ::: "memory");
    value = RS_DWT_CYCCNT;
    __asm__ volatile ("" ::: "memory");
    return value;
}

/* Correct across one wrap, provided the true interval is less than 2^32 cycles. */
static inline uint32_t BenchDelta(uint32_t start, uint32_t end)
{
    return (uint32_t)(end - start);
}

static inline void BenchBurn(uint32_t iterations)
{
    for (uint32_t i = 0U; i < iterations; ++i)
    {
        __asm__ volatile ("nop" ::: "memory");
    }
}

static inline void BenchFail(char const *where)
{
    kSchLock();
    printf("RS FAIL experiment=%s reason=%s\r\n", rsExperiment, where);
    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

static inline void BenchCheck(int condition, char const *where)
{
    if (condition == 0)
    {
        BenchFail(where);
    }
}

static inline void BenchRequire(RK_ERR err, char const *where)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("RS ERROR code=%d\r\n", (int)err);
        BenchFail(where);
    }
}

static inline void BenchWait(RK_TASK_EVENT flags)
{
    BenchRequire(kEventGet(flags, RK_EVENT_ALL, NULL, RS_SETUP_TIMEOUT),
                 "event handshake timeout");
}

static inline void BenchSignal(RK_TASK_HANDLE task, RK_TASK_EVENT flags)
{
    BenchRequire(kEventSet(task, flags), "event signal");
}

static inline void BenchEmit(char const *scenario, unsigned sample,
                             char const *metric, uint32_t cycles,
                             unsigned operations)
{
    /* Sample zero is an unreported warm-up, with all assertions still active. */
    if (sample != 0U)
    {
        printf("RS DATA %s,%s,%u,%s,%lu,%u\r\n", rsExperiment, scenario,
               sample, metric, (unsigned long)cycles, operations);
    }
}

static inline void BenchFinish(void)
{
    printf("RS PASS experiment=%s\r\n", rsExperiment);
    for (;;)
    {
        (void)kTaskSelfSuspend();
    }
}

/* This benchmark-only task prevents WFI/sleep clock gating during intervals. */
RK_DECLARE_TASK(rsAwakeHandle, RsAwakeTask, rsAwakeStack, 128U)
VOID RsAwakeTask(VOID *args)
{
    RK_UNUSEARGS
    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

static inline void BenchKeepAwakeInit(void)
{
    BenchRequire(kTaskInit(&rsAwakeHandle, RsAwakeTask, RK_NO_ARGS, "RSawake",
                          rsAwakeStack, 128U, 30U, RK_PREEMPT), "awake task");
}

/* Call after kCoreInit(), before kInit(). Never reset CYCCNT during a run. */
static inline void BenchInit(char const *experiment)
{
    rsExperiment = experiment;
    RS_DEMCR |= (1UL << 24);
    __asm__ volatile ("dsb\nisb" ::: "memory");
    BenchCheck((RS_DWT_CTRL & (1UL << 25)) == 0UL, "DWT NOCYCCNT");
    RS_DWT_CTRL &= ~1UL;
    RS_DWT_CYCCNT = 0U;
    RS_DWT_CTRL |= 1UL;
    __asm__ volatile ("dsb\nisb" ::: "memory");
    BenchCheck((RS_DWT_CTRL & 1UL) != 0UL, "DWT enable did not latch");
    uint32_t start = BenchNow();
    BenchBurn(64U);
    BenchCheck(BenchDelta(start, BenchNow()) != 0U, "DWT counter did not advance");
    rsReadPairMin = UINT32_MAX;
    for (unsigned i = 0U; i < 256U; ++i)
    {
        uint32_t begin = BenchNow();
        uint32_t elapsed = BenchDelta(begin, BenchNow());
        if (elapsed < rsReadPairMin)
        {
            rsReadPairMin = elapsed;
        }
    }
    printf("RS META schema=1 experiment=%s board=NUCLEO-F103RB "
           "timer=DWT_CYCCNT unit=cpu_cycles core_hz=%lu tick_hz=%lu "
           "samples=%u warmups=1 batch=%u work=%u medium_work=%u "
           "read_pair_min=%lu idle_policy=busy irq_policy=enabled "
           "overhead_subtracted=0 revision=%s compiler=%s\r\n",
           experiment, (unsigned long)RK_gSysCoreClock,
           (unsigned long)RK_CONF_SYSTICK_DIV, (unsigned)RS_SAMPLES,
           (unsigned)RS_BATCH, (unsigned)RS_WORK, (unsigned)RS_MEDIUM_WORK,
           (unsigned long)rsReadPairMin, RS_REVISION, __VERSION__);
    printf("RS CONFIG build=%s optimisation=%s error_checks=%u trace=%u "
           "rcc_cfgr=0x%08lx flash_acr=0x%08lx "
           "timestamp_barrier=compiler timeout_ms=50 setup_timeout_ms=2000\r\n",
           RS_BUILD_NAME, RS_OPT_NAME, (unsigned)RK_CONF_ERR_CHECK,
           (unsigned)RK_CONF_TRACE, (unsigned long)RS_REG32(0x40021004UL),
           (unsigned long)RS_REG32(0x40022000UL));
    printf("RS CSV experiment,case,sample,metric,cycles,operations\r\n");
}

#endif
