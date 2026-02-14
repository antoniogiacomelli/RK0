# RK0 Services Quick Guide (API + Return Values)

Target: RK0 `v0.9.15`

This is a quick-reference guide, not a tutorial. It focuses on:

- what each service does,
- the exact public APIs,
- expected return values,
- minimal idiomatic usage patterns.

All API names and return values are aligned with `core/inc/kapi.h` and `core/inc/kcommondefs.h`.

---

## 1. Scheduler and Execution Model

RK0 scheduling model (practical view):

- Priority-based preemptive scheduler, `0` is highest priority.
- O(1) ready selection using per-priority ready queues + ready bitmap.
- One FIFO queue per priority.
- No time-slicing.
- Same-priority tasks cooperate by blocking/waiting/yielding.
- Two system tasks always exist:
  - `IdleTask` (lowest effective priority)
  - `PostProcSysTask` (priority `0`, non-preemptible by user tasks)

### 1.1 Context-switch triggers

Context switch is requested when:

- a higher-priority task becomes `READY`,
- current task yields (`kYield()`),
- current task blocks,
- tick/path handling readies a higher-priority task.

`kSchLock()` and `kSchUnlock()` defer preemption windows for the running task:

- `kSchLock()` increments scheduler lock counter.
- `kSchUnlock()` decrements it.
- if a switch became pending while locked, unlock triggers it immediately.

### 1.2 Scheduler-related APIs

```c
RK_ERR kCreateTask(RK_TASK_HANDLE *taskHandlePtr,
                   const RK_TASKENTRY taskFunc,
                   VOID *argsPtr,
                   CHAR *const taskName,
                   RK_STACK *const stackBufPtr,
                   const ULONG stackSize,
                   const RK_PRIO priority,
                   const ULONG preempt);

VOID kInit(VOID);
VOID kYield(VOID);
VOID kSchLock(VOID);
VOID kSchUnlock(VOID);
```

`kCreateTask()` return values:

- `RK_ERR_SUCCESS`
- `RK_ERR_ERROR`

### 1.3 Scheduling rules that affect service behaviour

- Highest-priority `READY` task always runs.
- `kYield()` only rotates execution among tasks at the same priority.
- If a task never blocks and never yields, same-priority peers can starve.
- A task made `READY` by an object operation (`post`, `unlock`, `signal`, `wake`, message receive/send release) can preempt immediately unless:
  - the running task is non-preemptible (`RK_NO_PREEMPT`), or
  - scheduler lock is active (`kSchLock()` depth > 0).
- The timer-handler system task is prioritised for deferred timer/callback work; callouts therefore compete at the top of the priority hierarchy.
- `kSemaphoreFlush()` and `kSleepQueueWake()` execute synchronously in thread context.
- From ISR, those operations are enqueued to the post-processing system task.

Minimal pattern:

```c
#include <kapi.h>

#define STACK_WORDS 256
RK_DECLARE_TASK(workerHandle, WorkerTask, workerStack, STACK_WORDS)

VOID WorkerTask(VOID *args)
{
    while (1)
    {
        kYield();
    }
}

VOID kApplicationInit(VOID)
{
    RK_ERR err = kCreateTask(&workerHandle, WorkerTask, RK_NO_ARGS,
                             "Worker", workerStack, STACK_WORDS,
                             3, RK_PREEMPT);
    K_ASSERT(err == RK_ERR_SUCCESS);
}
```

---

## 2. Return-Value Model

RK0 uses `RK_ERR` for most APIs:

- `0` (`RK_ERR_SUCCESS`): successful operation.
- `> 0`: unsuccessful but defined runtime outcome.
- `< 0`: error (invalid object/parameter/context or invariant break).

Common tokens:

- Timeouts: `RK_NO_WAIT`, `RK_WAIT_FOREVER`, finite ticks (`<= RK_MAX_PERIOD`).
- Event options: `RK_EVENT_ANY`, `RK_EVENT_ALL`.
- Mutex mode: `RK_INHERIT`, `RK_NO_INHERIT`.

