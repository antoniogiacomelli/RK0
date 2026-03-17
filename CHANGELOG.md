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
