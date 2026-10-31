/* SPDX-License-Identifier: Apache-2.0 */
/* Sleep Release is restricted to statically initialised tasks. */
#include <kapi.h>
#include <stdio.h>
#include <string.h>

#define STACKSIZE 256U
#define PERIOD 5UL
#define DONE RK_EVENT_1

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(staticHandle, StaticTask, staticStack, STACKSIZE)
RK_DECLARE_DYNAMIC_TASK(dynamicHandle, DynamicTask)
RK_DECLARE_DYNAMIC_STACK_POOL(stackPool, stackStorage, 1U, STACKSIZE)

static RK_DYNAMIC_TASK_ATTR attributes = {
    DynamicTask, RK_NO_ARGS, "SRdyn", 2U, RK_PREEMPT, &stackPool
};

static VOID Stop_(VOID)
{
    while (1)
        (void)kSleep(1000UL);
}

static VOID Check_(RK_BOOL const ok, CHAR const *const where)
{
    if (ok == RK_FALSE)
    {
        printf("SR FAIL %s\r\n", where);
        Stop_();
    }
}

static VOID Expect_(RK_ERR const actual, RK_ERR const expected,
                     CHAR const *const where)
{
    if (actual != expected)
    {
        printf("SR FAIL %s expected=%d actual=%d\r\n", where, expected, actual);
        Stop_();
    }
}

int main(void)
{
    kCoreInit();
    kInit();
    return (0);
}

VOID kApplicationInit(VOID)
{
    Expect_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "SRctrl",
                      controlStack, STACKSIZE, 8U, RK_PREEMPT),
             RK_ERR_SUCCESS, "controller init");
    Expect_(kMemPartitionInit(&stackPool, stackStorage,
                              sizeof(stackStorage[0]), 1UL),
             RK_ERR_SUCCESS, "stack partition init");
    /* Creation method determines the restriction, even before dispatch starts. */
    Expect_(kTaskSpawn(&attributes, &dynamicHandle), RK_ERR_SUCCESS,
             "spawn before scheduler start");
}

static VOID WaitDynamic_(VOID)
{
    Expect_(kEventGet(DONE, RK_EVENT_ALL, NULL, 100UL), RK_ERR_SUCCESS,
             "dynamic task completed");
    Check_((RK_BOOL)(dynamicHandle->status == RK_SELF_SUSPENDED),
            "dynamic task suspended");
    Expect_(kTaskTerminate(&dynamicHandle), RK_ERR_SUCCESS, "terminate dynamic");
    Check_((RK_BOOL)((dynamicHandle == NULL) && (stackPool.nFreeBlocks == 1UL)),
            "dynamic resources reclaimed");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS
    RK_TASK_HANDLE const reusedSlot = dynamicHandle;
    WaitDynamic_();

    /* Static tasks retain the phase grid while dynamic creation is enabled. */
    Expect_(kSleepRelease(PERIOD), RK_ERR_SUCCESS, "static release");
    Expect_(kSleepPeriodic(PERIOD), RK_ERR_SUCCESS, "static periodic alias");

    /* The higher-priority task runs immediately when spawn unlocks scheduling. */
    Expect_(kTaskSpawn(&attributes, &dynamicHandle), RK_ERR_SUCCESS,
             "runtime spawn");
    Check_((RK_BOOL)(dynamicHandle == reusedSlot), "dynamic TCB reused");
    WaitDynamic_();

    /* Reusing the same TCB for a static task must clear the restriction. */
    Expect_(kTaskInit(&staticHandle, StaticTask, RK_NO_ARGS, "SRstatic",
                      staticStack, STACKSIZE, 2U, RK_PREEMPT),
             RK_ERR_SUCCESS, "static replacement init");
    Check_((RK_BOOL)(staticHandle == reusedSlot), "static TCB reused");
    Expect_(kEventGet(DONE, RK_EVENT_ALL, NULL, 100UL), RK_ERR_SUCCESS,
             "static replacement completed");
    printf("SR PASS static-only sleep release\r\n");
    Stop_();
}

VOID DynamicTask(VOID *args)
{
    RK_UNUSEARGS
    RK_TASK_HANDLE const self = kTaskGetRunningHandle();
    static RK_TCB before;
    RK_CR_AREA

    for (UINT alias = 0U; alias < 2U; alias++)
    {
        RK_CR_ENTER
        memcpy(&before, self, sizeof(before));
        RK_ERR const err = (alias == 0U) ? kSleepRelease(PERIOD)
                                         : kSleepPeriodic(PERIOD);
        RK_BOOL const preserved =
            (RK_BOOL)(memcmp(&before, self, sizeof(before)) == 0);
        RK_CR_EXIT
        Expect_(err, RK_ERR_INVALID_OBJ, "dynamic release rejected");
        Check_(preserved, "rejection preserves phase, overruns, and timeout state");
    }

    Expect_(kSleepDelay(1UL), RK_ERR_SUCCESS, "dynamic relative sleep");
    RK_TICK anchor = kTickGet();
    Expect_(kSleepUntil(&anchor, PERIOD), RK_ERR_SUCCESS,
             "dynamic locally anchored sleep");
    Expect_(kEventSet(controlHandle, DONE), RK_ERR_SUCCESS, "signal completion");
    (void)kTaskSelfSuspend();
    Stop_();
}

VOID StaticTask(VOID *args)
{
    RK_UNUSEARGS
    Expect_(kSleepRelease(PERIOD), RK_ERR_SUCCESS, "reused static release");
    Expect_(kSleepPeriodic(PERIOD), RK_ERR_SUCCESS, "reused static alias");
    Expect_(kEventSet(controlHandle, DONE), RK_ERR_SUCCESS, "static completion");
    (void)kTaskSelfSuspend();
    Stop_();
}