Frequent positive outcomes:

- `RK_ERR_TIMEOUT`
- `RK_ERR_NOWAIT`
- `RK_ERR_EMPTY_WAITING_QUEUE`
- `RK_ERR_MESGQ_EMPTY`, `RK_ERR_MESGQ_FULL`
- `RK_ERR_MUTEX_LOCKED`
- `RK_ERR_SEMA_BLOCKED`, `RK_ERR_SEMA_FULL`

Frequent negative errors:

- `RK_ERR_OBJ_NULL`
- `RK_ERR_OBJ_NOT_INIT`
- `RK_ERR_INVALID_OBJ`
- `RK_ERR_INVALID_PARAM`
- `RK_ERR_INVALID_TIMEOUT`
- `RK_ERR_INVALID_ISR_PRIMITIVE`

---

## 3. Service Overview (by API family)

Configuration switches are in `core/inc/kconfig.h`.

Core services (always present):

- task/scheduler
- task event flags
- time primitives
- memory partition allocator

Optional services:

- `RK_CONF_SEMAPHORE`
- `RK_CONF_MUTEX`
- `RK_CONF_SLEEP_QUEUE`
- `RK_CONF_MESG_QUEUE` (`RK_CONF_PORTS` optional on top)
- `RK_CONF_MRM`
- `RK_CONF_CALLOUT_TIMER`

---

## 4. Task Event Flags

### APIs

```c
RK_ERR kTaskEventGet(ULONG const required,
                     UINT const options,
                     ULONG *const gotFlagsPtr,
                     RK_TICK const timeout);

RK_ERR kTaskEventSet(RK_TASK_HANDLE const taskHandle,
                     ULONG const mask);

RK_ERR kTaskEventQuery(RK_TASK_HANDLE const taskHandle,
                       ULONG *const gotFlagsPtr);

RK_ERR kTaskEventClear(RK_TASK_HANDLE const taskHandle,
                       ULONG const flagsToClear);
```

### Return values

`kTaskEventGet`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_FLAGS_NOT_MET`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_INVALID_PARAM`

`kTaskEventSet`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_PARAM`

`kTaskEventQuery`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kTaskEventClear`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_INVALID_PARAM`


```c
#define EVT_RX   (1UL << 0)
#define EVT_ERR  (1UL << 1)

RK_ERR err = kTaskEventSet(workerHandle, EVT_RX);
K_ASSERT(err == RK_ERR_SUCCESS);

ULONG got = 0;
err = kTaskEventGet(EVT_RX | EVT_ERR, RK_EVENT_ANY, &got, 50);
if (err == RK_ERR_SUCCESS)
{
    if (got & EVT_RX)
    {
        /* handle RX */
    }
}
```

```c
typedef struct
{
    ULONG pendingBit;
    RK_TASK_HANDLE dstTask;
    ULONG dstSignal;
} Route_t;

static const Route_t routes[] =
{
    { PENDING_AIRFLOW_INCREASE, airFlowTaskHandle, AIRFLOW_INCREASE_SIGNAL },
    { PENDING_TEMP_DECREASE,    tempTaskHandle,    TEMP_DECREASE_SIGNAL }
};

VOID SupervisorTask(VOID *args)
{
    while (1)
    {
        ULONG gotFlags = 0;
        RK_ERR err = kTaskEventGet(0xFFFF, RK_FLAGS_ANY, &gotFlags, SUPERVISOR_T_PERIOD);

        if (err == RK_ERR_SUCCESS && gotFlags != 0)
        {
            for (ULONG i = 0; i < ARRAY_LEN(routes); ++i)
            {
                if ((gotFlags & routes[i].pendingBit) != 0)
                {
                    err = kTaskEventSet(routes[i].dstTask, routes[i].dstSignal);
                    K_ASSERT(err == RK_ERR_SUCCESS);
                }
            }
        }
    }
}
```

---

## 5. Semaphores (`RK_CONF_SEMAPHORE`)

### APIs

```c
RK_ERR kSemaphoreInit(RK_SEMAPHORE *const kobj,
                      UINT const initValue,
                      const UINT maxValue);

