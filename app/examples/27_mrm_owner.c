/* SPDX-License-Identifier: Apache-2.0 */
/* A static MRM keeps its initialising task as owner. A dynamic MRM keeps its
 * creating task as owner through destruction. Debug checks reject another
 * task's publish/destroy without changing buffers, payloads, or handles.
 * Readers remain able to get/unget from another task. */
#include <kapi.h>
#include <stdio.h>

#define STACKSIZE 256U
#define N_BUFS 3U
#define OLD_VALUE 0x11111111UL
#define GOOD_VALUE 0x22222222UL
#define FOREIGN_VALUE 0xBAD0BAD0UL
#define RESERVED_VALUE 0xCAFEBABEUL
#define REQUEST_PUBLISH 1U
#define REQUEST_DESTROY 2U

RK_DECLARE_TASK(ownerHandle, OwnerTask, ownerStack, STACKSIZE)
RK_DECLARE_TASK(readerHandle, ReaderTask, readerStack, STACKSIZE)

static RK_MRM staticMrm;
static RK_MRM_BUF staticBuffers[N_BUFS];
static ULONG staticPayloads[N_BUFS];
static RK_MRM_HANDLE dynamicMrm;
static RK_MRM_BUF dynamicBuffers[N_BUFS];
static ULONG dynamicPayloads[N_BUFS];
static RK_SEMAPHORE requestGate;
static RK_SEMAPHORE doneGate;
static RK_MRM *target;
static RK_MRM_BUF *candidate;
static RK_MRM_BUF *previous;
static volatile UINT request;

static VOID Check_(RK_BOOL const ok, CHAR const *const wherePtr)
{
    if (ok == RK_FALSE)
    {
        printf("MO FAIL %s\r\n", wherePtr);
        while (1)
            (void)kSleep(RK_MS_TO_TICKS(1000UL));
    }
}

static VOID CheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
        printf("MO ERR %s err=%d\r\n", wherePtr, err);
    Check_((RK_BOOL)(err == RK_ERR_SUCCESS), wherePtr);
}

static VOID AskReader_(UINT const operation)
{
    request = operation;
    CheckErr_(kSemaphorePost(&requestGate), "request reader");
    CheckErr_(kSemaphorePend(&doneGate, RK_WAIT_FOREVER), "reader completed");
    Check_((RK_BOOL)(target->publisherHandle == ownerHandle), "owner unchanged");
}

static VOID CheckPublication_(RK_MRM *const mrm, ULONG const expected)
{
    ULONG received = 0UL;
    RK_MRM_BUF *const readPtr = kMRMGet(mrm, &received);
    Check_((RK_BOOL)((readPtr != NULL) && (received == expected)), "reader payload");
    CheckErr_(kMRMUnget(mrm, readPtr), "reader unget");
}

static VOID RunPublishCase_(RK_MRM *const mrm)
{
    Check_((RK_BOOL)(mrm->publisherHandle == ownerHandle), "initial owner recorded");
    RK_MRM_BUF *const first = kMRMReserve(mrm);
    Check_((RK_BOOL)(first != NULL), "owner reserve first");
    ULONG const oldValue = OLD_VALUE;
    CheckErr_(kMRMPublish(mrm, first, &oldValue), "owner publish first");

    /* Keep an old reader pinned so the new reservation is a distinct buffer. */
    ULONG received = 0UL;
    previous = kMRMGet(mrm, &received);
    Check_((RK_BOOL)((previous == first) && (received == OLD_VALUE)), "pin old version");
    candidate = kMRMReserve(mrm);
    Check_((RK_BOOL)((candidate != NULL) && (candidate != previous)), "owner reserve next");
    *(ULONG *)candidate->mrmData = RESERVED_VALUE;
    target = mrm;
    AskReader_(REQUEST_PUBLISH);
    Check_((RK_BOOL)(*(ULONG *)previous->mrmData == OLD_VALUE), "pinned version intact");

#if (RK_CONF_ERR_CHECK != ON)
    /* The unchecked foreign publish consumed the reservation. */
    candidate = kMRMReserve(mrm);
    Check_((RK_BOOL)(candidate != NULL), "owner reserve after unchecked publish");
#endif
    ULONG const goodValue = GOOD_VALUE;
    CheckErr_(kMRMPublish(mrm, candidate, &goodValue), "owner publish after reader");
    CheckPublication_(mrm, GOOD_VALUE);
    CheckErr_(kMRMUnget(mrm, previous), "release pinned version");
    Check_((RK_BOOL)((mrm->mrmMem.nFreeBlocks == N_BUFS - 1UL) &&
                     (mrm->mrmDataMem.nFreeBlocks == N_BUFS - 1UL)), "pool accounting intact");
}

