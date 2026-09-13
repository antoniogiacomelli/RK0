[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.80.1-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
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

# Running

## Build Model

RK0 separates the CPU architecture from the board/runtime:

* `ARCH=armv7m` or `ARCH=armv6m` selects the Cortex-M architecture port.
* `PLATFORM=qemu` selects the QEMU runtime. The QEMU machine is chosen from
  `ARCH`: `armv7m` runs `lm3s6965evb`, and `armv6m` runs `microbit`.
* `PLATFORM=stm32f103rb` selects the Nucleo F103RB board. This platform is
  always `ARCH=armv7m`.

`PLATFORM` is required for build/run targets. `make help` prints the supported
commands.

## Quick Start: QEMU

Prerequisites:

* ARM GNU Toolchain (`arm-none-eabi-gcc, arm-none-eabi-gdb / gdb-multiarch (Debian)`)

* QEMU for ARM (`qemu-system-arm`)

Build and run the Cortex-M3 QEMU demo:

```shell
git clone https://github.com/antoniogiacomelli/RK0.git
cd RK0
make PLATFORM=qemu ARCH=armv7m QEMU_SYSCORECLK=50000000UL qemu
```

Build for the Cortex-M0 QEMU target:

```shell
make PLATFORM=qemu ARCH=armv6m QEMU_SYSCORECLK=50000000UL all
```

QEMU builds require a non-zero `RK_CONF_SYSCORECLK`. Pass
`QEMU_SYSCORECLK=50000000UL`, or set `RK_CONF_SYSCORECLK` to a non-zero value in
`core/inc/kconfig.h`. `RK_CONF_SYSCORECLK=0` is valid for boards that provide a
clock fallback, but not for QEMU.

## Real Hardware

Below you find building environment packages for what we consider the two well-suited CPUs for RK0 Cortex-M0 and M3. Packages are supposed to be self-contained.
    See the Wiki for more information on how to integrate on VSCode on Win/macOS/Linux.

* 🔌 **[Nucleo F103RB](dist/rk0-0.74.0-stm32f103rb-flash.zip)** **(ARM Cortex M3) Build Environment Package**

* 🔌 **[Nucleo F030R8](dist/rk0-0.74.0-stm32f030r8-flash.zip)** **(ARM Cortex M0) Build Environment Package**

The main tree also has optional Nucleo F103RB support:

```shell
make PLATFORM=stm32f103rb all
make PLATFORM=stm32f103rb flash
```

`PLATFORM=stm32f103rb` defaults to `RK_CONF_SYSCORECLK=0UL`, which falls back to
the board maximum of `72000000UL` and configures the PLL from the 8 MHz HSE
input. You may pass `F103RB_SYSCORECLK=<hz>` for another exactly derivable clock
up to 72 MHz.

Thread-Metric benchmark binaries can be built for QEMU or F103RB:

```shell
make PLATFORM=qemu ARCH=armv7m QEMU_SYSCORECLK=50000000UL thread-metric-qemu-benches
make PLATFORM=stm32f103rb thread-metric-f103rb-benches
make PLATFORM=stm32f103rb run-thread-metric-f103rb SERIAL_PORT=/dev/tty.usbmodem...
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