RK_ERR kSemaphorePend(RK_SEMAPHORE *const kobj,
                      const RK_TICK timeout);

RK_ERR kSemaphorePost(RK_SEMAPHORE *const kobj);
RK_ERR kSemaphoreFlush(RK_SEMAPHORE *const kobj);
RK_ERR kSemaphoreQuery(RK_SEMAPHORE const *const kobj,
                       INT *const countPtr);
```

Convenience macros:

```c
kSemaCountInit(&sem, initValue);
kSemaBinInit(&sem, initValue);
```

### Return values

`kSemaphoreInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_DOUBLE_INIT`, `RK_ERR_INVALID_PARAM`, `RK_ERR_ERROR`

`kSemaphorePend`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_SEMA_BLOCKED`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kSemaphorePost`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_SEMA_FULL`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

`kSemaphoreFlush`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_EMPTY_WAITING_QUEUE`, `RK_ERR_NOWAIT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

`kSemaphoreQuery`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

Minimal pattern:

```c
static RK_SEMAPHORE slots;

RK_ERR err = kSemaCountInit(&slots, 8);
K_ASSERT(err == RK_ERR_SUCCESS);

err = kSemaphorePend(&slots, RK_WAIT_FOREVER);
if (err == RK_ERR_SUCCESS)
{
    err = kSemaphorePost(&slots);
    K_ASSERT(err == RK_ERR_SUCCESS || err == RK_ERR_SEMA_FULL);
}
```


```c
#define BUFSIZ K
static ITEM_t buf[BUFSIZ];
static UINT getIdx = 0;
static UINT putIdx = 0;

static RK_SEMAPHORE itemSema;    /* number of items */
static RK_SEMAPHORE slotSema;    /* number of free slots */
static RK_SEMAPHORE acquireSema; /* mutual exclusion on buffer state */

VOID BufferInit(VOID)
{
    RK_ERR err;

    err = kSemaphoreInit(&itemSema, 0, K);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSemaphoreInit(&slotSema, K, K);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSemaphoreInit(&acquireSema, 1, 1);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID PutItem(ITEM_t const *insertItemPtr)
{
    RK_ERR err;

    err = kSemaphorePend(&slotSema, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSemaphorePend(&acquireSema, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    buf[putIdx] = *insertItemPtr;
    putIdx = (putIdx + 1U) % BUFSIZ;

    err = kSemaphorePost(&acquireSema);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSemaphorePost(&itemSema);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

VOID GetItem(ITEM_t *extractItemPtr)
{
    RK_ERR err;

    err = kSemaphorePend(&itemSema, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSemaphorePend(&acquireSema, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    *extractItemPtr = buf[getIdx];
    getIdx = (getIdx + 1U) % BUFSIZ;

    err = kSemaphorePost(&acquireSema);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSemaphorePost(&slotSema);
    K_ASSERT(err == RK_ERR_SUCCESS);
}
```

---

## 6. Mutex (`RK_CONF_MUTEX`)

### APIs

```c
RK_ERR kMutexInit(RK_MUTEX *const kobj, UINT prioInh);
RK_ERR kMutexLock(RK_MUTEX *const kobj, RK_TICK const timeout);
RK_ERR kMutexUnlock(RK_MUTEX *const kobj);
RK_ERR kMutexQuery(RK_MUTEX const *const kobj, UINT *const statePtr);
```

### Return values

`kMutexInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_ERROR`, `RK_ERR_OBJ_DOUBLE_INIT`, `RK_ERR_INVALID_PARAM`

`kMutexLock`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MUTEX_LOCKED`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_MUTEX_REC_LOCK`

