[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.71.0-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
[![Docs](https://img.shields.io/badge/docs-HTML-orange)](https://antoniogiacomelli.github.io/RK0/)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

### **Zero surprises: Not a minimal RTOS...**

<img src="https://github.com/user-attachments/assets/5d5a15bf-9a3b-4abb-94f7-6449243e8948" width="7%" align="right" alt="image">

*Interaction-oriented: an RK0ish application code describes how tasks interact rather than delegating to application code to compose generic services. Recurring coordination patterns for real-time applications are totally defined by the relationship between concurrency entities (Tasks) and optimised to handle worst-case scenarios.*




* [RK0 Docbook](https://antoniogiacomelli.github.io/RK0/): compreehensive document with design internals, architecture, caveats and usage examples

* [User Manual](https://github.com/antoniogiacomelli/RK0/blob/main/rk0-user-manual.pdf): a more operational document explaining services with no design details.

* [Service Map](https://github.com/antoniogiacomelli/RK0/wiki/Service-Semantics): a must-read for developing

* [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki): misc of information, requirement matrix, design patterns, setting up VSCode/QEMU/GDB on Linux/Win/MacOS, packages for Nucleo-boards (M0/M3/M4), profiling metrics.

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

```shell
make cppcheck
make cppcheck-report
```

---

### Dependencies
* _RK0 compiles only with ARM GCC_.
* _The C code standard is C99_.
  
---

Copyright (C) 2026 Antonio Giacomelli | All Rights Reserved | www.kernel0.org | [📫](mailto:dev@kernel0.org)
