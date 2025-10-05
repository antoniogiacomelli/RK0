# Real-Time Kernel 0 – v0.8.0-dev

## Characteristics
- **Target**: constrained embedded systems (ARMv6/7M), GCC toolchain.
- **Deterministic by design**: predictability is never compromised.
- **Modular & composable**; semantics are clear.
- **Scheduler**: O(1), priority-based, pre-emptive, no time-slice.
  - Priorities: 0-31, 0 is the highest.
  - Highest-priority READY task always runs (even if it just yielded).
  - Same-priority tasks: FIFO.
  - Yield ⇒ tail of ready queue. Block ⇒ tail when readied. Pre-empted ⇒ head when READY again.

## Conventions
- In `kconfig.h`: declare the exact number of tasks and the maximum priority used.
- Initialisation: call `kCoreInit()` (if CMSIS), then `kInit()`.
- Applications include `application.h`.
- Kernel calls start with `k...`.
- `RK_...` macros don’t need `;`.
- `K_...` macros must end with `;`.

---

## Programming with RK0

### main entry

A typical main() will include the platform HAL, and <application.h>. After
initialising the HAL, the kernel is initialised using `kCoreInit()` and 
`kInit()`:
```C

/* Platform includes */
#include <application.h>

int main(void)
{
    /* init clock, peripherals.... */
  
  kCoreInit(); /* initialise the interrupts needed for the kernel */  
  
  kInit(); /* start-up the executive: run low-level routines and call 
  kApplicationInit() */

while (1)
  {
             /* if stopping here, failure */
        asm volatile("BKPT #0");
        asm volatile ("cpsid i" : : : "memory");
  }
}

```
### The so-called application

It is up to you how to handle the highest layer. One or a dozen files. What is
mandatory is:

- The `VOID kApplicationInit(VOID)` function must exist and it creates all tasks
-- that will exist forever.
- Typically you initialise other kernel objects in this function -- sometimes, 
they can be initialised on the entry of a task before entering the `while(1)` 
loop.
- A task function has the signature VOID f(VOID*). 
- The convenience macro RK_DECLARE_TASK(taskHandle, taskEntry, stackBuf, stackSizeInWords)
can be used to declare the main static objects that compose a task.
- On kApplicationInit the `kCreateTask()` is used. 

RK_DECLARE_TASK(blinkHandle, BlinkTask, blinkStack, 512);

kCreateTask(&blinkHandle, /* task handle */
            BlinkTask,    /* task entry function */
            RK_NO_ARGS,   /* arguments */
            "blink",      /* task ascii name */
            blinkStack,   /* stack address */
            512,          /* stack size (words) */
            3,            /* priority */
            RK_PREEMPT);  /* preemptible or non preemptible task */

VOID BlinkTask(VOID *args)
{
    RK_UNUSEARGS;
    while (1)
    {
        toggle();
        kSleepPeriodic(100);
    }
}
```

- Prefer `kSleepPeriodic(period)` for aligned loops.
- Use `kSleep(ticks)` for relative waits.
- Use `kDelay()` only for simulating load.

---

## Inter-Task Communication

### Task Flags
```c
kTaskFlagsSet(taskHandle, 0x01);

ULONG got = 0;
kTaskFlagsGet(0xFF, RK_FLAGS_ANY, &got, RK_WAIT_FOREVER);
```

### Semaphores
```c
RK_SEMAPHORE slots;
kSemaphoreInit(&slots, 0, 16);

kSemaphorePend(&slots, RK_WAIT_FOREVER);
/* critical */
kSemaphorePost(&slots);
```

### Mutexes
```c
RK_MUTEX guard;
kMutexInit(&guard, RK_INHERIT);

kMutexLock(&guard, RK_WAIT_FOREVER);
/* critical region */
kMutexUnlock(&guard);
```

### Message Queues
```c
typedef struct
{
    UINT id;
    UINT value;
} Msg_t;

RK_MESG_QUEUE q;
RK_DECLARE_MESGQ_BUF(qbuf, Msg_t, 16);

kMesgQueueInit(&q,
               qbuf,
               K_MESGQ_MESG_SIZE(Msg_t),
               16);

kMesgQueueSend(&q, &msg, RK_NO_WAIT);
kMesgQueueRecv(&q, &out, RK_WAIT_FOREVER);
```

### Mailbox
```c
RK_MAILBOX mbox;
kMailboxInit(&mbox);

Sample *p = &isr_sample;
kMailboxPost(&mbox, &p, RK_NO_WAIT);

Sample *in = NULL;
kMailboxPend(&mbox, &in, RK_WAIT_FOREVER);
```

### Ports (RPC)
```c
/* Server */
RK_PORT port;
RK_DECLARE_PORT_BUF(pbuf, 4, 16);
kPortInit(&port, pbuf, 4, 16, serverHandle);