`kMutexUnlock`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`,
  `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_MUTEX_NOT_LOCKED`, `RK_ERR_MUTEX_NOT_OWNER`

`kMutexQuery`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

Minimal pattern:

```c
static RK_MUTEX lock;

RK_ERR err = kMutexInit(&lock, RK_INHERIT);
K_ASSERT(err == RK_ERR_SUCCESS);

err = kMutexLock(&lock, RK_WAIT_FOREVER);
if (err == RK_ERR_SUCCESS)
{
    /* critical section */
    err = kMutexUnlock(&lock);
    K_ASSERT(err == RK_ERR_SUCCESS);
}
```

---

## 7. Sleep Queue + Condition Variables (`RK_CONF_SLEEP_QUEUE`)

### APIs

```c
RK_ERR kSleepQueueInit(RK_SLEEP_QUEUE *const kobj);
RK_ERR kSleepQueueWait(RK_SLEEP_QUEUE *const kobj, const RK_TICK timeout);
RK_ERR kSleepQueueWake(RK_SLEEP_QUEUE *const kobj, UINT nTasks, UINT *uTasksPtr);
RK_ERR kSleepQueueSignal(RK_SLEEP_QUEUE *const kobj);
RK_ERR kSleepQueueReady(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE taskHandle);
RK_ERR kSleepQueueSuspend(RK_SLEEP_QUEUE *const kobj, RK_TASK_HANDLE handle);
RK_ERR kSleepQueueQuery(RK_SLEEP_QUEUE const *const kobj, ULONG *const nTasksPtr);

RK_ERR kCondVarWait(RK_SLEEP_QUEUE *const cv, RK_MUTEX *const mutex, RK_TICK timeout);
RK_ERR kCondVarSignal(RK_SLEEP_QUEUE *const cv);
RK_ERR kCondVarBroadcast(RK_SLEEP_QUEUE *const cv);
```


```c
kSleepQueueFlush(&sq);
```

### Return values

`kSleepQueueWait`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_NOWAIT`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kSleepQueueWake`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_EMPTY_WAITING_QUEUE`, `RK_ERR_NOWAIT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_PARAM`
- ISR usage: request is enqueued to post-processing; `uTasksPtr` must be `NULL`

`kSleepQueueSignal`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_EMPTY_WAITING_QUEUE`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

`kSleepQueueReady`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_EMPTY_WAITING_QUEUE`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

`kSleepQueueSuspend`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_PARAM`

`kSleepQueueQuery`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

`kCondVarWait`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_TIMEOUT`, `RK_ERR_NOWAIT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_INVALID_ISR_PRIMITIVE` (+ propagated mutex/sleepq errors)

`kCondVarSignal`, `kCondVarBroadcast`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_EMPTY_WAITING_QUEUE`
- errors: `RK_ERR_INVALID_ISR_PRIMITIVE` (+ propagated sleepq errors)

Minimal monitor-style loop:

```c
while (!predicate())
{
    RK_ERR err = kCondVarWait(&cv, &lock, RK_WAIT_FOREVER);
}
```


```c
typedef struct
{
    RK_MUTEX lock;
    RK_SLEEP_QUEUE allSynch;
    UINT count;
    UINT round;
    UINT nRequired;
} Barrier_t;

VOID BarrierInit(Barrier_t *const barPtr, UINT nRequired)
{
    RK_ERR err;

    err = kMutexInit(&barPtr->lock, RK_INHERIT);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kSleepQueueInit(&barPtr->allSynch);
    K_ASSERT(err == RK_ERR_SUCCESS);

    barPtr->count = 0;
    barPtr->round = 0;
    barPtr->nRequired = nRequired;
}

