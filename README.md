<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

# About 

**RK*0*** is a lean, feature-rich, highly deterministic Real-Time Kernel for deeply embedded solutions.

 _Supported Architectures: ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7)_.


## **Documentation**

* The _RK0_ Docbook provides a comprehensive system description

   * [PDF Docbook](docbook/docbook.pdf)
   * [HTML Docbook](https://antoniogiacomelli.github.io/RK0/)

*  [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki) QEMU setup, Reference projects for download (Nucleo boards)

*  [RK0 Blog](https://kernel0.org/blog/) provides some quick complementary reads.

---

## RK0 Main Features (V0.9.18-dev)

- **Scheduler: priority-preemptive**
    - O(1) choose-next algorithm.
    - 32 priorities (non-exclusive)
     
- **Synch Pack:**
  - _Semaphores_ (Counting/Binary)
  - _Mutexes_ with _fully transitive Priority Inheritance_ 
  - _Conditional Critical Regions (Condition Variable/Monitor-like constructions)_
  - _Task Event Registers_ 

- **Priority-aware message-passing**
   - _Message Queues (Mailboxes)_ for general asynchronous message-passing
   - _Ports_ for client-server synchronous RPC, message-driven priority inheritance
     
- **Most-Recent Message Protocol**
  - Asynchronous, Lock-Free, purpose-built for Real-time Control Loops
    
- **High-Precision Timers:**
   - Minimal Tick Handling overhead for Bounded Waiting, Phase-locked Periodic Releases and Callout Timers.

- **Memory Partition**:
   - Well-proven, deterministic memory allocator suitable for real-time systems.
     
- **Suits both procedural/shared-memory and message-passing paradigms**

- **Highly Modular with clean and consistent API**.

  
- _(And that wicked cool mascot)_ 



---
# Quick Start (QEMU)

Prerequisites:
- ARM GNU Toolchain (`arm-none-eabi-gcc, arm-none-eabi-gdb / gdb-multiarch (Debian)`)
- QEMU for ARM (`qemu-system-arm`)

Build and run the RK0 demo on QEMU:

```bash
git clone https://github.com/antoniogiacomelli/RK0.git
cd RK0
make arch=<armv6/7m> qemu
```

---

### Code Quality 
RK0 source code compiles cleanly with the following GCC flags:

`-Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic`

Static Analysis (Cppcheck)  is clean with no warnings, errors, or style issues.

---

### Dependencies
* _RK0 compiles only with ARM GCC_.
* _The C code standard is C99_.
  
---

Copyright (C) 2026 Antonio Giacomelli | All Rights Reserved | www.kernel0.org | [📫](mailto:dev@kernel0.org)




