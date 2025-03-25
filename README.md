# K0BA Lil' 

**K0BA** (_Kernel 0 For emBedded Applications_) is a _highly_ deterministic Real-Time Kernel aiming constrained MCU-based solutions. 

## Features (v0.4.0)

- Clean API with composable, modular, configurable services.
- Low-latency, deterministic Preemptive priority O(1) scheduler, with optional time-slice. 
- Synchronisation services: Semaphores, Mutex w/ priority inheritance, Condition Variables, Event Groups and Direct Task Signals (Binary Semaphore and Flags).
- Message Passing: synchronous and asynchronous message passing (Queues, Streams and 'Pump-Drop' Buffers)
- High-precision application timers.
- Fixed-size block efficent memory allocator.
- Footprint as low as 5KB. 

*Logical Architecture:*

<img width="450" alt="kernel" src="https://github.com/antoniogiacomelli/K0BA_Lite/blob/master/layeredkernel.png">

 
## Dependencies

Currently the kernel _depends_ on CMSIS:

* CMSIS-GCC
* CMSIS-Core