VOID BarrierWait(Barrier_t *const barPtr)
{
    UINT myRound;
    RK_ERR err;

    err = kMutexLock(&barPtr->lock, RK_WAIT_FOREVER);
    K_ASSERT(err == RK_ERR_SUCCESS);

    myRound = barPtr->round;
    barPtr->count++;

    if (barPtr->count == barPtr->nRequired)
    {
        barPtr->round++;
        barPtr->count = 0;

        err = kCondVarBroadcast(&barPtr->allSynch);
        K_ASSERT(err == RK_ERR_SUCCESS || err == RK_ERR_EMPTY_WAITING_QUEUE);
    }
    else
    {
        while ((UINT)(barPtr->round - myRound) == 0U)
        {
            err = kCondVarWait(&barPtr->allSynch, &barPtr->lock, RK_WAIT_FOREVER);
            K_ASSERT(err == RK_ERR_SUCCESS);
        }
    }

    err = kMutexUnlock(&barPtr->lock);
    K_ASSERT(err == RK_ERR_SUCCESS);
}
```

---

## 8. Message Queues and Mailbox (`RK_CONF_MESG_QUEUE`)

### APIs (queue)

```c
RK_ERR kMesgQueueInit(RK_MESG_QUEUE *const kobj,
                      VOID *const bufPtr,
                      const ULONG mesgSizeInWords,
                      const ULONG nMesg);

RK_ERR kMesgQueueSetOwner(RK_MESG_QUEUE *const kobj,
                          const RK_TASK_HANDLE taskHandle);

RK_ERR kMesgQueueRecv(RK_MESG_QUEUE *const kobj,
                      VOID *const recvPtr,
                      const RK_TICK timeout);

RK_ERR kMesgQueueSend(RK_MESG_QUEUE *const kobj,
                      VOID *const sendPtr,
                      const RK_TICK timeout);

RK_ERR kMesgQueueJam(RK_MESG_QUEUE *const kobj,
                     VOID *const sendPtr,
                     const RK_TICK timeout);

RK_ERR kMesgQueuePeek(RK_MESG_QUEUE const *const kobj,
                      VOID *const recvPtr);

RK_ERR kMesgQueueReset(RK_MESG_QUEUE *const kobj);
RK_ERR kMesgQueueQuery(RK_MESG_QUEUE const *const kobj, UINT *const nMesgPtr);
RK_ERR kMesgQueuePostOvw(RK_MESG_QUEUE *const kobj, VOID *sendPtr);
```

### APIs (mailbox)

```c
kMailboxInit
kMailboxPost
kMailboxPend
kMailboxReset
kMailboxPeek
kMailboxPostOvw
kMailboxSetOwner
```

### Return values (queue)

`kMesgQueueInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_MESGQ_INVALID_MESG_SIZE`, `RK_ERR_MESGQ_INVALID_SIZE`, `RK_ERR_OBJ_DOUBLE_INIT`

`kMesgQueueSetOwner`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_HAS_OWNER`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`

`kMesgQueueRecv`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_NOT_OWNER`, `RK_ERR_MESGQ_EMPTY`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kMesgQueueSend` / `kMesgQueueJam`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_FULL`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kMesgQueuePeek`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_EMPTY`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`

`kMesgQueueReset`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_NOT_INIT`

`kMesgQueueQuery`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_OBJ`

`kMesgQueuePostOvw`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_NOT_A_MBOX`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`

Minimal pattern:

```c
typedef struct
{
    ULONG id;
    ULONG value;
} SampleMsg;

static RK_MESG_QUEUE q;
RK_DECLARE_MESG_QUEUE_BUF(qBuf, SampleMsg, 8)

RK_ERR err = kMesgQueueInit(&q, qBuf, RK_MESGQ_MESG_SIZE(SampleMsg), 8);
K_ASSERT(err == RK_ERR_SUCCESS);

SampleMsg tx = {1, 42};
SampleMsg rx = {0, 0};

err = kMesgQueueSend(&q, &tx, RK_NO_WAIT);
K_ASSERT(err == RK_ERR_SUCCESS || err == RK_ERR_MESGQ_FULL);

