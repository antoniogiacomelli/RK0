<h1 align="left">RK<em>0</em> - The Real-Time Kernel <em>'0'</em> <img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

## Know it

**RK*0*** is a lean, feature-rich, highly deterministic Real-Time Kernel for deeply embedded solutions.

> - See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a thorough design description and usage examples.
> - Check the [**RK*0* Blog**](https://kernel0.org/blog) for some quick reads.

 _Supported Architectures_: **ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7)**.


## RK0 Main Features (0.8.3-dev)

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
## Quick Start (QEMU)

Prerequisites:
- ARM GNU Toolchain (`arm-none-eabi-gcc, arm-none-eabi-gdb / gdb-multi-arch (Debian)`)
- QEMU for ARM (`qemu-system-arm`)

Build and run the RK0 demo on QEMU:

```bash
git clone https://github.com/antoniogiacomelli/RK0.git
cd RK0
make qemu
```

---

## Detailed guide: up and running

> - [**Emulated hardware**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-%E2%80%90-Running-on-QEMU):  build and debug on VSCode (Win/macOS/Linux)
> - [**Nucleo boards**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-on-Nucleo-boards/): real hardware builds
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





