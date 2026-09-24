[![CI](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/antoniogiacomelli/RK0/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.83.0-blue)](https://github.com/antoniogiacomelli/RK0/blob/main/CHANGELOG.md)
[![Docs](https://img.shields.io/badge/docs-HTML-orange)](https://antoniogiacomelli.github.io/RK0/)

<h1 align="left">RK<em>0</em> - The Embedded Real-Time Kernel '0'<img src="https://github.com/user-attachments/assets/b8b5693b-197e-4fd4-b51e-5865bb568447" width="7%" align="left" alt="image"></h1>

***

Zero surprises: Not a minimal RTOS...
------------------------------------
<img src="https://github.com/user-attachments/assets/5d5a15bf-9a3b-4abb-94f7-6449243e8948" width="12%" align="right" alt="image">

*Interaction-oriented: an RK0ish application code describes how tasks interact rather than delegating to application code to compose generic services. Recurring coordination patterns for real-time applications are totally defined by the relationship between concurrency entities (Tasks) and optimised to handle worst-case scenarios.*

* [RK0 Docbook](https://antoniogiacomelli.github.io/RK0/): compreehensive document with design internals, architecture, caveats and usage examples

* [Service Map](https://github.com/antoniogiacomelli/RK0/wiki/Service-Semantics): a must-read for developing

* [RK0 Wiki](https://github.com/antoniogiacomelli/RK0/wiki): misc of information: requirement matrix, design patterns, setting up VSCode/QEMU/GDB on Linux/Win/MacOS, profiling metrics.

* [RK0 Blog](https://rkernel0.org/blog/): blogs about RK0 and systems programming in general

***

# Running

## Build Model

RK0 separates the CPU architecture from the board/runtime:

* `ARCH=armv7m` or `ARCH=armv6m` selects the Cortex-M architecture port.

* `PLATFORM=qemu` selects the QEMU runtime. The QEMU machine is chosen from
  `ARCH`: `armv7m` runs `lm3s6965evb`, and `armv6m` runs `microbit`.

`PLATFORM` is required for build/run targets. `make help` prints supported
commands.

## Quick Start: QEMU

Prerequisites:

* ARM GNU Toolchain (`arm-none-eabi-gcc, arm-none-eabi-gdb / gdb-multiarch (Debian)`)

* QEMU for ARM (`qemu-system-arm`)

Build and run the Cortex-M3 QEMU demo:

```shell
git clone https://github.com/antoniogiacomelli/RK0.git
cd RK0
make PLATFORM=qemu ARCH=armv7m qemu
```

Build for the Cortex-M0 QEMU target:

```shell
make PLATFORM=qemu ARCH=armv6m all
```

For QEMU, `RK_CONF_SYSCORECLK=0` selects the emulated target's default clock:
50 MHz for `armv7m` and 20 MHz for `armv6m`. Set `QEMU_SYSCORECLK` or configure
a non-zero `RK_CONF_SYSCORECLK` to override that default.

## Image Builds

Build an image from an app source with:

```shell
make image PLATFORM=<platform> BUILD=<DEBUG|PROFILE|RELEASE> APP=<path/to/app.c> TARGET=<image-name>
```

That writes `build/<arch>/<platform>/<build>/<image-name>.{elf,bin,hex}`.
`BUILD` defaults to `DEBUG` when omitted.

The shipped `app/src/application.c` is a minimal two-task example using
`kSleep()` and `kPuts()`. The former all-in-one demonstrations are preserved in
[`app/application_examples.md`](app/application_examples.md).

Three additional timing APIs are optional and disabled by default:

* `RK_CONF_BUSY_DELAY=ON` enables `kDelay()` and `kBusyDelay()`.

* `RK_CONF_SLEEP_RELEASE=ON` enables `kSleepRelease()` and
  `kSleepPeriodic()`.

* `RK_CONF_SLEEP_UNTIL=ON` enables `kSleepUntil()`.

`kSleep()` and `kSleepDelay()` are always available. QEMU unit-test builds
enable all optional timing APIs automatically.

Run an existing QEMU image by path:

```shell
make run PLATFORM=qemu TOOL=qemu IMAGE=build/armv7m/qemu/DEBUG/rk0_demo.elf
```

`IMAGE=<path>` may also be written as the final make goal when the path has a
normal image suffix:

```shell
make run PLATFORM=qemu TOOL=qemu build/armv7m/qemu/DEBUG/rk0_demo.elf
```

## Real Hardware

This building environment also supports real STM32 Nucleo boards:

* `PLATFORM=stm32f030r8` selects the Nucleo-F030R8 Cortex-M0 target.

  > This MCU is very tiny. Recommend using only a couple of optional services, if any. (Reasonable choice: Mutex and Sleep Queues)

* `PLATFORM=stm32f103rb` selects the Nucleo-F103RB Cortex-M3 target.

* `PLATFORM=stm32f401re` selects the Nucleo-F401RE Cortex-M4F target.

The HAL provided is not from any vendor. We made it just
enough for supporting the CPU itself and USART2. Also, the debugging/run
environment is not locked to any IDE. The real dependencies are ARM-GCC and the
GNU Debugger. The wiki has pages explaining environment setup on Win/Linux/macOS.

Build the default app image for a board with:

```shell
make PLATFORM=stm32f401re
```

That uses `APP_MAIN=app/src/application.c`, `TARGET=rk0_demo`, and writes
`build/armv7m/stm32f401re/DEBUG/rk0_demo.{elf,bin,hex}`. Use
`PLATFORM=stm32f103rb` for the Nucleo-F103RB board, or build the Cortex-M0
board with:

```shell
make PLATFORM=stm32f030r8
```

The F030R8 target uses its 8 MHz internal oscillator and configures a 48 MHz
system clock by default. USART2 on PA2/PA3 is connected to the ST-LINK virtual
COM port at 115200 baud. The minimal default application fits this board
without enabling optional services. F030R8 builds set
`RK_CONF_N_USRTASKS_MAX=3U` to fit the available 8 KB SRAM.

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