err = kMesgQueueRecv(&q, &rx, RK_WAIT_FOREVER);
K_ASSERT(err == RK_ERR_SUCCESS);
```

---

## 9. Ports / RPC (`RK_CONF_PORTS`)

### APIs

```c
RK_ERR kPortInit(RK_PORT *const kobj,
                 VOID *const buf,
                 const ULONG msgWords,
                 const ULONG nMesg,
                 RK_TASK_HANDLE const owner);

RK_ERR kPortSend(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout);
RK_ERR kPortRecv(RK_PORT *const kobj, VOID *const msg, const RK_TICK timeout);
RK_ERR kPortServerDone(RK_PORT *const kobj);

RK_ERR kRegisterMailbox(RK_TASK_HANDLE const taskHandle,
                        RK_MAILBOX *const replyBox);

RK_ERR kPortSendRecv(RK_PORT *const kobj,
                     ULONG *const msgWordsPtr,
                     UINT *const replyCodePtr,
                     const RK_TICK timeout);

RK_ERR kPortSendRecvFromMbox(RK_PORT *const kobj,
                             ULONG *const msgWordsPtr,
                             UINT *const replyCodePtr,
                             RK_MAILBOX *const replyBox,
                             const RK_TICK timeout);

RK_ERR kPortReply(RK_PORT *const kobj,
                  ULONG const *const msgWords,
                  const UINT replyCode);

RK_ERR kPortReplyDone(RK_PORT *const kobj,
                      ULONG const *const msgWords,
                      const UINT replyCode);
```

### Return values

`kPortInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_MESGQ_INVALID_MESG_SIZE`, `RK_ERR_MESGQ_INVALID_SIZE`, `RK_ERR_OBJ_DOUBLE_INIT`, `RK_ERR_MESGQ_HAS_OWNER`

`kPortSend`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_FULL`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kPortRecv`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_NOT_OWNER`, `RK_ERR_MESGQ_EMPTY`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kPortServerDone`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`

`kRegisterMailbox`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_MESGQ_HAS_OWNER`, `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_ISR_PRIMITIVE`
- note: pass `replyBox == NULL` to unregister and explicitly disable RPC replies for that task.

`kPortSendRecv`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_FULL`, `RK_ERR_MESGQ_EMPTY`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_MESGQ_INVALID_MESG_SIZE`
- note: `RK_ERR_OBJ_NULL` is also returned when the caller has no mailbox registered.

`kPortSendRecvFromMbox`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MESGQ_FULL`, `RK_ERR_MESGQ_EMPTY`, `RK_ERR_TIMEOUT`, `RK_ERR_INVALID_TIMEOUT`, `RK_ERR_MESGQ_HAS_OWNER`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_MESGQ_INVALID_MESG_SIZE`
- note: does not require task mailbox registration; the reply mailbox is supplied per call.
- note: `replyBox` must be unowned or owned by the caller task.

`kPortReply`, `kPortReplyDone`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_MESGQ_INVALID_MESG_SIZE`

Minimal client/server pattern:

```c
static RK_PORT svc;
RK_DECLARE_PORT_BUF(svcBuf, RK_PORT_MESG_4WORD, 8)

/* server init */
RK_ERR err = kPortInit(&svc,
                       svcBuf,
                       RK_MESGQ_MESG_SIZE(RK_PORT_MESG_4WORD),
                       8,
                       serverHandle);
K_ASSERT(err == RK_ERR_SUCCESS);

/* client setup */
static RK_MAILBOX replyBox;
err = kMailboxInit(&replyBox);
K_ASSERT(err == RK_ERR_SUCCESS);

err = kRegisterMailbox(clientHandle, &replyBox);
K_ASSERT(err == RK_ERR_SUCCESS);

/* client call */
RK_PORT_MESG_4WORD req = {0};
UINT reply = 0;
err = kPortSendRecv(&svc, (ULONG *)&req, &reply, RK_WAIT_FOREVER);
K_ASSERT(err == RK_ERR_SUCCESS);
```

Docbook-derived idiom (sync request/reply with server demotion on `ReplyDone`):

