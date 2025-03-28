# RK0 - _The Real-Time Kernel '0'_

**RK*0*** is a lean, _highly_ deterministic Real-Time Kernel targeting deeply embedded solutions.

## Features (v0.4.0)

- Clean API with composable, modular, configurable services.
- Low-latency, deterministic Preemptive priority O(1) scheduler, with optional time-slice. 
- Inter-Task Communication: a composable rich set of synchronisation and message-passing mechanisms, designed with different best-use cases in mind
- High-precision application timers.
- Efficient fixed-size Memory Allocator
- Footprint as low as 2KB ROM and 500B RAM (core features). 

### Logical Architecture

<img width="450" alt="kernel" src="https://github.com/antoniogiacomelli/RK0/blob/master/layeredkernel.png">

 
## Dependencies
* CMSIS-GCC
* Provided Makefile targets C Newlib.

----

> [!NOTE]
> Work in progress.
