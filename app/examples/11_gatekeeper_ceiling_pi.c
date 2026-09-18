
/*
 * Cross-task composition of an asynchronous-message ceiling and mutex PI.
 *
 * G (nominal 8) owns a message from a ceiling-3 pool, then blocks on a PI
 * mutex owned by O (nominal 10). O must inherit G's effective priority 3,
 * although O owns no message. A priority-1 controller observes the actual
 * blocked dependency before allowing it to end.
 *
 * Phase 1: O unlocks; G acquires the mutex. O returns to 10, G stays at 3.
 * Phase 2: G's mutex wait times out. O still owns the mutex but returns to 10;
 *          G still owns the message and stays at 3.
 * In both phases, G returns to 8 only after freeing its message.
 *
 * This models the resource-using interval of a gatekeeper. It checks priority
 * propagation and restoration, not a complete request/reply service or a
 * response-time bound. Polling sleeps only coordinate the test tasks.
 */


#define RK_CEIL_TEST

#ifdef RK_CEIL_TEST

#include <kapi.h>
#include <ksch.h>
#include <stdio.h>

#define STACKSIZE 192U
#define CONTROL_PRIO 1U
#define CEILING_PRIO 3U
#define GATEKEEPER_PRIO 8U
#define OWNER_PRIO 10U
#define HANDOFF_PHASE 1U
#define TIMEOUT_PHASE 2U
#define MUTEX_TIMEOUT_TICKS RK_MS_TO_TICKS(200)
#define SYNC_LIMIT_TICKS RK_MS_TO_TICKS(1500)

typedef struct GatekeeperPayload
{
    ULONG value;
} GatekeeperPayload;

RK_DECLARE_TASK(controlHandle, ControlTask, controlStack, STACKSIZE)
RK_DECLARE_TASK(gatekeeperHandle, GatekeeperTask, gatekeeperStack, STACKSIZE)
RK_DECLARE_TASK(ownerHandle, OwnerTask, ownerStack, STACKSIZE)
RK_DECLARE_MESG_POOL(messagePool, messagePoolBuf, GatekeeperPayload, 1U)

static RK_MUTEX resourceMutex;
static RK_MESG *volatile gateMessage;
static volatile UINT ownerStart;
static volatile UINT ownerLocked;
static volatile UINT ownerRelease;
static volatile UINT ownerDone;
static volatile UINT gateStart;
static volatile UINT gateReturned;
static volatile UINT gateFree;
static volatile UINT gateDone;

