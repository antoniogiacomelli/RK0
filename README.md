# RK*0* - _The Real-Time Kernel '0'_

**RK*0*** is a lean, _highly_ deterministic Real-Time Kernel targeting deeply embedded solutions.
(Target Architecture: ARMv7M)

## Features (v0.4.0)

- Clean API with composable, modular, configurable services.
- Low-latency, deterministic Preemptive priority O(1) scheduler, with optional time-slice. 
- Inter-Task Communication: a composable rich set of synchronisation and message-passing mechanisms, designed with different best-use cases in mind
- High-precision application timers.
- Efficient fixed-size Memory Allocator (Memory Pools)
- Footprint as low as 2KB ROM and 500B RAM (core features). 

## Design and Logical Architecture

<img width="450" alt="kernel" src="https://github.com/antoniogiacomelli/RK0/blob/master/layeredkernel.png">

See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a comprehensive design description.
 
### Dependencies
* ARM-GCC, CMSIS-GCC

## Building System: RK0 with QEMU

In this [branch](https://github.com/antoniogiacomelli/RK0/tree/qemu-m3) you can find a building system of RK0 to run on QEMU-ARM (Cortex-M3).

----

> [!NOTE]
> This is a (serious) work in progress.