VOID ServerTask(VOID *args)
{
    RK_UNUSEARGS;
    RK_PORT_MESG_4WORD m;
    while (1)
    {
        kPortRecv(&port, &m, RK_WAIT_FOREVER);
        UINT reply = do_work((VOID*)m.payload[0], m.payload[1]);
        kPortReplyDone(&port, (ULONG*)&m, reply);
    }
}
```

```c
/* Client */
RK_MAILBOX replyBox;
kMailboxInit(&replyBox);

RK_PORT_MESG_4WORD req = {0};
req.payload[0] = (ULONG)buf;
req.payload[1] = len;

UINT reply = 0;
kPortSendRecv(&port, (ULONG*)&req, &replyBox, &reply, RK_WAIT_FOREVER);
```

---

## Condition Variables & Monitors

### Mesa-style loop
```c
kMutexLock(&lock, RK_WAIT_FOREVER);
while (!predicate())
{
    kCondVarWait(&condQ, &lock, RK_WAIT_FOREVER);
}
kMutexUnlock(&lock);
```

### Barrier
```c
Barrier_t b;
BarrierInit(&b, 3);
BarrierWait(&b);
```

### Readers–Writers
```c
RwLock_t rw;
RwLockInit(&rw);

RwLockAcquireRead(&rw);
RwLockReleaseRead(&rw);

RwLockAcquireWrite(&rw);
RwLockReleaseWrite(&rw);
```

---

## Message Passing as a Kernel Model

RK0 can operate as a **message-passing kernel**:

- Queues: bounded, copy-based.
- Mailboxes: one-word channels, FIFO or latest-only.
- Ports: RPC with priority hand-off.

This allows systems with no shared state. Ownership is explicit. Priority correctness is guaranteed.

---

## Most Recent Message (MRM)

MRM is a data freshness protocol. Not message passing.

```c
typedef struct
{
    UINT speed;
    RK_TICK ts;
} SpeedMsg;

SpeedMsg *w;
kMRMReserve(&mrmCtl, (VOID**)&w);
*w = (SpeedMsg){ .speed = read(), .ts = kTickGet() };
kMRMPublish(&mrmCtl, w);

SpeedMsg r;
RK_MRM_BUF *h;
if (kMRMGet(&mrmCtl, &h, &r) == RK_ERR_SUCCESS)
{
    use(&r);
    kMRMUnget(&mrmCtl, h);
}
```

---

## Active Object Pattern

Encapsulate state and behaviour in a task with a message interface.

```c
typedef struct
{
    INT k;
    INT acc;
} FilterState_t;

typedef struct
{
    RK_TASK_HANDLE task;
    RK_MESG_QUEUE  q;
    RK_PORT        port;
} FilterAO_t;
```

AO task processes async queue + sync RPC:

```c
static VOID _FilterTask(VOID *args)
{
    FilterAO_t *ao = (FilterAO_t*)args;
    FilterState_t st = { .k = 1, .acc = 0 };
    RK_UNUSEARGS;

    AoMsg_t m = {0};

    while (1)
    {
        while (kMesgQueueRecv(&ao->q, &m, RK_NO_WAIT) == RK_ERR_SUCCESS)
        {
            if (m.cmd == AO_CMD_SET_GAIN)
                st.k = m.a0;
            else if (m.cmd == AO_CMD_UPDATE)
                st.acc += (m.a0 * st.k);
        }

        RK_PORT_MESG_4WORD req = {0};
        if (kPortRecv(&ao->port, &req, 10) == RK_ERR_SUCCESS)
        {
            UINT verb = (UINT)req.payload[0];
            UINT reply = (verb == 1U) ? (UINT)st.acc : 0U;
            kPortReplyDone(&ao->port, (ULONG*)&req, reply);
        }

        if (kMesgQueueRecv(&ao->q, &m, RK_NO_WAIT) != RK_ERR_SUCCESS)
            kYield();
    }
}
```

Client demo:

```c
FilterAO_t f;
FilterAO_Init(&f, "FilterAO", 2, _filterStack, 512);

FilterAO_SetGain(&f, 3, RK_NO_WAIT);
FilterAO_Update(&f, 11, RK_NO_WAIT);

INT acc = 0;
if (FilterAO_GetAcc(&f, &acc, 50) == RK_ERR_SUCCESS)
{
    kprintf("ACC=%d\r\n", acc);
}
```

---

## Common Pitfalls

- Using `kDelay()` in loops. Use `kSleepPeriodic()` or `kSleep()`.
- Treating binary semaphores as mutexes. Use Mutex with `RK_INHERIT`.
- Forgetting priority inheritance.
- Misusing mailboxes: default FIFO, overwrite only with `postovw`.
- Starving same-priority tasks by never yielding.
- Deadlocks from unordered locks.
- Dropping messages silently: check return codes.
- Exhausting memory pools: they are fixed-size.
- Bypassing encapsulation: prefer Active Objects.

---

 **In short:** RK0 is deterministic, modular, and lets you choose shared-memory synchronisation or a clean message-passing kernel with Active Objects.