static VOID Fail_(CHAR const *const wherePtr)
{
    printf("GC FAIL %s\r\n", wherePtr);
    K_ASSERT(0);
    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

static VOID Check_(RK_BOOL const condition, CHAR const *const wherePtr)
{
    if (condition == RK_FALSE)
    {
        Fail_(wherePtr);
    }
}

static VOID CheckErr_(RK_ERR const err, CHAR const *const wherePtr)
{
    if (err != RK_ERR_SUCCESS)
    {
        printf("GC ERR %s err=%d\r\n", wherePtr, err);
        Fail_(wherePtr);
    }
}

static VOID ExpectPrio_(RK_TASK_HANDLE const task, RK_PRIO const expected,
                        CHAR const *const wherePtr)
{
    if (RK_TASK_PRIO(task) != expected)
    {
        printf("GC PRIO %s exp=%u got=%u\r\n", wherePtr, (UINT)expected,
               (UINT)RK_TASK_PRIO(task));
        Fail_(wherePtr);
    }
}

static VOID WaitPhase_(volatile UINT const *const flagPtr, UINT const phase,
                       CHAR const *const wherePtr)
{
    for (ULONG tick = 0UL; tick < SYNC_LIMIT_TICKS; tick++)
    {
        if (*flagPtr == phase)
        {
            return;
        }
        kSleep(1UL);
    }
    Fail_(wherePtr);
}

static VOID WaitDependency_(VOID)
{
    for (ULONG tick = 0UL; tick < SYNC_LIMIT_TICKS; tick++)
    {
        RK_BOOL waiting;
        RK_CR_AREA
        RK_CR_ENTER
        waiting = (RK_BOOL)((gatekeeperHandle->status == RK_BLOCKED) &&
                            (gatekeeperHandle->waitingForMutexPtr ==
                             &resourceMutex) &&
                            (resourceMutex.waitingQueue.size == 1UL) &&
                            (kTCBQPeek(&resourceMutex.waitingQueue) ==
                             gatekeeperHandle));
        RK_CR_EXIT
        if (waiting == RK_TRUE)
        {
            return;
        }
        kSleep(1UL);
    }
    Fail_("gatekeeper did not block on owner's mutex");
}

static VOID Stop_(VOID)
{
    while (1)
    {
        kSleep(RK_MS_TO_TICKS(1000));
    }
}

int main(void)
{
    kCoreInit();
    kInit();
    Fail_("scheduler returned");
}

VOID kApplicationInit(VOID)
{
    CheckErr_(kTaskInit(&controlHandle, ControlTask, RK_NO_ARGS, "GCctrl",
                        controlStack, STACKSIZE, CONTROL_PRIO, RK_PREEMPT),
              "controller init");
    CheckErr_(kTaskInit(&gatekeeperHandle, GatekeeperTask, RK_NO_ARGS, "GCgate",
                        gatekeeperStack, STACKSIZE, GATEKEEPER_PRIO, RK_PREEMPT),
              "gatekeeper init");
    CheckErr_(kTaskInit(&ownerHandle, OwnerTask, RK_NO_ARGS, "GCowner",
                        ownerStack, STACKSIZE, OWNER_PRIO, RK_PREEMPT),
              "owner init");
    CheckErr_(kMutexInit(&resourceMutex, RK_PRIO_INHERITANCE), "mutex init");
    CheckErr_(kMesgPoolInit(&messagePool, messagePoolBuf,
                            sizeof(GatekeeperPayload), 1U, CEILING_PRIO),
              "message pool init");
}

VOID ControlTask(VOID *args)
{
    RK_UNUSEARGS

    for (UINT phase = HANDOFF_PHASE; phase <= TIMEOUT_PHASE; phase++)
    {
        ownerStart = phase;
        /* only prints if error */
        WaitPhase_(&ownerLocked, phase, "owner lock handshake");
        ExpectPrio_(ownerHandle, OWNER_PRIO, "owner initially nominal");
        gateStart = phase;
        WaitDependency_();

        Check_((RK_BOOL)(resourceMutex.ownerPtr == ownerHandle),
               "owner still holds mutex");
        Check_((RK_BOOL)((gateMessage != NULL) &&
                         (gateMessage->owner == gatekeeperHandle)),
               "gatekeeper owns message while blocked");
        Check_((RK_BOOL)(ownerHandle->asynchMesgOwnedList.size == 0UL),
               "owner has no message ceiling of its own");
        ExpectPrio_(gatekeeperHandle, CEILING_PRIO, "blocked gatekeeper ceiling");
        ExpectPrio_(ownerHandle, CEILING_PRIO, "owner inherits ceiling via mutex");
        printf("GC phase=%u blocked: G=3 O=3\r\n", phase);

        if (phase == HANDOFF_PHASE)
        {
            ownerRelease = phase;
        }
        WaitPhase_(&gateReturned, phase, "gatekeeper mutex return");
        Check_((RK_BOOL)((resourceMutex.waitingQueue.size == 0UL) &&
                         (gatekeeperHandle->waitingForMutexPtr == NULL)),
               "mutex dependency removed");
        ExpectPrio_(ownerHandle, OWNER_PRIO, "owner restored after dependency");
        ExpectPrio_(gatekeeperHandle, CEILING_PRIO, "message ceiling survives");
        Check_((RK_BOOL)(gateMessage->owner == gatekeeperHandle),
               "message ownership survives");
        Check_((RK_BOOL)(resourceMutex.ownerPtr ==
                         ((phase == HANDOFF_PHASE) ? gatekeeperHandle
                                                   : ownerHandle)),
               "mutex ownership after handoff or timeout");
        printf("GC phase=%u %s: G=3 O=10\r\n", phase,
               (phase == HANDOFF_PHASE) ? "handoff" : "timeout");

        if (phase == TIMEOUT_PHASE)
        {
            ownerRelease = phase;
        }
        WaitPhase_(&ownerDone, phase, "owner release completion");
        gateFree = phase;
        WaitPhase_(&gateDone, phase, "gatekeeper free completion");
        ExpectPrio_(gatekeeperHandle, GATEKEEPER_PRIO, "gatekeeper restored");
        ExpectPrio_(ownerHandle, OWNER_PRIO, "owner remains restored");
        Check_((RK_BOOL)((resourceMutex.lock == RK_FALSE) &&
                         (resourceMutex.ownerPtr == NULL) &&
                         (gateMessage == NULL)), "phase cleanup");
        printf("GC phase=%u freed: G=8 O=10\r\n", phase);
    }

    printf("GC PASS gatekeeper ceiling through mutex PI\r\n");
    Stop_();
}

VOID GatekeeperTask(VOID *args)
{
    RK_UNUSEARGS

    for (UINT phase = HANDOFF_PHASE; phase <= TIMEOUT_PHASE; phase++)
    {
        RK_MESG *messagePtr = NULL;
        WaitPhase_(&gateStart, phase, "gatekeeper start");
        CheckErr_(kMesgAlloc(&messagePool, &messagePtr, RK_NO_WAIT),
                  "gatekeeper allocate before mutex wait");
        gateMessage = messagePtr;
        ExpectPrio_(gatekeeperHandle, CEILING_PRIO, "allocated message ceiling");

        RK_ERR const err = kMutexLock(&resourceMutex,
            (phase == HANDOFF_PHASE) ? RK_WAIT_FOREVER : MUTEX_TIMEOUT_TICKS);
        if (phase == HANDOFF_PHASE)
        {
            CheckErr_(err, "gatekeeper mutex handoff");
        }
        else
        {
            Check_((RK_BOOL)(err == RK_ERR_TIMEOUT), "gatekeeper mutex timeout");
        }
        ExpectPrio_(gatekeeperHandle, CEILING_PRIO, "ceiling after mutex return");
        gateReturned = phase;
        WaitPhase_(&gateFree, phase, "permission to finish message interval");

        if (phase == HANDOFF_PHASE)
        {
            CheckErr_(kMutexUnlock(&resourceMutex), "gatekeeper unlock");
            ExpectPrio_(gatekeeperHandle, CEILING_PRIO, "ceiling after unlock");
        }
        CheckErr_(kMesgFree(messagePtr), "gatekeeper free");
        gateMessage = NULL;
        ExpectPrio_(gatekeeperHandle, GATEKEEPER_PRIO, "nominal after free");
        gateDone = phase;
    }
    Stop_();
}

VOID OwnerTask(VOID *args)
{
    RK_UNUSEARGS

    for (UINT phase = HANDOFF_PHASE; phase <= TIMEOUT_PHASE; phase++)
    {
        WaitPhase_(&ownerStart, phase, "owner start");
        CheckErr_(kMutexLock(&resourceMutex, RK_NO_WAIT), "owner initial lock");
        ownerLocked = phase;
        WaitPhase_(&ownerRelease, phase, "owner release handshake");
        CheckErr_(kMutexUnlock(&resourceMutex), "owner unlock");
        ExpectPrio_(ownerHandle, OWNER_PRIO, "owner nominal after unlock");
        ownerDone = phase;
    }
    Stop_();
}

#else

#include <kapi.h>
#include <stdio.h>

#define STACKSIZ 256U

#define TASK_A_PRIO 2U
#define TASK_B_PRIO 5U
#define TASK_C_PRIO 8U

#define PERIOD  RK_MS_TO_TICKS(1000)
#define B_PHASE RK_MS_TO_TICKS(20)

/*  RK_MESG_PRIO_CEILING_NONE to see inversion */
#ifndef MESSAGE_CEILING
#define MESSAGE_CEILING TASK_A_PRIO
#endif

typedef struct Result
{
    ULONG sequence;
    ULONG checksum;
    RK_TICK startedMs;
    RK_TICK finishedMs;
} Result;

RK_DECLARE_TASK(taskAhandle, ReceiverTask, aStackBuf, STACKSIZ)
RK_DECLARE_TASK(taskBhandle, BackgroundTask, bStackBuf, STACKSIZ)
RK_DECLARE_TASK(taskChandle, ProducerTask, cStackBuf, STACKSIZ)

RK_DECLARE_MESG_POOL(resultPool, resultStorage, Result, 2U)

static volatile ULONG backgroundChecksum;

static VOID AppCheck_(RK_ERR const err)
{
    if (err != RK_ERR_SUCCESS)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
        while (1) {}
    }
}

