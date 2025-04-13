<h1 align="left">RK0 - The Real-Time Kernel 0 <img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

**RK*0*** is a lean, _highly_ deterministic Real-Time Kernel for deeply embedded solutions.

 _Supported Architectures_: ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7).

> 📂 Check [_arch/qemu-m3_](./arch/qemu-m3/) or [_arch/armv6m_](./arch/armv6m/)

> 📖 See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a thorough design description.

## Logical Arhitecture

<img src="https://github.com/antoniogiacomelli/RK0/blob/docs/docs/images/images/layeredkernel.png?raw=true" width="50%">

 ## Features (v0.4.0) 
 - Clean API with composable, modular, configurable services.
 - Low-latency, deterministic Preemptive priority O(1) scheduler, with optional time-slice.
   (4 CPU cycles pick-next algorithm).
 - Inter-Task Communication: a composable rich set of synchronisation and message-passing mechanisms, designed with different best-use cases in mind
 - High-precision application timers.
 - Efficient fixed-size Memory Allocator (Memory Pools)
 - Footprint as low as 3KB ROM and 500B RAM (core features).
   
(RK0 runs on ARMv7M, M3/4/7, supporting Float Point co-processor Unit if present).

### Dependencies
* ARM-GCC, CMSIS-GCC

---
Copyright (C) 2025 Antonio Giacomelli | All Rights Reserved | www.kernel0.org | dev@kernel0.org
 