```c
typedef RK_PORT_MESG_4WORD RpcMsg;

static RK_PORT serverPort;
RK_DECLARE_PORT_BUF(portBuf, RK_PORT_MESG_4WORD, 16)

VOID ServerTask(VOID *args)
{
    RpcMsg msg;

    while (1)
    {
        RK_ERR err = kPortRecv(&serverPort, &msg, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);

        /* trivial reply code from payload */
        UINT replyCode = (UINT)msg.payload[0];

        err = kPortReplyDone(&serverPort, (ULONG const *)&msg, replyCode);
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}

VOID ClientTask(VOID *args)
{
    RK_MAILBOX replyBox;
    RpcMsg msg = {0};
    UINT reply = 0;

    RK_ERR err = kMailboxInit(&replyBox);
    K_ASSERT(err == RK_ERR_SUCCESS);

    err = kRegisterMailbox(clientHandle, &replyBox);
    K_ASSERT(err == RK_ERR_SUCCESS);

    while (1)
    {
        err = kPortSendRecv(&serverPort, (ULONG *)&msg, &reply, RK_WAIT_FOREVER);
        K_ASSERT(err == RK_ERR_SUCCESS);
        K_ASSERT(reply == (UINT)msg.payload[0]);
    }
}
```

---

## 10. MRM (`RK_CONF_MRM`)

### APIs

```c
RK_ERR kMRMInit(RK_MRM *const kobj,
                RK_MRM_BUF *const mrmPoolPtr,
                VOID *mesgPoolPtr,
                ULONG const nBufs,
                ULONG const dataSizeWords);

RK_MRM_BUF *kMRMReserve(RK_MRM *const kobj);
RK_ERR kMRMPublish(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr, VOID const *dataPtr);
RK_MRM_BUF *kMRMGet(RK_MRM *const kobj, VOID *const getMesgPtr);
RK_ERR kMRMUnget(RK_MRM *const kobj, RK_MRM_BUF *const bufPtr);
```

### Return values

`kMRMInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_DOUBLE_INIT`

`kMRMPublish`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_OBJ`

`kMRMUnget`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_NOT_INIT`, `RK_ERR_INVALID_OBJ`

`kMRMReserve` and `kMRMGet` return pointers (`NULL` on failure/unavailable).

Minimal pattern:

```c
typedef struct
{
    ULONG pos;
    ULONG vel;
} Telemetry;

#define MRM_SLOTS 5
static RK_MRM mrm;
static RK_MRM_BUF mrmPool[MRM_SLOTS];
static ULONG mrmDataPool[MRM_SLOTS * RK_TYPE_WORD_COUNT(Telemetry)];

RK_ERR err = kMRMInit(&mrm, mrmPool, mrmDataPool,
                      MRM_SLOTS, RK_TYPE_WORD_COUNT(Telemetry));
K_ASSERT(err == RK_ERR_SUCCESS);

Telemetry t = {10, 2};
RK_MRM_BUF *wb = kMRMReserve(&mrm);
if (wb != NULL)
{
    err = kMRMPublish(&mrm, wb, &t);
    K_ASSERT(err == RK_ERR_SUCCESS);
}
```

---

## 11. Application Timers (`RK_CONF_CALLOUT_TIMER`)

### APIs

```c
RK_ERR kTimerInit(RK_TIMER *const kobj,
                  const RK_TICK phase,
                  const RK_TICK countTicks,
                  const RK_TIMER_CALLOUT funPtr,
                  VOID *argsPtr,
                  const UINT reload);

RK_ERR kTimerCancel(RK_TIMER *const kobj);
```

### Return values

`kTimerInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_DOUBLE_INIT`, `RK_ERR_INVALID_PARAM`

`kTimerCancel`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_ERROR`

Minimal pattern:

```c
static RK_TIMER hb;

static VOID hbCbk(VOID *args)
{
    RK_ERR err = kTaskEventSet(workerHandle, 1UL << 0);
    K_ASSERT(err == RK_ERR_SUCCESS);
}

