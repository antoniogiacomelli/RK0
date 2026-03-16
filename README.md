[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.13.3-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
[![Docs](https://img.shields.io/badge/docs-HTML-orange)](https://antoniogiacomelli.github.io/RK0/)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

# About 

**RK*0*** is a lean but feature-rich, highly deterministic Real-Time Kernel for deeply embedded solutions. 

_RK0_ is not a 'minimal RTOS', because it is neither miminal nor an RTOS. It is a kernel; or a real-time executive.
Targeting firm real-time applications, it is oriented for making WCET analysable as straightforward as possible. 

The way to achieve that was enforcing predictability through semantics, optimise for worst-case over a disciplined, 
scheduler-centred design. 

This gave birth to orthogonal features, as rich as [fully priority transitive inheritance on mutexes](https://kernel0.org/2026/01/12/transitive-priority-inheritance-on-mutexes/)
and dual programming support paradigm [including priority-driven message-passing](https://antoniogiacomelli.github.io/RK0/#ports_synchronous_client_server_procedure_call). 
It is modular, composable, transparent and features do not overlap.

* [RK0 Docbook](https://antoniogiacomelli.github.io/RK0/) Service descriptions, Usage Patterns, Design Internals

* [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki) QEMU setup, Reference projects for download (Nucleo boards)

* [RK0 Blog](https://kernel0.org/blog/) provides some quick complementary reads.


---
# Quick Start (QEMU)

Prerequisites:
- ARM GNU Toolchain (`arm-none-eabi-gcc, arm-none-eabi-gdb / gdb-multiarch (Debian)`)
- QEMU for ARM (`qemu-system-arm`)

Build and run the RK0 demo on QEMU:

```shell
git clone https://github.com/antoniogiacomelli/RK0.git
cd RK0
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
