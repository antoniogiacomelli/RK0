# RK0 - _Real-Time Kernel '0'_

**RK*0*** is a lean, _highly_ deterministic Real-Time Kernel targeting deeply embedded solutions.

## Features (v0.4.0)

- Clean API with composable, modular, configurable services.
- Low-latency, deterministic Preemptive priority O(1) scheduler, with optional time-slice. 
- Inter-Task Communication: a composable rich set of synchronisation and message-passing mechanisms, designed with different best-use cases in mind
- High-precision application timers.
- Fixed-size block efficent memory allocator.
- Footprint as low as 5KB. 

*Logical Architecture:*

<img width="450" alt="kernel" src="https://github.com/antoniogiacomelli/K0BA_Lite/blob/master/layeredkernel.png">

 
## Dependencies

Currently the kernel _depends_ on CMSIS:

* CMSIS-GCC
* CMSIS-Core
