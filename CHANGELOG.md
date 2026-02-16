
### RK0 CHANGELONG 

#### VERSION 0.9.14 (11 Feb 26)

- Garbage pushed. Broken branch would not compile.

##### VERSION 0.9.15 (12 Feb 26)

##### BUG FIXES:

1. It compiles. Wow!

2. Blocking calls would accept >  `RK_MAX_PERIOD` silently.

3. Flushes and Wakes could potentially grab a NULL pointer as the chosenTCBPtr.

##### FEATURE CHANGES:

1. If PORTS are enabled each TCB has a dedicated pointer to a Mailbox.
   Then, the API kPortSendRecv is reduced in one paramater `replyBoxPtr`.
   But each client has to register a mailbox addresss using the API 
   `kRegisterMailbox`.
   Optionally the old behaviour remains on the api kPortSendRecvMbox.

2. Flushes on semaphores if be called from ISRs, will run asynchronously 
   on PostProcSysTask.

3. Same applies to Sleep Queues Wake/Flush.

4. kSchLock/kSchUnlock are no longer inlined functions.

5. kCondVar broadcast/signal/wait are no longer inlined functions.

6. SysTask naming aligned to PostProcSysTask.

7. Small optimisations for Mailbox and Binary Semaphores were added.

##### ENVIRONMENT/FILE TREE CHANGES

1. Makefile now supports ARMv6M Cortex-MO QEMU (BBC micro:bit)

### VERSION 0.9.16 (14 Feb 26)

#### BUG FIXES

N/A

#### FEATURE CHANGES:

1. PORTS reverted. No per-task mailbox. 

#### ENVIRONMENT/FILE TREE CHANGES

N/A

### VERSION 0.9.17 
(15 Feb 26)

### BUG FIXES:

N/A

#### FEATURE CHANGES:

1. PORTS logic moved out of `kmesgq.c` into `kport.c` for maintainability.

### VERSION 0.9.18 
(16 Feb 26)

#### BUG FIXES:

1. `kMesgQueueReset` now defers to PostProc when called from ISR and there are
   waiting tasks, returning immediately instead of waking tasks inline in ISR
   context.

#### FEATURE CHANGES:
1. Message queue reset post-processing path was aligned with system post-proc
   jobs (`RK_POSTPROC_JOB_MESGQ_RESET`) for ISR-safe deferred wake handling.

#### ENVIRONMENT/FILE TREE CHANGES

1. ARM core headers/sources were consolidated and renamed:
   `kdefs.h` + `khal.h` -> `kcoredefs.h`, and `khal.c` -> `kcore.c`
   (ARMv6-M and ARMv7-M trees).
2. Removed cmsis_gcc.h is no longer needed.