/*
 * Application workload: generate data and accumulate its checksum.
 * Each invocation has independent state.
 *
 * This represents the computation needed to prepare a message.
 */
static ULONG Compute_(ULONG state, ULONG const count)
{
    ULONG checksum = 0UL;

    for (ULONG i = 0UL; i < count; i++)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        checksum += state;
    }

    return checksum;
}

int main(void)
{
    kCoreInit();
    kInit();

    while (1)
    {
        kErrHandler(RK_FAULT_APP_CRASH);
    }
}

VOID kApplicationInit(VOID)
{
    AppCheck_(kTaskInit(&taskAhandle, ReceiverTask, RK_NO_ARGS,
                       "A", aStackBuf, STACKSIZ,
                       TASK_A_PRIO, RK_PREEMPT));

    AppCheck_(kTaskInit(&taskBhandle, BackgroundTask, RK_NO_ARGS,
                       "B", bStackBuf, STACKSIZ,
                       TASK_B_PRIO, RK_PREEMPT));

    AppCheck_(kTaskInit(&taskChandle, ProducerTask, RK_NO_ARGS,
                       "C", cStackBuf, STACKSIZ,
                       TASK_C_PRIO, RK_PREEMPT));

    /* A is the single receiver. */
    AppCheck_(kMesgEndpointInit(taskAhandle));

    AppCheck_(kMesgPoolInit(&resultPool, resultStorage,
                           sizeof(Result), 2U,
                           MESSAGE_CEILING));
}

