/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RK_EXPERIMENT_BENCH_H
#define RK_EXPERIMENT_BENCH_H

#include <kapi.h>
#include <stdio.h>

#ifndef EXP_SAMPLES
#define EXP_SAMPLES 5U
#endif
#ifndef EXP_WORK
#define EXP_WORK 4000UL
#endif
#ifndef EXP_INTERFERENCE
#define EXP_INTERFERENCE 40000UL
#endif
#ifndef EXP_ITERATIONS
#define EXP_ITERATIONS 256U
#endif
#define EXP_STACK 192U
#define EXP_GO RK_EVENT_1
#define EXP_LIMIT RK_MS_TO_TICKS(2000)

static inline VOID BenchCheck_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("EXP FAIL %s\r\n", where);
        for (;;) { kSleep(1UL); }
    }
}

static inline VOID BenchOK_(RK_ERR const err, CHAR const *const where)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("EXP FAIL %s err=%d\r\n", where, err);
        for (;;) { kSleep(1UL); }
    }
}

static inline VOID BenchWait_(RK_TASK_EVENT const flags)
{
    BenchOK_(kEventGet(flags, RK_EVENT_ALL, NULL, EXP_LIMIT), "event wait");
}

static inline VOID BenchGo_(VOID)
{
    BenchOK_(kEventGet(EXP_GO, RK_EVENT_ALL, NULL, RK_WAIT_FOREVER), "start wait");
}

static inline VOID BenchSignal_(RK_TASK_HANDLE const task,
                                RK_TASK_EVENT const flags)
{
    BenchOK_(kEventSet(task, flags), "event signal");
}

/* Read the existing CPU-clocked SysTick; do not reconfigure any timer.
 * PRIMASK makes tick/VAL consistent. Account for a pending, unserviced tick.
 * Unsigned subtraction is valid for intervals shorter than one 32-bit wrap.
 */
static inline ULONG BenchNow_(VOID)
{
    RK_CR_AREA
    RK_CR_ENTER
    ULONG tick = kTickGet();
    ULONG const reload = RK_REG_SYSTICK_LOAD;
    ULONG value = RK_REG_SYSTICK_VAL;
    if ((RK_REG_SCB_ICSR & (1UL << 26)) != 0UL)
    {
        tick++;
        value = RK_REG_SYSTICK_VAL;
    }
    ULONG const result = tick * (reload + 1UL) + (reload - value);
    RK_CR_EXIT
    return (result);
}

static inline VOID BenchWork_(ULONG const count)
{
    for (ULONG i = 0UL; i < count; i++) { RK_NOP }
}

static inline VOID BenchHeader_(CHAR const *const experiment)
{
    ULONG overhead = RK_ULONG_MAX;
    for (UINT i = 0U; i < 32U; i++)
    {
        ULONG const start = BenchNow_();
        ULONG const elapsed = BenchNow_() - start;
        if (elapsed < overhead) { overhead = elapsed; }
    }
    printf("EXP META experiment=%s clock_hz=%lu tick_hz=%lu "
           "timer=SysTick unit=clock_counts timer_pair_min=%lu "
           "samples=%u work=%lu interference=%lu\r\n",
           experiment, RK_gSysCoreClock, (ULONG)RK_CONF_SYSTICK_DIV,
           overhead, (UINT)EXP_SAMPLES, (ULONG)EXP_WORK,
           (ULONG)EXP_INTERFERENCE);
}

static inline VOID BenchStop_(VOID)
{
    for (;;) { kSleep(RK_MS_TO_TICKS(1000)); }
}

int main(void)
{
    kCoreInit();
    kInit();
    BenchCheck_(RK_FALSE, "scheduler returned");
    return (1);
}
#endif
