[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
![Version](https://img.shields.io/badge/version-0.12.2-blue)
![Arch](https://img.shields.io/badge/arch-ARMv6M%20%7C%20ARMv7M-informational)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

---

**Current Version: _v0.12.2_**

# About 


**RK*0*** is a lean but feature-rich, highly deterministic Real-Time Kernel for deeply embedded solutions. 

 _Supported Architectures: ARMv6M (Cortex-M0/0+) and ARMv7M (Cortex-M3/4/7)_.


## **Documentation**

* The _RK0_ Docbook provides a comprehensive system description

   * [PDF Docbook](docbook/docbook.pdf)
   * [HTML Docbook](https://antoniogiacomelli.github.io/RK0/)

*  [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki) QEMU setup, Reference projects for download (Nucleo boards)

*  [RK0 Blog](https://kernel0.org/blog/) provides some quick complementary reads.


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
