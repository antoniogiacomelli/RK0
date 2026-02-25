# The Duality of Messages and Shared State (and the Determinism Budget)

Concurrency discussions often get framed as a choice between:

- shared memory + locks, or
- message passing + actors/channels.

In practice, both styles are ways to enforce the same thing: **exclusive control
of state** and a **well-defined order of updates**. What changes is where the
system stores the "waiting" and how explicit you make the worst-case bounds.

This post grounds that duality in classic patterns and shows how they translate,
using RK0 primitives as concrete examples (`kMutex*`, `kSemaphore*`,
`kTaskEvent*`, `kMesgQueue*`, `kMailbox*`, `kPort*`).

## One Table That Explains the Duality

If you like thinking in events:

| Events view | Threads/monitors view |
|---|---|
| event handlers accepted by a handler | monitors (serialised modules) |
| `SendMessage` / `AwaitReply` | procedure call (or fork/join) |
| `SendReply` | return from procedure |
| waiting for messages | waiting on condition variables |

The key observation is that a monitor and an event handler are both "a place
where state changes happen one-at-a-time". The rest is interface.

## Determinism: Where the Worst-Case Wait Lives

For a system to be deterministic you need **a bounded waiting story**:

- Shared state tends to be bounded by:
  - maximum lock / critical-section time, and
  - priority inversion (mitigate with inheritance like `RK_INHERIT`).
- Message passing tends to be bounded by:
  - maximum queue backlog, and
  - maximum handler/service time.

Neither style is automatically deterministic. Both become deterministic when you
bound the right things and keep the rules consistent.

## Latch vs Counter vs Queue (Semantics)

Before picking an implementation, be precise about the delivery semantics you
need. This matters more than the "shared vs messages" label.

- **Latch (coalescing, level-triggered):** event flags. Multiple sets of the
  same flag collapse into "flag is set". Great for "predicate became true" and
  for coalescing bursts, but not suitable when you must count every occurrence.
  In RK0 this is `kTaskEvent*` and the bits live in the target task's TCB
  (shared state), not in a queued message stream.
- **Counter (token accumulation):** semaphores. Each post contributes one unit
  (up to a maximum). Great for "count resources/occurrences". In RK0 this is
  `kSemaphore*`.
- **Queue (ordered payloads):** message queues and ports. Each send is a
  distinct item; ordering is preserved. Great when every event matters and
  carries data, but backlog becomes part of your timing budget. In RK0 this is
  `kMesgQueue*` and `kPort*`.
- **Latest-wins (bounded backlog = 1):** one-slot overwrite queues/mailboxes.
  Great for snapshots/config where "latest is what matters". In RK0 this is
  `kMesgQueuePostOvw` (and mailbox helpers built on the same queue).

## Six Classic Shared-State Patterns and Their Message Duals

Each section below shows the shared-state pattern, then the message-passing
counterpart. These are mechanism-agnostic translations; RK0 names are used only
as concrete anchors.

### 1) Mutex-Protected Object <-> Active Object (Owner + Inbox)

Shared state is the familiar "struct + mutex":

```c
#include <kapi.h>

typedef struct {
    RK_MUTEX mu;
    ULONG x;
} counter_t;

static counter_t g;

void counter_init(void) {
    kMutexInit(&g.mu, RK_INHERIT);
    g.x = 0;
}

void counter_add(ULONG d) {
    kMutexLock(&g.mu, RK_WAIT_FOREVER);
    g.x += d;
    kMutexUnlock(&g.mu);
}
```

Message dual: one task owns `x` and exposes operations by receiving command
messages.

```c
#include <kapi.h>

typedef struct { ULONG op; ULONG arg; } cmd2_t; /* 2 words */
enum { OP_ADD = 1 };

static RK_MESG_QUEUE inbox;
RK_DECLARE_MESG_QUEUE_BUF(inbox_buf, cmd2_t, 8);
static ULONG x;

void counter_owner(void *arg) {
    (void)arg;
    cmd2_t m;
    for (;;) {
        kMesgQueueRecv(&inbox, &m, RK_WAIT_FOREVER);
        if (m.op == OP_ADD) x += m.arg;
    }
}
```

Determinism trade:
- Mutex: bound hold time; inheritance bounds inversion.
- Inbox: bound depth; bound handler time.

### 2) Condition Variable (wait for predicate) <-> Wait for message/event

Shared state typically looks like "wait until predicate is true":

```c
lock(mu);
while (!pred()) wait(cond, mu);
use_state();
unlock(mu);
```

Message dual: the predicate becomes "a message has arrived" or "a flag is set".
In RK0, directed task event flags often act like coalescing conditions:

