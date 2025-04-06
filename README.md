<h1 align="left">RK0 - The Real-Time Kernel 0 <img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

**RK*0*** is a lean, _highly_ deterministic Real-Time Kernel for deeply embedded solutions.
(Target Architecture: ARMv7M)

 📖 See the [**RK*0* Docbook**](https://antoniogiacomelli.github.io/RK0/) for a thorough design description.

> Logical Arhitecture

> ![image](https://github.com/antoniogiacomelli/RK0/blob/docs/docs/images/images/layeredkernel.png)

 ## Features (v0.4.0) 
 - Clean API with composable, modular, configurable services.
 - Low-latency, deterministic Preemptive priority O(1) scheduler, with optional time-slice.
   (4 CPU cycles are needed to determine the next task to be dispatched).
 - Inter-Task Communication: a composable rich set of synchronisation and message-passing mechanisms, designed with different best-use cases in mind
 - High-precision application timers.
 - Efficient fixed-size Memory Allocator (Memory Pools)
 - Footprint as low as 3KB ROM and 500B RAM (core features).
   
(RK0 runs on ARMv7M, M3/4/7, supporting Float Point co-processor Unit if present).

### Dependencies
* ARM-GCC, CMSIS-GCC

---

# Building System for QEMU 

For a first contact there is a simple building system to try RK0 using QEMU

## Requirements

- ARM GCC Toolchain (`arm-none-eabi-gcc`) (the files here do not contain any Newlib syscalls, you need to add it yourself.)
- QEMU for ARM (`qemu-system-arm`)
 
## Target Platform

This branch uses the `lm3s6965evb` QEMU machine. This emulates the Texas Instruments Stellaris LM3S6965, based on ARM Cortex-M3. 

## Project Structure

```
├── Src/                # 🐰 source files
├── Inc/                # 🐰 header files
├── app/                # Sample application
│   ├── Src/            # Application source files
│   └── Inc/            # Application header files
├── Build/              # Build output directory 
├── Lib/                # Output directory for the compiled library  
├── Linker/             # Linker scripts (these are created during build using the .ld in the root directory)
├── main.c              # Application entry point
├── startup_cortexm.c   # MCU startup code
├── cortex_m_generic.ld # Generic linker script
├── lm3s6965_flash_M3.ld # Target-specific linker script
└── Makefile            # Build system
```

## Building and Running

### Build for Cortex-M3 and Run in QEMU

Make sure the `Build`, `Lib` and `Linker` directories exist. 

```bash
make qemu-m3
```

> **Note**: The `lm3s6965evb` machine model in QEMU is specifically a Cortex-M3 implementation. The options for M4 and M7 _will not work without appropriate linker scripts and QEMU machine options_.

> For debugging use `make qemu-debug` to create a GDB target; on another terminal issue `make gdb` to connect to the local target.

## Customising

 You can adjust the application by modifying the files in the `app/Src` directory. Tasks are defined in `app/Src/application.c`. Also you need to configure the kernel in `Inc/kconfig.h` and `Inc/kenv.h`. 

This is a minimal sample with 3 tasks writing on the UART peripheral.
(on QEMU-ARM 9.2.0/macOS 15.2)

![image](https://github.com/user-attachments/assets/9059c565-80ba-4829-9a45-683f4a7312c2)

 Here, GDB integrated to VSCodium -- with the extension Cortex-Debug. 

![image](https://github.com/user-attachments/assets/b9074038-06b3-49ee-a693-1fe4ee3568a9)

### Tutorial for QEMU+VsCode on Windows:

[Windows tutorial](https://github.com/antoniogiacomelli/RK0/wiki/Running-RK0-on-QEMU-with-integrated-debugging-to-VSCode-on-Windows)

Copyright (C) 2025 Antonio Giacomelli | All Rights Reserved | www.kernel0.org | dev@kernel0.org
 