RK_ERR err = kTimerInit(&hb, 0, 100, hbCbk, RK_NO_ARGS, RK_TIMER_RELOAD);
K_ASSERT(err == RK_ERR_SUCCESS);
```

---

## 12. Time Primitives

### APIs

```c
RK_ERR kSleepDelay(const RK_TICK ticks);   /* alias: kSleep(t) */
RK_ERR kSleepRelease(RK_TICK const period);/* alias: kSleepPeriodic(t) */
RK_ERR kSleepUntil(RK_TICK *lastTickPtr, RK_TICK const period);
RK_TICK kTickGet(VOID);
RK_TICK kTickGetMs(VOID);
RK_ERR kDelay(RK_TICK const ticks);
```

### Return values

`kSleepDelay`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_TASK_INVALID_ST`, `RK_ERR_INVALID_PARAM`

`kSleepRelease`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_INVALID_PARAM`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kSleepUntil`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_ELAPSED_PERIOD`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_PARAM`, `RK_ERR_INVALID_ISR_PRIMITIVE`

`kDelay`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_INVALID_ISR_PRIMITIVE`, `RK_ERR_INVALID_PARAM`

Minimal pattern:

```c
RK_TICK anchor = kTickGet();

for (;;)
{
    RK_ERR err = kSleepUntil(&anchor, 100);
    if (err == RK_ERR_ELAPSED_PERIOD)
    {
        /* deadline miss handling */
    }
    else
    {
        K_ASSERT(err == RK_ERR_SUCCESS);
    }
}
```

---

## 13. Memory Partition (fixed-block allocator)

### APIs

```c
RK_ERR kMemPartitionInit(RK_MEM_PARTITION *const kobj,
                         VOID *memPoolPtr,
                         ULONG blkSize,
                         const ULONG numBlocks);

VOID *kMemPartitionAlloc(RK_MEM_PARTITION *const kobj);
RK_ERR kMemPartitionFree(RK_MEM_PARTITION *const kobj, VOID *blockPtr);
```

### Return values

`kMemPartitionInit`:

- success: `RK_ERR_SUCCESS`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_OBJ_DOUBLE_INIT`

`kMemPartitionAlloc`:

- returns block pointer on success, `NULL` on failure

`kMemPartitionFree`:

- success: `RK_ERR_SUCCESS`
- unsuccessful: `RK_ERR_MEM_FREE`
- errors: `RK_ERR_OBJ_NULL`, `RK_ERR_INVALID_OBJ`, `RK_ERR_OBJ_NOT_INIT`

Minimal pattern:

```c
#define BLK_SIZE  32
#define BLK_COUNT 16

static BYTE pool[BLK_SIZE * BLK_COUNT];
static RK_MEM_PARTITION part;

RK_ERR err = kMemPartitionInit(&part, pool, BLK_SIZE, BLK_COUNT);
K_ASSERT(err == RK_ERR_SUCCESS);

VOID *blk = kMemPartitionAlloc(&part);
if (blk != NULL)
{
    err = kMemPartitionFree(&part, blk);
    K_ASSERT(err == RK_ERR_SUCCESS);
}
```

---

## 14. Misc Helpers

```c
unsigned int kGetVersion(void);
void kErrHandler(RK_FAULT fault);
```

Scheduler helper usage:

```c
kSchLock();
/* short non-preemptible section */
kSchUnlock();
```

---

## 15. Practical Rules (Engineering Constraints)

- Blocking APIs are not valid in ISR context.
- `kYield()` is for same-priority cooperation, not fairness across priorities.
- Keep timer callbacks short and non-blocking.
- Use `RK_INHERIT` for mutexes across mixed-priority tasks.
- For ports, either register client mailbox before `kPortSendRecv()`, or call `kPortSendRecvFromMbox()` with an explicit mailbox.
- For periodic control tasks, prefer `kSleepRelease`/`kSleepUntil` over `kSleep`.