```c
#include <kapi.h>

#define EV_READY RK_EVENT_1

/* waiter */
kTaskEventGet(EV_READY, RK_EVENT_ANY, 0, RK_WAIT_FOREVER);

/* signaler (after making the predicate true) */
kTaskEventSet(waiterHandle, EV_READY);
```

Determinism tip: coalescing helps. A burst of "ready" signals stays "ready"
instead of creating an unbounded queue of wakeups.

### 3) Semaphore Tokens <-> Token Messages

A semaphore is already a message channel where the only message is "one permit".

Shared state:

```c
kSemaphorePend(&slots, timeout); /* receive permit */
kSemaphorePost(&slots);          /* send permit */
```

Message dual: represent permits explicitly as queue items when you want routing,
ownership, or multi-hop flow control.

```c
ULONG t;
kMesgQueueRecv(&tokens, &t, timeout);
kMesgQueueSend(&tokens, &t, RK_NO_WAIT);
```

Determinism tip: use bounded token pools and avoid blocking in ISR paths.

### 4) Bounded Buffer (producer/consumer) <-> Bounded Channel

Shared state bounded buffer is usually mutex + `not_empty` + `not_full`:

- `slots` semaphore bounds capacity.
- `items` semaphore indicates queued work.
- mutex protects indices and payload.

Message dual is the bounded queue itself:

```c
#include <kapi.h>

static RK_MESG_QUEUE q;
RK_DECLARE_MESG_QUEUE_BUF(qbuf, ULONG, 8);

void q_init(void) { kMesgQueueInit(&q, qbuf, 1, 8); }

RK_ERR put(ULONG v, RK_TICK tmo) { return kMesgQueueSend(&q, &v, tmo); }
RK_ERR get(ULONG *v, RK_TICK tmo) { return kMesgQueueRecv(&q, v, tmo); }
```

Determinism pitfall: one FIFO queue shared by mixed priorities can cause
head-of-line blocking (high-priority work stuck behind old low-priority work).
If priority response matters, shard by priority/criticality or use a mechanism
with priority-correct service.

### 5) Monitor Call/Return <-> RPC (Send/AwaitReply/Reply/Done)

Monitors make concurrency look like procedure calls: call a method, maybe wait,
return a result. The message dual is RPC.

RK0 ports match the event vocabulary directly: send + await reply, then server
replies and ends the transaction.

```c
#include <kapi.h>

static RK_PORT port;
RK_DECLARE_PORT_BUF(portbuf, RK_PORT_MESG_4WORD, 8);

/* client-side call */
RK_ERR call(ULONG *msgWordsPtr, RK_MAILBOX *replyBox, UINT *reply, RK_TICK tmo) {
    return kPortSendRecv(&port, msgWordsPtr, replyBox, reply, tmo);
}

/* server-side accept + return */
void server(void *arg) {
    (void)arg;
    RK_PORT_MESG_4WORD msg;
    for (;;) {
        kPortRecv(&port, &msg, RK_WAIT_FOREVER);
        kPortReplyDone(&port, (ULONG const *)&msg, 0U);
    }
}
```

Determinism tip: ports/RPC make waiting explicit and encourage bounded
transactions. Keep server work bounded; offload long processing to background
workers if needed.

### 6) Read-Mostly Config <-> Publish Latest (Overwrite / Most-Recent)

Shared state read-mostly config often uses "copy under a short lock". The message
dual is snapshot publication where "latest wins".

RK0 has overwrite semantics for one-slot queues:

```c
/* one-slot queue only */
kMesgQueuePostOvw(&cfgQ, &cfg);
kMesgQueuePeek(&cfgQ, &cfg);
```

Determinism win: backlog is bounded to 1. You are choosing semantics:
"latest value matters" rather than "process every update".

## A Translation Recipe (Shared State -> Messages)

To convert a shared-state module into message passing:

1. Pick the owner (one task owns the state).
2. Define command messages (fixed-size if you care about bounded runtime).
3. Replace direct state access with `Send` (and optionally `SendRecv`).
4. Put all state mutations in the owner's receive loop.
5. Decide overload behaviour:
   - block with timeout,
   - drop,
   - overwrite (most-recent),
   - or backpressure via token/semaphore.
6. Audit bounds:
   - queue depth,
   - message size/copy time,
   - handler execution time.

## Closing: Determinism is a Budget

Shared state spends its determinism budget in lock hold time and inversion.
Message passing spends it in backlog and service time. Both can be engineered to
be predictable; the right choice is the one whose budget is easiest to make
explicit in your system.
