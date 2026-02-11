RK0 - CHANGELOG 

VERSION 0.6.14-dev
11 Feb 26

- BUG FIXES:
1. Blocking calls would accept >  `RK_MAX_PERIOD` and not return an error.
2. Flushes and Wakes could potentially grab a NULL pointer as the chosenTCBPtr.

- FEATURE CHANGES:
1. If PORTS are enabled each TCB has a dedicated pointer to a Mailbox.
   Then, the API kPortSendRecv is reduced in one paramater `replyBoxPtr`.
   But each client has to register a mailbox addresss using the API 
   `kRegisterMailbox`.

2. Wakes/Flush in SleepQueues are now deferred to 
   Post-processing function that runs within PendSV, if the number of tasks
   to be waken is higher than a ratio of the total number of tasks defined in
   kconfig.h on the parameter `RK_CONF_SLEEPQ_WAKE_INLINE_THRESHOLD`. This 
   makes the call asynchronous but avoids enabling/disabling interrupts for 
   each loop. 

3. Flushes in semaphores are always deferred and can be called within ISRs.

4. kSchLock/kSchUnlock are no longer inlined functions.

5. kCondVar broadcast/signal/wait are no longer inlined functions and cannot
   be called within ISRs. This was indicated in the docbook but not enforced
   on code.

6. SysTask PostProcesTask is called again TimHandleTask.

7. Small optimisations for Mailbox and Binary Semaphores were added.

- ENVIRONMENT CHANGES

1. Makefile now supports ARMv6M Cortex-MO QEMU.