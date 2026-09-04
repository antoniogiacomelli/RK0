[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.73.1-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
[![Docs](https://img.shields.io/badge/docs-HTML-orange)](https://antoniogiacomelli.github.io/RK0/)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

***

### **Zero surprises: Not a minimal RTOS...**

<img src="https://github.com/user-attachments/assets/5d5a15bf-9a3b-4abb-94f7-6449243e8948" width="7%" align="right" alt="image">

*Interaction-oriented: an RK0ish application code describes how tasks interact rather than delegating to application code to compose generic services. Recurring coordination patterns for real-time applications are totally defined by the relationship between concurrency entities (Tasks) and optimised to handle worst-case scenarios.*

* [RK0 Docbook](https://antoniogiacomelli.github.io/RK0/): compreehensive document with design internals, architecture, caveats and usage examples

* [Service Map](https://github.com/antoniogiacomelli/RK0/wiki/Service-Semantics): a must-read for developing

* [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki): misc of information: requirement matrix, design patterns, setting up VSCode/QEMU/GDB on Linux/Win/MacOS, profiling metrics.

* [RK0 Blog](https://kernel0.org/blog/): blogs about RK0 and systems programming in general

***


## 🔌 **Nucleo F030R8 (ARM Cortex M0) Build Environment in this branch**

> Note F030R8 has only 8KiB of RAM and 64 KiB of ROM. By default, static
> semaphores are enabled for the mailbox demo, dynamic objects stay OFF, and
> RK0 programs the board clock to 48 MHz. Set RK_CONF_SYSCORECLK to 0 only when
> linking a CMSIS system file that provides SystemCoreClock and owns clock setup.


```shell
make
make flash-f030r8 FLASH_TOOL=<st-flash|openocd|jlink|stm32programmer>
```

***

### Code Quality

RK0 source code compiles cleanly with the following GCC flags:

`-Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic`

Static Analysis (Cppcheck)  is clean with no warnings, errors, or style issues.

```shell
make cppcheck
make cppcheck-report
```

***

### Dependencies

* _RK0 compiles only with ARM GCC_.

* _The C code standard is C99_.

***

Copyright (C) 2026 Antonio Giacomelli | All Rights Reserved | [www.kernel0.org](http://www.kernel0.org) | [📫](mailto:dev@kernel0.org)
