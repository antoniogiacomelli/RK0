RK0 - CHANGELOG 

VERSION 0.9.14
11 Feb 26

- Garbage pushed. 

VERSION 0.9.15
12 Feb 26

- BUG FIXES:

1. Blocking calls would accept >  `RK_MAX_PERIOD` and not return an error.

2. Flushes and Wakes could potentially grab a NULL pointer as the chosenTCBPtr.

- FEATURE CHANGES:

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

- ENVIRONMENT CHANGES

1. Makefile now supports ARMv6M Cortex-MO QEMU (BBC micro:bit)

VERSION 0.9.16
14 Feb 26

- FEATURE CHANGES:

1. PORTS reverted to explicit reply mailbox:
   `kPortSendRecv(..., replyBox, ...)` (no per-TCB mailbox attachment).

VERSION 0.9.17
15 Feb 26

- BUG FIXES:.

- FEATURE CHANGES:

1. PORTS logic moved out of `kmesgq.c` into `kport.c` for maintainability.
