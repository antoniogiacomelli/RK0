[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.19.1-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
[![Docs](https://img.shields.io/badge/docs-HTML-orange)](https://antoniogiacomelli.github.io/RK0/)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

### **Zero surprises: Not a minimal RTOS...**

<img src="https://github.com/user-attachments/assets/5d5a15bf-9a3b-4abb-94f7-6449243e8948" width="7%" align="right" alt="image">

* [Services](https://github.com/antoniogiacomelli/RK0/wiki/Service-Semantics): _RK0_ helps to meet real-time demands by a curated set of orthogonal and composable services with an expressive API. This combination results in applications that are easier to reason about runtime behaviour and a codebase with improved maintanability.

* [RK0 Docbook](https://antoniogiacomelli.github.io/RK0/): Architecture,  design internals and usage patterns.
  
* [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki): QEMU setup, Reference projects for download (Nucleo boards)

* [RK0 Blog](https://kernel0.org/blog/): some quick complementary reads.

> 🛑 RK0 has been showing a steady pattern of roughly 700 clones and around 350 unique cloners for long enough with activity that is clearly organic. Yet the project is barely being discussed. I am looking for sober technical criticism: concrete feedback on the model, design and implementation rationale. Not appraisal. Not empty flaming.

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
