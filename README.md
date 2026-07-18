[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.20.2-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
[![Docs](https://img.shields.io/badge/docs-HTML-orange)](https://antoniogiacomelli.github.io/RK0/)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

### **Zero surprises: Not a minimal RTOS...**

<img src="https://github.com/user-attachments/assets/5d5a15bf-9a3b-4abb-94f7-6449243e8948" width="7%" align="right" alt="image">

RK0 is centred on the idea that concurrency requirements are the major commonality accross real-time systems. It supports real-time application programmers by moving worst-case coordination mechanics into the kernel. Programmers express the dependency being created, and the kernel enforces the corresponding scheduling behaviour. This is important because real-time correctness is often lost in worst-case scenarios: priority inversion, blocked producers, nested ownership, timeout races, and delayed receivers, among others, are difficult to manage reliably at the application level.

* [Service Map](https://github.com/antoniogiacomelli/RK0/wiki/Service-Semantics): a must-read for developing

* [RK0 Docbook](https://antoniogiacomelli.github.io/RK0/): compreehensive document with design internals, architecture, caveats and usage examples
  
* [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki): useful information such as setting up environment (VSCode/QEMU) (Linux/Win/MacOS), functional packages for Nucleo-boards (M0/M3/M4), some profiling metrics.

* [RK0 Blog](https://kernel0.org/blog/): blogs about RK0 and systems programming in general 


---
# Quick Start (QEMU)

Prerequisites:
- ARM GNU Toolchain (`arm-none-eabi-gcc, arm-none-eabi-gdb / gdb-multiarch (Debian)`)
- QEMU for ARM (`qemu-system-arm`)

Build and run the RK0 demo on QEMU:

```shell
git clone https://github.com/antoniogiacomelli/RK0.git
cd RK0
make arch=armv6m|armv7m qemu
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
