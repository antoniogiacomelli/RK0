<h1 align="left">RK<em>0</em> - The Real-Time Kernel <em>'0'</em> <img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

## Tickless Idle Snapshot

- Added `kTicklessEntry` and `kTicklessExit` so SysTick windows expand across idle gaps without losing pending timer expirations.
- Scheduler now parks the CPU when the ready queue is empty, arming a one-shot SysTick calculated from the next timer node and capping it at the hardware reload range.
- Wakes reconstitute the kernel time base by crediting the elapsed ticks back into `kTimers`, so software timers and sleeps stay bit-for-bit accurate.
- Reuses the existing scheduler and timer queues; applications do not change APIs or timing contracts.

- Roadmap: wire the same tickless hooks into STM32 low-power primitives (STOP mode + LPTIM wake) so hardware-offloaded timing replaces SysTick when available.


**Test log**



      2 ms ::
   
       Task 3 is waiting at the barrier... 
       5 ms :: 
       Task 1 is waiting at the barrier... 
       5 ms ::
        Task 2 is waiting at the barrier... 
       8 ms ::
       
        Task 3 passed the barrier! 
      10 ms :: Task 1 passed the barrier! 
      11 ms :: Task 2 passed the barrier! 
         TICKLESS: 0% idle (0/19 ticks)
    1511 ms :: Task 1 is waiting at the barrier... 
         TICKLESS: 99% idle (1490/1496 ticks)
    2011 ms :: Task 2 is waiting at the barrier... 
         TICKLESS: 99% idle (495/497 ticks)
    3008 ms :: Task 3 is waiting at the barrier... 
    3008 ms :: Task 3 passed the barrier! 
    3008 ms :: Task 1 passed the barrier! 
    3008 ms :: Task 2 passed the barrier! 
         TICKLESS: 99% idle (995/998 ticks)
    4508 ms :: Task 1 is waiting at the barrier... 
         TICKLESS: 100% idle (1498/1498 ticks)
    5008 ms :: Task 2 is waiting at the barrier... 
         TICKLESS: 99% idle (499/501 ticks)
    6008 ms :: Task 3 is waiting at the barrier... 
    6008 ms :: Task 3 passed the barrier! 
    6008 ms :: Task 1 passed the barrier! 
    6008 ms :: Task 2 passed the barrier! 
         TICKLESS: 99% idle (999/1001 ticks)
    7508 ms :: Task 1 is waiting at the barrier... 
         TICKLESS: 99% idle (1497/1498 ticks)
    8008 ms :: Task 2 is waiting at the barrier... 
         TICKLESS: 100% idle (500/500 ticks)
    9008 ms :: Task 3 is waiting at the barrier... 
    9008 ms :: Task 3 passed the barrier! 
    9008 ms :: Task 1 passed the barrier! 
    9008 ms :: Task 2 passed the barrier! 
         TICKLESS: 99% idle (1000/1001 ticks)
   10508 ms :: Task 1 is waiting at the barrier... 
         TICKLESS: 99% idle (1499/1500 ticks)
   11008 ms :: Task 2 is waiting at the barrier... 
         TICKLESS: 100% idle (499/499 ticks)
   12008 ms :: Task 3 is waiting at the barrier... 
   12008 ms :: Task 3 passed the barrier! 
   12008 ms :: Task 1 passed the barrier! 
   12008 ms :: Task 2 passed the barrier! 
         TICKLESS: 99% idle (999/1001 ticks)
   13508 ms :: Task 1 is waiting at the barrier... 
         TICKLESS: 100% idle (1499/1499 ticks)


---

## Know it

**RK*0*** is a lean, feature-rich, highly deterministic Real-Time Kernel for deeply embedded solutions.

> - See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a thorough design description and usage examples.
> - Check the [**RK*0* Blog**](https://kernel0.org/blog) for some quick reads.

 _Supported Architectures_: **ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7)**.

---

## RK0 Main Features (0.8.0-dev)

- **O(1) Scheduler: priority preemptive (RMS)**

- **Synch Pack:**
  - _Semaphores_ (Counting/Binary)
  - _Mutexes_ with _fully transitive Priority Inheritance_ for nested locks
  - _Condition Variables_
  - _Direct Task Notifications_ 

- **Priority-aware message-passing**
   - _Message Queues (Mailboxes)_ for general asynchronous message-passing
   - _Ports_ for client-server synchronous RPC, message-driven priority inheritance
     
- **Most-Recent Message Protocol**
  
  - Asynchronous, Lock-Free, purpose-built for Real-time Control Loops
    
- **High-Precision Timers:**
  
   - Minimal Tick Handling overhead for Bounded Waiting, Periodic Sleeps and Application Timers.

- **Memory Partition**:
   - Well-proven, deterministic memory allocator suitable for real-time systems.
     
- **Suits both procedural/shared-memory and message-passing paradigms**

- **Highly Modular with clean and consistent API**.

  
- _(And that wicked cool mascot)_ 

---

## Use it

> - [**Emulated hardware**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-%E2%80%90-Running-on-QEMU): the provided Makefile in this branch targets QEMU
> - [**Nucleo boards**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-on-Nucleo-boards/)
---

## System Architecture

If no more details are to be provided, the kernel has a top and a bottom layer. On the top, the Executive manages the resources needed by the application. On the bottom, the Low-level Scheduler works as a software extension of the CPU. 

<img src="https://github.com/antoniogiacomelli/RK0/blob/docs/docs/images/images/layeredkernel.png?raw=true" width="35%">

---
## Code Quality 
RK0 source code compiles cleanly with the following GCC flags:

`-Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic`

Static Analysis (Cppcheck)  is clean with no warnings, errors, or style issues.

---

### Dependencies
* ARM-GCC, CMSIS-GCC
  
---

Copyright (C) 2025 Antonio Giacomelli | All Rights Reserved | www.kernel0.org | [📫](mailto:dev@kernel0.org)





