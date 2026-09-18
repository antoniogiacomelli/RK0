/* SPDX-License-Identifier: Apache-2.0 */
/******************************************************************************/
/*                                                                            */
/* RK0 - The Embedded Real-Time Kernel '0'                                    */
/* VERSION: V0.82.0                                                          */
/* (C) 2026 Antonio Giacomelli <dev@kernel0.org>                              */
/*                                                                            */
/******************************************************************************/

/*
 * Release-build regression for kMesgAlloc(). The output pointer must be
 * cleared before an exhausted pool blocks, and a blocking allocation must be
 * rejected before the scheduler has selected RK_gRunPtr.
 */

#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 192U
#define CONTROL_PRIO 2U
#define HELPER1_PRIO 10U
#define HELPER2_PRIO 11U
#define HELPER3_PRIO 12U
#define ALLOC_TIMEOUT_TICKS RK_MS_TO_TICKS(20)

typedef struct MesgAllocPayload
{
    ULONG value;
} MesgAllocPayload;

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(helper1Handle, HelperTask, helper1Stack, STACKSIZE)
RK_DECLARE_TASK(helper2Handle, HelperTask, helper2Stack, STACKSIZE)
RK_DECLARE_TASK(helper3Handle, HelperTask, helper3Stack, STACKSIZE)

RK_DECLARE_MESG_POOL(preDispatchPool, preDispatchPoolBuf, MesgAllocPayload, 1U)
RK_DECLARE_MESG_POOL(timeoutPool, timeoutPoolBuf, MesgAllocPayload, 1U)

static RK_MESG *preDispatchHeld;
static RK_MESG *preDispatchOut;
static RK_ERR preDispatchErr;

static VOID Stop_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

static VOID Fail_(CHAR const *const wherePtr)
{
    printf("MA FAIL %s\r\n", wherePtr);
    Stop_();
}

static VOID Check_(RK_BOOL const condition, CHAR const *const wherePtr)
{
    if (condition == RK_FALSE)
    {
        Fail_(wherePtr);
    }
}

static VOID InitRequire_(RK_ERR const err)
{
    if (err != RK_ERR_SUCCESS)
    {
        while (1)
        {
        }
    }
}

int main(void)
{
    kCoreInit();
    kInit();

    while (1)
    {
    }
}

VOID kApplicationInit(VOID)
{
    InitRequire_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "MActrl",
                           controlStack, STACKSIZE, CONTROL_PRIO, RK_PREEMPT));
    InitRequire_(kTaskInit(&helper1Handle, HelperTask, RK_NO_ARGS, "MAh1",
                           helper1Stack, STACKSIZE, HELPER1_PRIO, RK_PREEMPT));
    InitRequire_(kTaskInit(&helper2Handle, HelperTask, RK_NO_ARGS, "MAh2",
                           helper2Stack, STACKSIZE, HELPER2_PRIO, RK_PREEMPT));
    InitRequire_(kTaskInit(&helper3Handle, HelperTask, RK_NO_ARGS, "MAh3",
                           helper3Stack, STACKSIZE, HELPER3_PRIO, RK_PREEMPT));

    InitRequire_(kMesgPoolInit(&preDispatchPool, preDispatchPoolBuf,
                               sizeof(MesgAllocPayload), 1U,
                               RK_MESG_PRIO_CEILING_NONE));
    InitRequire_(kMesgPoolInit(&timeoutPool, timeoutPoolBuf,
                               sizeof(MesgAllocPayload), 1U,
                               RK_MESG_PRIO_CEILING_NONE));

    InitRequire_(kMesgAlloc(&preDispatchPool, &preDispatchHeld, RK_NO_WAIT));
    preDispatchOut = preDispatchHeld;
    preDispatchErr = kMesgAlloc(&preDispatchPool, &preDispatchOut,
                                ALLOC_TIMEOUT_TICKS);
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS

    Check_((RK_BOOL)(preDispatchErr == RK_ERR_INVALID_ISR_PRIMITIVE),
           "pre-dispatch blocking result");
    Check_((RK_BOOL)(preDispatchOut == NULL), "pre-dispatch output clear");
    Check_((RK_BOOL)((preDispatchHeld != NULL) &&
                     (preDispatchHeld->owner == NULL) &&
                     (preDispatchPool.nFreeBlocks == 0UL)),
           "pre-dispatch ownership");
    Check_((RK_BOOL)(kMesgFree(preDispatchHeld) == RK_ERR_SUCCESS),
           "pre-dispatch free");
    Check_((RK_BOOL)(preDispatchPool.nFreeBlocks == 1UL),
           "pre-dispatch free count");

    RK_MESG *heldPtr = NULL;
    Check_((RK_BOOL)(kMesgAlloc(&timeoutPool, &heldPtr, RK_NO_WAIT) ==
                     RK_ERR_SUCCESS),
           "initial allocation");
    Check_((RK_BOOL)((heldPtr != NULL) && (heldPtr->owner == controlHandle) &&
                     (timeoutPool.nFreeBlocks == 0UL)),
           "initial ownership");

    RK_TASK_HANDLE const ownerBefore = heldPtr->owner;
    RK_MESG *timedOutPtr = heldPtr;
    RK_ERR const err =
        kMesgAlloc(&timeoutPool, &timedOutPtr, ALLOC_TIMEOUT_TICKS);

    Check_((RK_BOOL)(err == RK_ERR_TIMEOUT), "finite timeout result");
    Check_((RK_BOOL)(timedOutPtr == NULL), "finite timeout output clear");
    Check_((RK_BOOL)((heldPtr->owner == ownerBefore) &&
                     (heldPtr->state == RK_MESG_STATE_ALLOCATED) &&
                     (timeoutPool.nFreeBlocks == 0UL)),
           "finite timeout ownership");

    Check_((RK_BOOL)(kMesgFree(heldPtr) == RK_ERR_SUCCESS), "held free");
    Check_((RK_BOOL)(timeoutPool.nFreeBlocks == 1UL), "held free count");

    printf("MA PASS release message allocation timeout\r\n");
    Stop_();
}

VOID HelperTask(VOID *args)
{
    RK_UNUSEARGS
    Stop_();
}
