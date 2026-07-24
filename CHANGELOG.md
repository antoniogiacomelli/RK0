**0.41.0 (2026-07-23)**

*Changes*

* Port, Channel, and Rendezvous coordination now enforce the single-authority
  rule: tasks that currently own a mutex cannot enter Port
  send/receive/jam/post/reset, Channel call/accept/done, or Rendezvous
  send/receive paths.
* QEMU coverage added for `MSGQ_RQ_19`, `CHAN_RQ_06`, and `RDVZ_RQ_10`
  single-authority mutex/Port/Channel/Rendezvous rejection cases.
* Trace UART RX is interrupt-driven on QEMU targets; the UART ISR buffers input
  and wakes the trace task with a task event.
* Trace `top` now prints each task's run count alongside priority-change and
  CPU tick counters.
* Trace task history now includes effective-priority changes with actor,
  old/new priority, and nominal priority via `hist task/<name-or-pid>`.
* The QEMU microbit logger uses smaller buffers so the trace-enabled demo fits
  in 16 KiB RAM.
* The QEMU makefile now includes generated dependency files, so header changes
  rebuild affected objects.

*Bug fixes*

* `kMesgQueuePostOvw()` now reports the single-slot mailbox precondition before
  applying the Port/mutex ownership restriction.

**0.40.0 (2026-07-21)**

*Changes*

* `RK_EXCHANGE` renamed to `RK_RENDEZVOUS` to make explicit it is unbuffered named send operation.
* Application demo switched to request handoff via rendezvous and trace output
  now includes rendezvous events.
* Trace `top` shows `PCHG`, the number of effective-priority changes recorded
  per task.
* QEMU CI now runs the `krendezvous` unit module on armv7m and armv6m, with
  `RDVZ_RQ_01` through `RDVZ_RQ_09` requirement IDs.

*Bug fixes*

* Sender-side rendezvous timeout invalidates the timed-out message for the
  receiver; a later receive cannot consume the stale pointer.

*TODO* 
Update Docbook. You can find information of TRACE and RENDEZVOUS services 
on `kapi.h`, Wiki and `application.c`.

**0.30.0 (2026-07-19)**

*Changes*

* Feature: RK_EXCHANGE Synchronous task-to-task exchange objects.
* Feature: TRACE console, object status and history (UART)
  
*Bug fixes*
Application timer list was nested with timeout list. 

**0.20.2 (2026-07-18)**

*Bug fixes*

* potentially undefined `1UL << 32` operation for ready bitmap is no longer executed.
* (in this toolchain/arch it was fine).
* on a chain of mutexes if one happens to not have prio inh enabled
  the PIP doesnt follow through.

**0.20.1 (2026-07-16)**

*Changes*

* `RK_CHANNEL` accept/done now uses an explicit request state transition and a
  single channel-owned active request, with dedicated busy/not-active errors.
* `RK_CHANNEL` finite timeouts can abandon active requests; abandoned request
  envelopes remain channel-owned until the server calls `kChannelDone()`.

**0.19.3 (2026-07-14)**

*Bug fixes*

* `RK_CHANNEL` now cancels timed-out client requests consistently: stale route
  words are removed or skipped, and late server completion frees accepted
  timed-out requests without waking the client.

**0.19.2 (2026-04-26)**

*Bug fixes*

* Scheduler lock nesting fixed.

**0.19.1 (2026-04-20)**

*Changes*

* Improved priority boosting on PORTs mechanism.
* Exposed PORT API.

**0.19.0 (2026-04-18)**

*Changes*

* Removed Task Mail (`kMail*`) API and core module.


**0.18.1 (2026-04-16)**

*Bug fixes*

* Enforced static/dynamic task termination policy: only runtime-spawned dynamic tasks can be terminated; terminating static tasks now returns `RK_ERR_INVALID_OBJ`.


**0.18.0 (2026-04-13)**

*Changes*

* Added support to dynamic tasks.


**0.17.0 (2026-04-04)**

*Changes*

* Cleaned blocking paths.

*Bug fixes*

* `logError`  fails immediately.

**0.16.1 (2026-03-30)**

*Changes*

* `RK_CHANNEL` replies now wake blocked requesters from a channel-owned waiting queue.
* `RK_REQ_BUF` no longer requires an event flag for CHANNEL call/reply flow.

*Bug fixes*

* Logger now marks truncated lines with `?`.

**0.16.0 (2026-03-29)**

*Changes*

* `RK_PORT` is deprecated. Procedure calls happen on `RK_CHANNEL` using a pool of request buffers.
* Semaphore flush is deprecated.
* `RK_MAILBOX` is deprecated.

*Bug fixes*

* Hardened initialisation check.

**0.15.0 (2026-03-22)**

*Changes*

* Timeout-node handling cleanup and hardening.

**0.14.2 (2026-03-19)**

*Changes*

* Port RPC now has an implicit reply route bound to the caller task, so clients no longer need to create and pass an explicit reply mailbox.

```c
/* before */
RK_MAILBOX replyBox;
UINT replyCode = 0U;

kMailboxInit(&replyBox);
kMailboxSetOwner(&replyBox, clientHandle);
kPortSendRecv(&port, (ULONG *)&req, &replyBox, &replyCode, RK_WAIT_FOREVER);

/* now */
UINT replyCode = 0U;

kPortSendRecv(&port, (ULONG *)&req, &replyCode, RK_WAIT_FOREVER);
```

