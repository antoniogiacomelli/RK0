<h1 align="left">RK<em>0</em> - The Real-Time Kernel <em>'0'</em> <img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

## Know it

**RK*0*** is a lean, feature-rich, highly deterministic Real-Time Kernel for deeply embedded solutions.

> - See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a thorough design description and usage examples.
> - Check the [**RK*0* Blog**](https://kernel0.org/blog) for some quick reads.

 _Supported Architectures_: **ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7)**.

---

## RK0 Main Features (0.8.0-dev)

- **O(1) Scheduler: priority preemptive (RMS)**

- **Suits both procedural/shared-memory and message-passing paradigms**

- **Synch Pack:**
  - _Semaphores_ (Counting/Binary)
  - _Mutexes_ with _fully transitive Priority Inheritance_ for nested locks
  - _Condition Variables_
  - _Direct Task Notifications_ 

- **Priority-aware message-passing**
   - _Message Queues (Mailboxes)_ for general asynchronous message-passing
   - _Ports_ for client-server synchronous RPC
     
- **Most-Recent Message Protocol**
  
  - Asynchronous, Lock-Free, purpose-built for Real-time Control Loops
    
- **High-Precision Timers:**
  
   - Minimal Tick Handling overhead for Bounded Waiting, Periodic Sleeps and Application Timers. 


- **Highly Modular with clean and consistent API**.

  
- _(And that wicked cool mascot)_ 

---

## Use it

> - [**Emulated hardware**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-%E2%80%90-Running-on-QEMU): the provided Makefile in this branch targets QEMU
> - [**Nucleo boards**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-V0.6.4-on-NUCLEO%E2%80%90F103RB)
---

## System Architecture

If no more details are to be provided, the kernel has a top and a bottom layer. On the top, the Executive manages the resources needed by the application. On the bottom, the Low-level Scheduler works as a software extension of the CPU. Together, they implement the Task abstraction — the Concurrency Unit that enables a multitasking environment.

In systems design jargon, the Executive enforces policy (what should happen). The Low-level Scheduler provides the mechanism (how it gets done). The services are the primitives that transform policy decisions into concrete actions executed by the Scheduler.


<img src="https://github.com/antoniogiacomelli/RK0/blob/docs/docs/images/images/layeredkernel.png?raw=true" width="50%">

---
## Code Quality 
RK0 source code compiles cleanly with the following GCC flags:

`-Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic`

Static Analysis (Cppcheck)  is clean with no warnings, errors, or style issues.

---

### Dependencies
* ARM-GCC, CMSIS-GCC
  
---
Copyright (C) 2025 Antonio Giacomelli | All Rights Reserved | www.kernel0.org 

[📫](mailto:dev@kernel0.org)
 
