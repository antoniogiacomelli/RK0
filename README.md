<h1 align="left">RK0 - The Real-Time Kernel 0 <img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

**RK*0*** is a lean, _highly_ deterministic Real-Time Kernel for deeply embedded solutions.

> - See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a thorough design description.
> 
 _Supported Architectures_: **ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7)**.

---

## Using it

> - [**Emulated hardware**](https://github.com/antoniogiacomelli/RK0/wiki/RK0-%E2%80%90-Running-on-QEMU): the provided Makefile targets QEMU

> - [**Hands-on: RK0 on NUCLEO-F030R8**](https://kernel0.org/2025/04/15/deploying-rk0-on-a-real-board-nucleo-f030r8/): The steps depicted here are useful for any setup.


---

## Logical Arhitecture

If no more details are to be provided, the kernel has a top and a bottom layer - on the top, the Executive manages the resources needed by the application; on the bottom, the Low-level Scheduler works as a software extension of the CPU.

<img src="https://github.com/antoniogiacomelli/RK0/blob/docs/docs/images/images/layeredkernel.png?raw=true" width="50%">

 ## Features (v0.4.0-dev) 
 - Priority Preemptive Scheduler
   (Low-latency, O(1): 4 CPU cycles pick-next algorithm).
 - FPU support.
 - Inter-Task Communication: a composable rich set of synchronisation and message-passing mechanisms, designed with different best-use cases in mind
 - High-precision application timers.
 - Efficient fixed-size Memory Allocator (Memory Pools)
 - Footprint as low as 3KB ROM and 500B RAM (core features).
 - Clean uniform API.
   
### Dependencies
* ARM-GCC, CMSIS-GCC

  
---
Copyright (C) 2025 Antonio Giacomelli | All Rights Reserved | www.kernel0.org | dev@kernel0.org
 