**0.14.1 (2026-03-17)**

*Bug fixes*

* Priority inheritance now requeues a ready mutex owner so its priority change is reflected in the ready queue ordering.

**0.14.0 (2026-03-17)**

*Changes*

* Deferred signals (DSIGNAL) deprecated.

**0.13.2 (2026-03-15)**

*Changes*

* Task mailbox module renamed to `ktaskmail` to avoid confusion with the general `RK_MAILBOX` exchange API; unit-test module/scripts updated accordingly.

**0.13.1 (2026-03-13)**

*Bug fixes*

* kMesgQueueReset defers only from ISR to avoid postproc re-enqueue loops.
* KASR unit harness seeds expected auxiliary tasks to satisfy user-task count.

*Feature changes*

* N/A

*Environment / file tree changes*

* N/A

**0.13.0 (2026-03-11)**

*Bug fixes*

* N/A

*Feature changes*

* Port message structs renamed so numeric suffix reflects payload words (0/2/4).
* Added Task Mailbox

*Environment / file tree changes*


* N/A

**0.12.2 (2026-02-26)**

*Bug fixes*

* N/A

*Feature changes*

* Refactored message queues for two explicit waiting lists (senders/receivers).

*Environment / file tree changes*

* N/A

**0.12.1 (2026-02-26)**

*Bug fixes*

* Number of system tasks depends on `RK_CONF_DSIGNAL` config switch.

*Feature changes*

* N/A


*Environment / file tree changes*

* N/A

**0.12.0 (2026-02-25)**

*Bug fixes*

* N/A

*Feature changes*

* Major refactor of ASR Signals now called Deferred Signals (FIFO delivery, fixed 32 IDs, bounded per-task queue).

*Environment / file tree changes*

* N/A

**0.11.0 (2026-02-23)**

*Bug fixes*

* N/A

*Feature changes*

* Reverted ASRs to a SIGNAL = { sigID, sigInfo, sigHander} record
  enqueued and handled asynchronously (priority or FIFO).

*Environment / file tree changes*

* N/A

**0.10.1 (2026-02-22)**

*Bug fixes*

* N/A

*Feature changes*

* Allowed asynchronous signals to not be queue/siginfo base but also a simple
  32-bit register that is ordered by priority.

*Environment / file tree changes*

* N/A

**0.10.0 (2026-02-22)**

*Bug fixes*

* Scheduler yield  defers the context switch while the scheduler is locked
  (`RK_gPendingCtxtSwtch`), avoiding a PendSV request inside CRs.

*Feature changes*

* Added asynchronous task signals (ASR) (`kSignalAsynch*`).

*Environment / file tree changes*

* GitHub Actions CI

**0.9.19 (2026-02-18)**

*Bug fixes*

* `kPortSendRecv` now marks timed-out client waits as stale reply-mailbox state,
  and clears stale mailbox content at the start of the next call.

* `kPortReply` now uses a non-blocking mailbox post (`RK_NO_WAIT`) so a late
  server reply fails fast (`RK_ERR_MESGQ_FULL`) instead of blocking indefinitely
  when the client has already timed out.

*Environment / file tree changes*

* N/A

**0.9.18 (2026-02-16)**

*Bug fixes*

* `kMesgQueueReset` now defers to PostProc when called from ISR and there are
  waiting tasks, returning immediately instead of waking tasks inline in ISR
  context.

*Feature changes*

* Message queue reset post-processing path was aligned with system post-proc jobs
  (`RK_POSTPROC_JOB_MESGQ_RESET`) for ISR-safe deferred wake handling.

*Environment / file tree changes*

* ARM core headers/sources were consolidated and renamed:

  * `kdefs.h` and `khal.h` -> `kcoredefs.h`

  * `khal.c` -> `kcore.c` (ARMv6-M and ARMv7-M trees)

* `cmsis_gcc.h` is no longer needed.

**0.9.17 (2026-02-15)**

*Bug fixes*

* N/A

*Feature changes*

* PORTS logic moved out of `kmesgq.c` into `kport.c` for maintainability.

*Environment / file tree changes*

* N/A

**0.9.16 (2026-02-14)**

*Bug fixes*

* N/A

*Feature changes*

* PORTS reverted. No per-task mailbox.

*Environment / file tree changes*

* N/A

**0.9.15 (2026-02-12)**

*Bug fixes*

* Blocking calls would accept values above `RK_MAX_PERIOD` silently.

* Flushes and wakes could potentially grab a NULL pointer as `chosenTCBPtr`.

*Feature changes*

* If PORTS are enabled, each TCB has a dedicated pointer to a mailbox.
  Then, the `kPortSendRecv` API is reduced to one parameter (`replyBoxPtr`).
  Each client now must register a mailbox address using `kRegisterMailbox`.
  Optionally, the old behaviour remains with `kPortSendRecvMbox`.

* Flushes on semaphores called from ISRs now run asynchronously on
  `PostProcSysTask`.

* Same applies to sleep queue wake/flush.

* `kSchLock`/`kSchUnlock` are no longer inline functions.

* `kCondVar` broadcast/signal/wait are no longer inline functions.

* System task naming aligned to `PostProcSysTask`.

* Small optimisations for mailbox and binary semaphores were added.

*Environment / file tree changes*

* Makefile now supports ARMv6-M Cortex-M0/QEMU (BBC micro:bit). n/