VOID ReceiverTask(VOID *args)
{
    RK_UNUSEARGS

    while (1)
    {
        RK_MESG *message = NULL;

        /* A cannot proceed until C supplies a result. */
        AppCheck_(kMesgWait(taskChandle, &message, RK_WAIT_FOREVER));

        Result const result = *RK_MESG_PAYLOAD(message, Result);

        AppCheck_(kMesgFree(message));

        printf("%lu ms A: result %lu, checksum=%08lx, "
               "preparation=%lu ms\r\n",
               kTickGetMs(),
               result.sequence,
               result.checksum,
               result.finishedMs - result.startedMs);
    }
}

VOID ProducerTask(VOID *args)
{
    RK_UNUSEARGS

    ULONG sequence = 0UL;

    while (1)
    {
        RK_MESG *message = NULL;

        RK_ERR const err =
            kMesgAlloc(&resultPool, &message, RK_NO_WAIT);

        if (err == RK_ERR_BUFFER_EMPTY)
        {
            /* Skip this production opportunity if no buffer is available. */
            AppCheck_(kSleepRelease(PERIOD));
            continue;
        }

        AppCheck_(err);

        /*
         * Ownership begins here, BEFORE preparing the result.
         * With the ceiling enabled, C now has effective priority 2.
         */
        Result *const result = RK_MESG_PAYLOAD(message, Result);

        result->sequence = ++sequence;
        result->startedMs = kTickGetMs();

        printf("%lu ms C: preparing result, priority=%u\r\n",
               kTickGetMs(), (UINT)RK_RUNNING_PRIO);

        AppCheck_(kBusyDelay(RK_MS_TO_TICKS(100)));
        result->checksum = Compute_(sequence, 256UL);
        result->finishedMs = kTickGetMs();

        AppCheck_(kMesgSend(taskAhandle, message));

        /* A owns the message now. C must no longer access it. */

        AppCheck_(kSleepRelease(PERIOD));
    }
}

VOID BackgroundTask(VOID *args)
{
    RK_UNUSEARGS

    /* First release occurs after C has started preparing its result. */
    AppCheck_(kSleep(B_PHASE));

    while (1)
    {
        printf("%lu ms B: unrelated computation starts\r\n",
               kTickGetMs());

      AppCheck_(kBusyDelay(RK_MS_TO_TICKS(200)));
      backgroundChecksum = Compute_(0x12345678UL, 256UL);

        printf("%lu ms B: unrelated computation ends\r\n",
               kTickGetMs());

        /* Subsequent releases occur 20 ms after each period boundary. */
        AppCheck_(kSleepRelease(PERIOD));
        AppCheck_(kSleep(B_PHASE));
    }
}
#endif