int main(void)
{
    kCoreInit();
    kInit();
    Check_(RK_FALSE, "kernel returned");
}

VOID kApplicationInit(VOID)
{
    CheckErr_(kTaskInit(&ownerHandle, OwnerTask, RK_NO_ARGS, "Owner", ownerStack,
                        STACKSIZE, 1U, RK_PREEMPT), "task Owner");
    CheckErr_(kTaskInit(&readerHandle, ReaderTask, RK_NO_ARGS, "Reader", readerStack,
                        STACKSIZE, 2U, RK_PREEMPT), "task Reader");
    CheckErr_(kSemaBinInit(&requestGate, 0U, RK_WAIT_PRIORITY), "request gate");
    CheckErr_(kSemaBinInit(&doneGate, 0U, RK_WAIT_PRIORITY), "done gate");
    CheckErr_(kObjPartitionsInit(), "object partitions");
}

VOID OwnerTask(VOID *args)
{
    RK_UNUSEARGS
    CheckErr_(kMRMInit(&staticMrm, staticBuffers, staticPayloads, N_BUFS, 1UL),
               "static MRM init by owner");
    RunPublishCase_(&staticMrm);
    printf("MO static owner and reader PASS\r\n");

    CheckErr_(kMRMCreate(&dynamicMrm, dynamicBuffers, dynamicPayloads, N_BUFS, 1UL),
               "dynamic MRM create by owner");
    RunPublishCase_(dynamicMrm);
#if (RK_CONF_ERR_CHECK == ON)
    AskReader_(REQUEST_DESTROY);
    Check_((RK_BOOL)((dynamicMrm == target) && (target->init == RK_TRUE)),
           "foreign destroy preserves object");
    CheckPublication_(dynamicMrm, GOOD_VALUE);
#endif
    CheckErr_(kMRMDestroy(&dynamicMrm), "owner destroy");
    Check_((RK_BOOL)(dynamicMrm == NULL), "owner handle cleared");
    printf("MO dynamic lifecycle PASS\r\n");
    printf("MO PASS fixed MRM owner lifecycle\r\n");
    while (1)
        (void)kSleep(RK_MS_TO_TICKS(1000UL));
}

VOID ReaderTask(VOID *args)
{
    RK_UNUSEARGS
    while (1)
    {
        CheckErr_(kSemaphorePend(&requestGate, RK_WAIT_FOREVER), "reader request");
        if (request == REQUEST_PUBLISH)
        {
#if (RK_CONF_ERR_CHECK == ON)
            ULONG const freeBlocks = target->mrmMem.nFreeBlocks;
            Check_((RK_BOOL)(kMRMReserve(target) == NULL), "foreign reserve rejected");
            Check_((RK_BOOL)(target->mrmMem.nFreeBlocks == freeBlocks), "reserve leaves pool intact");
            if (target == &staticMrm)
                Check_((RK_BOOL)(kMRMInit(target, staticBuffers, staticPayloads,
                                         N_BUFS, 1UL) == RK_ERR_OBJ_DOUBLE_INIT),
                       "static owner cannot be rebound");
#endif
            ULONG const foreignValue = FOREIGN_VALUE;
            RK_ERR const err = kMRMPublish(target, candidate, &foreignValue);
#if (RK_CONF_ERR_CHECK == ON)
            Check_((RK_BOOL)(err == RK_ERR_INVALID_OWNER), "foreign publish rejected");
            Check_((RK_BOOL)((target->currBufPtr == previous) &&
                             (candidate->reserved == RK_TRUE) &&
                             (*(ULONG *)candidate->mrmData == RESERVED_VALUE)),
                   "rejected publish changes no buffer or payload");
            CheckPublication_(target, OLD_VALUE);
#else
            CheckErr_(err, "unchecked publish");
            CheckPublication_(target, FOREIGN_VALUE);
#endif
        }
#if (RK_CONF_ERR_CHECK == ON)
        else if (request == REQUEST_DESTROY)
        {
            RK_MRM_HANDLE handle = target;
            Check_((RK_BOOL)(kMRMDestroy(&handle) == RK_ERR_INVALID_OWNER),
                   "foreign destroy rejected");
            Check_((RK_BOOL)(handle == target), "foreign handle preserved");
        }
#endif
        else
            Check_(RK_FALSE, "unknown reader request");
        CheckErr_(kSemaphorePost(&doneGate), "reader done");
    }
}
