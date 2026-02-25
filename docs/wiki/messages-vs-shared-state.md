# Messages vs Shared State (Duality)

You may want to read [this](https://kernel0.org/message-passing-overview)
for background on message passing and how they are 'different' on systems without [isolated processes](https://kernel0.org/2025/05/16/about-processes-tasks-and-threads/).

In RK0, all tasks execute over shared memory. The distinction between *shared-state* and *message passing* is therefore not about memory isolation, but about **ownership, waiting, and where protocol logic resides**.

The debate is not about which model is “more deterministic”. Both require:

- Ordering  
- Exclusion  
- Bounded waiting  

The difference is where those guarantees are expressed.

---

## Vocabulary Map (Events ↔ Monitors)

“Event-driven” here refers to a task receiving an information context and deciding how to act — similar to a server loop.

A “monitor” refers to serialised access to a shared state guarded by mutual exclusion, where threads:

- enter one at a time  
- read or modify shared state  
- decide whether to act or wait  
- rely on another thread to signal progress  

| Events view | Threads / monitors view |
|-------------|------------------------|
| Event handler | Monitor (serialised module) |
| `SendMessage` / `AwaitReply` | Procedure call |
| `SendReply` | Return from procedure |
| Waiting for messages | Waiting on condition variables |

## Latch vs Counter vs Queue vs Stateless Wake

The semantic behaviour of primitives matters more than the
“shared vs message” label.

### Sleep Queues — Stateless Wake

Sleep queues hold only blocked tasks; they store no event state.

A wake operation affects only tasks currently sleeping.  
If no task is sleeping at the time of the wake, the wake is lost.

Properties:

- No memory of past events  
- No coalescing  
- No accumulation  
- No payload  

Appropriate for:

- Building condition variables  
- Internal synchronisation patterns  
- Cases where missed wakes are acceptable  

Not appropriate when the event must be remembered.

---

### Task Events — Latch (Level-Triggered, Coalescing)

Task Events represent state stored in the task control block.

- Bits represent predicates  
- Repeated sets collapse into “bit is set”  
- `get` clears required bits  

Properties:

- Level-triggered  
- Coalescing  
- Remembered until cleared  
- No multiplicity  

Appropriate for:

- “Predicate became true”  
- State transitions  

Not appropriate for counting occurrences.

---

### Binary Semaphore — Single Token

A binary semaphore represents at most one outstanding event.

- Value is 0 or 1  
- Multiple posts do not accumulate  

Properties:

- Edge-triggered  
- One outstanding token  
- No payload  

Appropriate for:

- One-shot availability  
- Single-resource gating  

---

### Counting Semaphore — Bounded Counter

A counting semaphore represents a bounded number of tokens.

- Posts accumulate up to a maximum  
- Pends consume tokens  

Properties:

- Edge-triggered  
- Accumulating  
- No payload  

Appropriate for:

- Resource pools  
- Counted events  
- Capacity control  

---

### Message Queues / Ports — Ordered Channel

Message queues and ports are ordered FIFO channels.

- Payload preserved  
- Ordering preserved  
- Each message processed  

Properties:

- Edge-triggered  
- Accumulating  
- Payload-carrying  
- Ownership explicit  

Appropriate for:

- Commands  
- Requests  
- Protocol sequencing  
- Data transport  

---

### One-Slot Overwrite — Latest Value

Overwrite semantics provide a bounded backlog of one.

- Only the most recent value is retained  
- Previous values are discarded  

Properties:

- Level-triggered  
- No accumulation  
- Payload-carrying  
- Bounded backlog = 1  

Appropriate for:

- Configuration snapshots  
- Status publishing  
- Read-mostly state  

---

### Deferred Signals — Resume-Bound Notification

Deferred Signals (DSignals) are queued task-directed notifications delivered only at the READY→RUNNING transition.

- FIFO within bounded queue  
- Small payload (`ULONG` or `void*`)  
- Not delivered while RUNNING  
- No protocol semantics  

Properties:

- Edge-triggered  
- Bounded accumulation  
- Resume-bound dispatch  
- No asynchronous diversion  

Appropriate for:

- Deferred side-effect notification  
- “Owner refresh” before execution  
- Bounded callback-style bookkeeping  