# RK0  –  QEMU and STM32 build system

RK_ARCH_EXPLICIT :=
ifdef ARCH
RK_ARCH_EXPLICIT := yes
endif
ifdef arch
RK_ARCH_EXPLICIT := yes
endif
ARCH ?= armv7m
ifdef arch
ARCH := $(arch)
endif
PLATFORM ?=
ifdef platform
PLATFORM := $(platform)
endif
ifneq ($(filter qemu QEMU,$(PLATFORM)),)
override PLATFORM := qemu
else ifneq ($(filter stm32f103rb STM32F103RB,$(PLATFORM)),)
override PLATFORM := stm32f103rb
else ifneq ($(filter stm32f401re STM32F401RE stm32f4014re STM32F4014RE,$(PLATFORM)),)
override PLATFORM := stm32f401re
else ifneq ($(filter stm32f030r8 STM32F030R8,$(PLATFORM)),)
override PLATFORM := stm32f030r8
endif
RK_IMAGE_ACTION_GOALS := run flash qemu qemu-debug
RK_CLI_IMAGE_GOALS :=
ifneq ($(filter $(RK_IMAGE_ACTION_GOALS),$(MAKECMDGOALS)),)
RK_CLI_IMAGE_GOALS := $(filter %.elf %.bin %.hex %/%,$(MAKECMDGOALS))
ifneq ($(words $(RK_CLI_IMAGE_GOALS)),0)
ifneq ($(words $(RK_CLI_IMAGE_GOALS)),1)
$(error Pass only one image path: $(RK_CLI_IMAGE_GOALS))
endif
ifneq ($(strip $(IMAGE)),)
$(error Use IMAGE=$(IMAGE) or positional image $(RK_CLI_IMAGE_GOALS), not both)
endif
override IMAGE := $(firstword $(RK_CLI_IMAGE_GOALS))
endif
endif
KCONFIG := core/inc/kconfig.h
RK_NO_PLATFORM_GOALS := clean help FORCE jlink-check cppcheck cppcheck-arch \
	cppcheck-report cppcheck-report-arch \
	thread-metric-f103rb-benches thread-metric-f401re-benches \
	run-thread-metric-f103rb run-thread-metric-f401re \
	flash-thread-metric-basic-processing \
	flash-thread-metric-cooperative-scheduling \
	flash-thread-metric-preemptive-scheduling \
	flash-thread-metric-interrupt-processing \
	flash-thread-metric-interrupt-preemption-processing \
	flash-thread-metric-message-processing \
	flash-thread-metric-synchronization-processing \
	flash-thread-metric-memory-allocation
ifeq ($(strip $(MAKECMDGOALS)),)
RK_PLATFORM_GOALS := __default__
else
RK_PLATFORM_GOALS := $(filter-out $(RK_NO_PLATFORM_GOALS),$(MAKECMDGOALS))
endif
ifeq ($(strip $(PLATFORM)),)
ifneq ($(strip $(RK_PLATFORM_GOALS)),)
$(error PLATFORM is required; use PLATFORM=qemu, PLATFORM=stm32f030r8, PLATFORM=stm32f103rb, or PLATFORM=stm32f401re)
endif
	PLATFORM := qemu
endif

BUILD ?= DEBUG

ifeq ($(PLATFORM),stm32f103rb)
ifneq ($(ARCH),armv7m)
ifneq ($(RK_ARCH_EXPLICIT),)
$(error PLATFORM=stm32f103rb is an armv7m board; do not pass ARCH=$(ARCH))
else
override ARCH := armv7m
endif
endif
endif
ifeq ($(PLATFORM),stm32f401re)
ifneq ($(ARCH),armv7m)
ifneq ($(RK_ARCH_EXPLICIT),)
$(error PLATFORM=stm32f401re is an armv7m board; do not pass ARCH=$(ARCH))
else
override ARCH := armv7m
endif
endif
endif
ifeq ($(PLATFORM),stm32f030r8)
ifneq ($(ARCH),armv6m)
ifneq ($(RK_ARCH_EXPLICIT),)
$(error PLATFORM=stm32f030r8 is an armv6m board; do not pass ARCH=$(ARCH))
else
override ARCH := armv6m
endif
endif
endif

RK_SYSCORECLK_OVERRIDE :=
ifdef SYSCORECLK
RK_SYSCORECLK_OVERRIDE := $(SYSCORECLK)
endif
ifdef CONF_SYSCORECLK
RK_SYSCORECLK_OVERRIDE := $(CONF_SYSCORECLK)
endif
ifdef RK_CONF_SYSCORECLK
RK_SYSCORECLK_OVERRIDE := $(RK_CONF_SYSCORECLK)
endif

F103RB_SYSCORECLK ?= $(if $(RK_SYSCORECLK_OVERRIDE),$(RK_SYSCORECLK_OVERRIDE),0UL)
F103RB_SYSTICK_DIV ?=
F103RB_HSECLK ?= 8000000UL
F103RB_HSE_BYPASS ?= ON
F103RB_HSE_DIV2 ?= OFF
F401RE_SYSCORECLK ?= $(if $(RK_SYSCORECLK_OVERRIDE),$(RK_SYSCORECLK_OVERRIDE),0UL)
F401RE_SYSTICK_DIV ?=
F030R8_SYSCORECLK ?= $(if $(RK_SYSCORECLK_OVERRIDE),$(RK_SYSCORECLK_OVERRIDE),0UL)
F030R8_SYSTICK_DIV ?=
F030R8_N_USRTASKS_MAX ?= 3U
QEMU_SYSCORECLK ?= $(RK_SYSCORECLK_OVERRIDE)
QEMU_SYSTICK_DIV ?=
F103RB_SYSTICK_DIV_DEF := $(if $(strip $(F103RB_SYSTICK_DIV)),-DRK_CONF_SYSTICK_DIV=$(F103RB_SYSTICK_DIV))
F401RE_SYSTICK_DIV_DEF := $(if $(strip $(F401RE_SYSTICK_DIV)),-DRK_CONF_SYSTICK_DIV=$(F401RE_SYSTICK_DIV))
F030R8_SYSTICK_DIV_DEF := $(if $(strip $(F030R8_SYSTICK_DIV)),-DRK_CONF_SYSTICK_DIV=$(F030R8_SYSTICK_DIV))
QEMU_SYSTICK_DIV_DEF := $(if $(strip $(QEMU_SYSTICK_DIV)),-DRK_CONF_SYSTICK_DIV=$(QEMU_SYSTICK_DIV))
RK_ZERO_SYSCORECLK_VALUES := 0 0U 0UL 0L 0u 0ul 0l (0) (0U) (0UL) (0L) (0u) (0ul) (0l)
RK_ZERO_SYSCORECLK_DEFS := $(addprefix -DRK_CONF_SYSCORECLK=,$(RK_ZERO_SYSCORECLK_VALUES))
RK_KCONFIG_SYSCORECLK := $(shell awk '/^[[:space:]]*\#[[:space:]]*define[[:space:]]+RK_CONF_SYSCORECLK[[:space:]]+/ { print $$3; exit }' $(KCONFIG) 2>/dev/null)
RK_QEMU_DEFAULT_SYSCORECLK := $(if $(filter armv6m,$(ARCH)),20000000UL,50000000UL)
RK_QEMU_REQUESTED_SYSCORECLK := $(if $(strip $(QEMU_SYSCORECLK)),$(QEMU_SYSCORECLK),$(RK_KCONFIG_SYSCORECLK))
RK_QEMU_EFFECTIVE_SYSCORECLK := $(if $(filter $(RK_ZERO_SYSCORECLK_VALUES),$(strip $(RK_QEMU_REQUESTED_SYSCORECLK))),$(RK_QEMU_DEFAULT_SYSCORECLK),$(if $(strip $(RK_QEMU_REQUESTED_SYSCORECLK)),$(RK_QEMU_REQUESTED_SYSCORECLK),$(RK_QEMU_DEFAULT_SYSCORECLK)))
QEMU_SYSCORECLK_DEF := -DRK_CONF_SYSCORECLK=$(RK_QEMU_EFFECTIVE_SYSCORECLK)

# Per-arch/platform settings: CPU, ABI, linker script and board/emulator defs.
ifeq ($(ARCH),armv7m)
FLOAT := soft
ifeq ($(PLATFORM),qemu)
CPU   := cortex-m3
QEMU_MACHINE := lm3s6965evb
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DQEMU_MACHINE_LM3S6965EVB $(QEMU_SYSCORECLK_DEF) $(QEMU_SYSTICK_DIV_DEF)
BOARD_UNDEFS := -USTM32F103xB -URK_MCU_F103RB -URK_CONF_STM32F103_HSECLK -URK_CONF_STM32F103_HSE_BYPASS -URK_CONF_STM32F103_HSE_DIV2 -USTM32F401xE -URK_MCU_F401RE
TRACE_SUPPORT_DEF :=
LINKER_SCRIPT ?= arch/armv7m/linker.ld
else ifeq ($(PLATFORM),stm32f103rb)
CPU   := cortex-m3
QEMU_MACHINE :=
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DSTM32F103xB -DRK_MCU_F103RB -D__NVIC_PRIO_BITS=4 -DRK_CONF_SYSCORECLK=$(F103RB_SYSCORECLK) $(F103RB_SYSTICK_DIV_DEF) -DRK_CONF_STM32F103_HSECLK=$(F103RB_HSECLK) -DRK_CONF_STM32F103_HSE_BYPASS=$(F103RB_HSE_BYPASS) -DRK_CONF_STM32F103_HSE_DIV2=$(F103RB_HSE_DIV2)
BOARD_UNDEFS := -USTM32F401xE -URK_MCU_F401RE
TRACE_SUPPORT_DEF :=
LINKER_SCRIPT ?= arch/armv7m/linker-stm32f103rb.ld
else ifeq ($(PLATFORM),stm32f401re)
CPU   := cortex-m4
QEMU_MACHINE :=
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DSTM32F401xE -DRK_MCU_F401RE -D__NVIC_PRIO_BITS=4 -DRK_CONF_SYSCORECLK=$(F401RE_SYSCORECLK) $(F401RE_SYSTICK_DIV_DEF)
BOARD_UNDEFS := -USTM32F103xB -URK_MCU_F103RB -URK_CONF_STM32F103_HSECLK -URK_CONF_STM32F103_HSE_BYPASS -URK_CONF_STM32F103_HSE_DIV2
TRACE_SUPPORT_DEF :=
LINKER_SCRIPT ?= arch/armv7m/linker-stm32f401re.ld
else
$(error "Unsupported PLATFORM=$(PLATFORM) for ARCH=armv7m. Use PLATFORM=qemu, PLATFORM=stm32f103rb, or PLATFORM=stm32f401re.")
endif
else ifeq ($(ARCH),armv6m)
CPU   := cortex-m0
FLOAT := soft
ifeq ($(PLATFORM),qemu)
QEMU_MACHINE := microbit
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DQEMU_MACHINE_MICROBIT $(QEMU_SYSCORECLK_DEF) $(QEMU_SYSTICK_DIV_DEF)
BOARD_UNDEFS := -USTM32F103xB -URK_MCU_F103RB -URK_CONF_STM32F103_HSECLK -URK_CONF_STM32F103_HSE_BYPASS -URK_CONF_STM32F103_HSE_DIV2 -USTM32F401xE -URK_MCU_F401RE
TRACE_SUPPORT_DEF := -URK_CONF_TRACE_SUPPORTED -DRK_CONF_TRACE_SUPPORTED=OFF
LINKER_SCRIPT ?= arch/armv6m/linker.ld
else ifeq ($(PLATFORM),stm32f030r8)
QEMU_MACHINE :=
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DSTM32F030x8 -DRK_MCU_F030R8 -D__NVIC_PRIO_BITS=2 -DRK_CONF_SYSCORECLK=$(F030R8_SYSCORECLK) -DRK_CONF_N_USRTASKS_MAX=$(F030R8_N_USRTASKS_MAX) $(F030R8_SYSTICK_DIV_DEF)
BOARD_UNDEFS := -UQEMU_MACHINE_MICROBIT -USTM32F103xB -URK_MCU_F103RB -URK_CONF_STM32F103_HSECLK -URK_CONF_STM32F103_HSE_BYPASS -URK_CONF_STM32F103_HSE_DIV2 -USTM32F401xE -URK_MCU_F401RE
TRACE_SUPPORT_DEF := -URK_CONF_TRACE_SUPPORTED -DRK_CONF_TRACE_SUPPORTED=OFF
LINKER_SCRIPT ?= arch/armv6m/linker-stm32f030r8.ld
else
$(error "Unsupported PLATFORM=$(PLATFORM) for ARCH=armv6m. Use PLATFORM=qemu or PLATFORM=stm32f030r8.")
endif
else
$(error "Only ARCH=armv7m or ARCH=armv6m.")
endif

MCU_FLAGS := -mcpu=$(CPU) -mfloat-abi=$(FLOAT) -mthumb
EXTRA_DEFS ?=
ifeq ($(PLATFORM),qemu)
override EXTRA_DEFS := $(filter-out $(RK_ZERO_SYSCORECLK_DEFS),$(EXTRA_DEFS))
endif

# PROJECT LAYOUT
ARCH_DIR   := arch/$(ARCH)/kernel
CORE_DIR   := core
APP_DIR    := app
BUILD_DIR  ?= build/$(ARCH)/$(PLATFORM)/$(BUILD)
LINKER_DIR := arch/$(ARCH)

INC_DIRS := -I$(ARCH_DIR)/inc -I$(CORE_DIR)/inc -I$(APP_DIR)/inc
APP_MAIN ?= $(APP_DIR)/src/application.c
ifneq ($(strip $(APP)),)
ifneq ($(filter /%,$(APP)),)
$(error APP must be a repository-relative path; absolute APP=$(APP) is not supported)
endif
override APP_MAIN := $(APP)
endif
ifneq ($(filter /%,$(APP_MAIN)),)
$(error APP_MAIN must be a repository-relative path; absolute APP_MAIN=$(APP_MAIN) is not supported)
endif
APP_SUPPORT_SRCS := $(filter-out $(APP_DIR)/src/application.c,$(wildcard $(APP_DIR)/src/*.c))

# FOOLCHAIN
CC       := arm-none-eabi-gcc
AS       := arm-none-eabi-gcc
LD       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size
GDB      := arm-none-eabi-gdb # or gdb-multiarch
QEMU_ARM := qemu-system-arm
SHELL	 := /bin/bash

ifneq ($(strip $(TOOL)),)
ifeq ($(PLATFORM),qemu)
ifneq ($(filter qemu QEMU,$(TOOL)),)
else
override QEMU_ARM := $(TOOL)
endif
endif
endif

# LINKER SCRIPT
LINKER_SCRIPT ?= $(LINKER_DIR)/linker.ld

# OUTPUT
TARGET ?= rk0_demo

ELF    := $(BUILD_DIR)/$(TARGET).elf
BIN    := $(BUILD_DIR)/$(TARGET).bin
HEX    := $(BUILD_DIR)/$(TARGET).hex
MAP    := $(ELF:.elf=.map)
RK_IMAGE_ARG := $(strip $(IMAGE))
RK_RUN_IMAGE := $(if $(RK_IMAGE_ARG),$(RK_IMAGE_ARG),$(ELF))
RK_FLASH_BIN_IMAGE := $(if $(RK_IMAGE_ARG),$(RK_IMAGE_ARG),$(BIN))
RK_FLASH_ELF_IMAGE := $(if $(RK_IMAGE_ARG),$(RK_IMAGE_ARG),$(ELF))
RK_OPENOCD_PROGRAM_ARGS = $(RK_FLASH_ELF_IMAGE)$(if $(filter .bin,$(suffix $(RK_FLASH_ELF_IMAGE))), $(FLASH_ADDR))
RK_RUN_PREREQS := $(if $(RK_IMAGE_ARG),,$(ELF))
RK_FLASH_PREREQS := $(if $(RK_IMAGE_ARG),,$(ELF) $(BIN))

# SOURCES
C_SRCS   := $(wildcard $(CORE_DIR)/src/*.c) \
            $(wildcard $(ARCH_DIR)/src/*.c) \
            $(APP_SUPPORT_SRCS) \
            $(APP_MAIN)

ASM_SRCS := $(wildcard $(ARCH_DIR)/src/*.S)

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

# QEMU
QEMU_FLAGS       := -machine $(QEMU_MACHINE) -nographic $(QEMU_EXTRA_FLAGS)
QEMU_DEBUG_FLAGS := $(QEMU_FLAGS) -S -gdb tcp::1234
QEMU_TIMEOUT ?= timeout
QEMU_BENCH_TIMEOUT ?= 8
QEMU_BENCH_LOG_DIR ?= build/$(ARCH)/qemu-public-benches

# STM32 flashing
FLASH_ADDR ?= 0x08000000
ifeq ($(PLATFORM),stm32f103rb)
FLASH_TOOL ?= jlink
else
FLASH_TOOL ?= st-flash
endif
ifneq ($(strip $(TOOL)),)
ifneq ($(PLATFORM),qemu)
override FLASH_TOOL := $(TOOL)
endif
endif
ST_FLASH_FLAGS ?= --connect-under-reset --reset
OPENOCD_INTERFACE ?= interface/stlink.cfg
ifeq ($(PLATFORM),stm32f401re)
OPENOCD_TARGET ?= target/stm32f4x.cfg
else ifeq ($(PLATFORM),stm32f030r8)
OPENOCD_TARGET ?= target/stm32f0x.cfg
else
OPENOCD_TARGET ?= target/stm32f1x.cfg
endif
OPENOCD_TRANSPORT ?=
OPENOCD_ADAPTER_SPEED ?=
STM32_PROGRAMMER_CLI ?= STM32_Programmer_CLI
JLINK ?= JLinkExe
ifeq ($(strip $(JLINK)),)
JLINK := JLinkExe
endif
ifeq ($(PLATFORM),stm32f401re)
JLINK_DEVICE ?= STM32F401RE
else ifeq ($(PLATFORM),stm32f030r8)
JLINK_DEVICE ?= STM32F030R8
else
JLINK_DEVICE ?= STM32F103RB
endif
JLINK_IF ?= SWD
JLINK_SPEED ?= 4000
JLINK_SCRIPT := $(BUILD_DIR)/flash.jlink

RK_IMAGE_SUFFIX := $(suffix $(RK_IMAGE_ARG))
ifneq ($(strip $(RK_IMAGE_ARG)),)
ifeq ($(PLATFORM),qemu)
ifneq ($(RK_IMAGE_SUFFIX),.elf)
$(error PLATFORM=qemu requires an .elf IMAGE; got '$(RK_IMAGE_ARG)')
endif
else ifneq ($(filter stm32f030r8 stm32f103rb stm32f401re,$(PLATFORM)),)
ifeq ($(FLASH_TOOL),openocd)
ifeq ($(filter .elf .hex .bin,$(RK_IMAGE_SUFFIX)),)
$(error FLASH_TOOL=openocd requires .elf, .hex, or .bin IMAGE; got '$(RK_IMAGE_ARG)')
endif
else ifeq ($(FLASH_TOOL),st-flash)
ifneq ($(RK_IMAGE_SUFFIX),.bin)
$(error FLASH_TOOL=st-flash requires a .bin IMAGE; got '$(RK_IMAGE_ARG)')
endif
else ifneq ($(filter jlink JLINK JLink J-Link j-link,$(FLASH_TOOL)),)
ifneq ($(RK_IMAGE_SUFFIX),.bin)
$(error FLASH_TOOL=$(FLASH_TOOL) requires a .bin IMAGE; got '$(RK_IMAGE_ARG)')
endif
else ifeq ($(FLASH_TOOL),stm32programmer)
ifneq ($(RK_IMAGE_SUFFIX),.bin)
$(error FLASH_TOOL=stm32programmer requires a .bin IMAGE; got '$(RK_IMAGE_ARG)')
endif
endif
endif
endif

CPPCHECK ?= cppcheck
CPPCHECK_ARCHES ?= armv7m armv6m
CPPCHECK_SUPPRESSIONS := cppcheck.suppressions
CPPCHECK_REPORT_DIR ?= build/cppcheck
CPPCHECK_REPORT := $(CPPCHECK_REPORT_DIR)/cppcheck-$(ARCH).txt
CPPCHECK_FLAGS := --quiet --enable=all --check-level=exhaustive \
                  --std=c99 --language=c --inline-suppr \
                  --suppressions-list=$(CPPCHECK_SUPPRESSIONS) \
                  --error-exitcode=1 --platform=unix32
CPPCHECK_DEFS := -D__GNUC__ -D'__has_builtin(x)=0' $(QEMU_MACHINE_DEF) $(TRACE_SUPPORT_DEF)

ifeq ($(ARCH),armv7m)
CPPCHECK_ARCH_DEF := -D__ARM_ARCH_7M__
else ifeq ($(ARCH),armv6m)
CPPCHECK_ARCH_DEF := -D__ARM_ARCH_6M__
endif

RK0_TELEMETRY ?= OFF
RK0_TELEMETRY_URL ?= https://antoniogiacomelli.com/


ifeq ($(BUILD),RELEASE)
	OPT     := -Os
	CFLAGS  := -std=gnu99 $(MCU_FLAGS)  -DNDEBUG  -Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic -Werror -ffunction-sections -fdata-sections $(OPT) $(INC_DIRS) $(QEMU_MACHINE_DEF) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF) $(BOARD_UNDEFS)
	ASFLAGS := $(MCU_FLAGS) -DNDEBUG -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections $(QEMU_MACHINE_DEF) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF) $(BOARD_UNDEFS)
	LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
	               -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
	           -specs=nano.specs -lc
else ifeq ($(BUILD),PROFILE)
	OPT     := -O2
	CFLAGS  := -std=gnu99 $(MCU_FLAGS)  -DNDEBUG  -Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic -Werror -ffunction-sections -fdata-sections $(OPT) $(INC_DIRS) $(QEMU_MACHINE_DEF) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF) $(BOARD_UNDEFS)
	ASFLAGS := $(MCU_FLAGS) -DNDEBUG -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections $(QEMU_MACHINE_DEF) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF) $(BOARD_UNDEFS)
	LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
	               -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
	           -specs=nano.specs -lc
else
# Use this for debug
	OPT     := -O0
	CFLAGS  := -std=gnu99 $(MCU_FLAGS) $(QEMU_MACHINE_DEF) -Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic -Werror  -ffunction-sections -fdata-sections -fstack-usage -g $(OPT) $(INC_DIRS) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF) $(BOARD_UNDEFS)
	ASFLAGS := $(MCU_FLAGS) -D__KDEF_STACKOVFLW -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections -g $(QEMU_MACHINE_DEF) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF) $(BOARD_UNDEFS)
	LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
	               -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
	           -specs=nano.specs -lc
endif

# TARGETS
all: $(BIN) $(HEX) sizes

image: all

PROFILE_PREEMPT_APP := app/examples/05_profile_preempt.c
PROFILE_PREEMPT_TARGET := rk0_profile_preempt

ifneq ($(strip $(RK_CLI_IMAGE_GOALS)),)
$(RK_CLI_IMAGE_GOALS):
	@:
endif

profile-preempt-same-space:
	$(MAKE) -B ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		BUILD=PROFILE \
		APP_MAIN=$(PROFILE_PREEMPT_APP) \
		TARGET=$(PROFILE_PREEMPT_TARGET) \
		EXTRA_DEFS='$(EXTRA_DEFS)'

-include Makefile.local

transitive-priority-inheritance-mutexes:
	$(MAKE) ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		APP_MAIN=app/examples/06_transitive_priority_inheritance.c \
		TARGET=rk0_mutex_transitive_pi

wait-queue-repriority-regression:
	$(MAKE) ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		APP_MAIN=app/examples/07_wait_queue_repriority.c \
		TARGET=rk0_wait_queue_repriority

async-ceiling-wait-regression:
	$(MAKE) -B ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		BUILD_DIR=build/$(ARCH)_async_ceiling_wait \
		APP_MAIN=app/examples/08_async_ceiling_wait.c \
		TARGET=rk0_async_ceiling_wait \
		EXTRA_DEFS='$(EXTRA_DEFS) -DRK_QEMU_UNIT_TEST -DRK_CONF_MUTEX=ON'

ready-queue-repriority-regression:
	$(MAKE) ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		APP_MAIN=app/examples/09_ready_queue_repriority.c \
		TARGET=rk0_ready_queue_repriority \
		EXTRA_DEFS='$(EXTRA_DEFS) -DRK_CONF_N_USRTASKS_MAX=4U -DRK_CONF_MUTEX=ON -DRK_CONF_SEMAPHORE=ON'

extended-rendezvous-priority-regression:
	$(MAKE) ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		APP_MAIN=app/examples/10_extended_rendezvous_priority.c \
		TARGET=rk0_extended_rendezvous_priority \
		EXTRA_DEFS='$(EXTRA_DEFS) -DRK_CONF_N_USRTASKS_MAX=3U -DRK_CONF_SYNCH_MESG=ON'

GATEKEEPER_CEILING_PI_DEFS := -DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=3U -DRK_CONF_MUTEX=ON -DRK_CONF_MESG_QUEUE=ON -DRK_CONF_ASYNCH_MESG=ON
MESG_ALLOC_RELEASE_DEFS := -DRK_QEMU_UNIT_TEST -DRK_CONF_MESG_QUEUE=ON -DRK_CONF_ASYNCH_MESG=ON

gatekeeper-ceiling-pi-regression:
	$(MAKE) -B ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		BUILD_DIR=build/$(ARCH)_gatekeeper_ceiling_pi \
		APP_MAIN=app/examples/11_gatekeeper_ceiling_pi.c \
		TARGET=rk0_gatekeeper_ceiling_pi \
		EXTRA_DEFS='$(EXTRA_DEFS) $(GATEKEEPER_CEILING_PI_DEFS)'

mesg-alloc-release-regression:
	$(MAKE) -B ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		BUILD=RELEASE BUILD_DIR=build/$(ARCH)_mesg_alloc_release \
		APP_MAIN=app/examples/12_mesg_alloc_release.c \
		TARGET=rk0_mesg_alloc_release \
		EXTRA_DEFS='$(EXTRA_DEFS) $(MESG_ALLOC_RELEASE_DEFS)'

define RUN_PUBLIC_QEMU_BENCH
	@mkdir -p "$(QEMU_BENCH_LOG_DIR)"
	@log="$(QEMU_BENCH_LOG_DIR)/$(1).log"; \
	echo "Run $(1) ($(ARCH))"; \
	set +e; \
	$(MAKE) --no-print-directory -B \
		ARCH=$(ARCH) PLATFORM=qemu QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" BUILD="$(if $(strip $(7)),$(7),DEBUG)" BUILD_DIR="$(2)" APP_MAIN="$(3)" TARGET="$(4)" \
		EXTRA_DEFS="$(5)" "$(2)/$(4).elf" > "$$log" 2>&1; \
	build_rc=$$?; \
	if [ "$$build_rc" -ne 0 ]; then \
		cat "$$log"; \
		exit "$$build_rc"; \
	fi; \
	$(QEMU_TIMEOUT) "$(QEMU_BENCH_TIMEOUT)s" $(MAKE) --no-print-directory \
		ARCH=$(ARCH) PLATFORM=qemu QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" IMAGE="$(2)/$(4).elf" \
		qemu </dev/null >> "$$log" 2>&1; \
	rc=$$?; \
	set -e; \
	if grep -Eq '(^|[[:space:]])(FAIL|ERR|FAULT|ASSERT|HardFault)' "$$log"; then \
		cat "$$log"; \
		exit 1; \
	fi; \
	if ! grep -Fq "$(6)" "$$log"; then \
		cat "$$log"; \
		echo "missing PASS marker: $(6)"; \
		exit 1; \
	fi; \
	if [ "$$rc" -ne 0 ] && [ "$$rc" -ne 124 ]; then \
		cat "$$log"; \
		exit "$$rc"; \
	fi; \
	echo "$(1): PASS ($(ARCH))"
endef

run-transitive-priority-inheritance-mutexes:
	$(call RUN_PUBLIC_QEMU_BENCH,transitive-priority-inheritance-mutexes,build/$(ARCH)_mutex_transitive_pi,app/examples/06_transitive_priority_inheritance.c,rk0_mutex_transitive_pi,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=5U -DRK_CONF_MUTEX=ON,PI PASS transitive priority inheritance)

run-wait-queue-repriority-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,wait-queue-repriority-regression,build/$(ARCH)_wait_queue_repriority,app/examples/07_wait_queue_repriority.c,rk0_wait_queue_repriority,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=3U -DRK_CONF_MUTEX=ON -DRK_CONF_CALLOUT_TIMER=ON,WQ PASS wait queue repriority)

run-async-ceiling-wait-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,async-ceiling-wait-regression,build/$(ARCH)_async_ceiling_wait,app/examples/08_async_ceiling_wait.c,rk0_async_ceiling_wait,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=4U -DRK_CONF_CALLOUT_TIMER=ON -DRK_CONF_MESG_QUEUE=ON -DRK_CONF_ASYNCH_MESG=ON -DRK_CONF_MUTEX=ON,AC PASS async ceiling waiters and send transfer)

run-ready-queue-repriority-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,ready-queue-repriority-regression,build/$(ARCH)_ready_queue_repriority,app/examples/09_ready_queue_repriority.c,rk0_ready_queue_repriority,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=4U -DRK_CONF_MUTEX=ON -DRK_CONF_SEMAPHORE=ON,RQ PASS ready queue repriority)

run-extended-rendezvous-priority-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,extended-rendezvous-priority-regression,build/$(ARCH)_extended_rendezvous_priority,app/examples/10_extended_rendezvous_priority.c,rk0_extended_rendezvous_priority,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=3U -DRK_CONF_SYNCH_MESG=ON,XR PASS extended rendezvous priority adoption)

run-gatekeeper-ceiling-pi-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,gatekeeper-ceiling-pi-regression,build/$(ARCH)_gatekeeper_ceiling_pi,app/examples/11_gatekeeper_ceiling_pi.c,rk0_gatekeeper_ceiling_pi,$(GATEKEEPER_CEILING_PI_DEFS),GC PASS gatekeeper ceiling through mutex PI)

run-mesg-alloc-release-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,mesg-alloc-release-regression,build/$(ARCH)_mesg_alloc_release,app/examples/12_mesg_alloc_release.c,rk0_mesg_alloc_release,$(MESG_ALLOC_RELEASE_DEFS),MA PASS release message allocation timeout,RELEASE)

public-qemu-benches: run-transitive-priority-inheritance-mutexes run-wait-queue-repriority-regression run-async-ceiling-wait-regression run-ready-queue-repriority-regression run-extended-rendezvous-priority-regression run-gatekeeper-ceiling-pi-regression run-mesg-alloc-release-regression

$(ELF): $(OBJS)
	@echo "Linking $(notdir $@)"
	$(LD) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

# C objects
$(BUILD_DIR)/%.o: %.c Makefile $(KCONFIG)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

# ASM objects
$(BUILD_DIR)/%.o: %.S Makefile $(KCONFIG)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# Binary / Hex
$(BIN): $(ELF) ; $(OBJCOPY) -O binary -S $< $@
$(HEX): $(ELF) ; $(OBJCOPY) -O ihex   -S $< $@

# QEMU run / debug
ifeq ($(PLATFORM),qemu)
qemu: $(RK_RUN_PREREQS)
	@if [ "$(RK0_TELEMETRY)" = "ON" ]; then \
		curl -fsS -m 1 -o /dev/null "$(RK0_TELEMETRY_URL)" || true; \
	fi
	@test -f "$(RK_RUN_IMAGE)" || { echo "error: image not found: $(RK_RUN_IMAGE)"; exit 1; }
	$(QEMU_ARM) $(QEMU_FLAGS) -kernel "$(RK_RUN_IMAGE)"

qemu-debug: $(RK_RUN_PREREQS)
	@test -f "$(RK_RUN_IMAGE)" || { echo "error: image not found: $(RK_RUN_IMAGE)"; exit 1; }
	$(QEMU_ARM) $(QEMU_DEBUG_FLAGS) -kernel "$(RK_RUN_IMAGE)"
else
qemu:
	$(error qemu requires PLATFORM=qemu)

qemu-debug:
	$(error qemu-debug requires PLATFORM=qemu)
endif

ifeq ($(PLATFORM),qemu)
run: qemu
else ifneq ($(filter stm32f030r8 stm32f103rb stm32f401re,$(PLATFORM)),)
run: flash
else
run:
	$(error run requires PLATFORM=qemu, PLATFORM=stm32f030r8, PLATFORM=stm32f103rb, or PLATFORM=stm32f401re)
endif

f030r8:
	$(MAKE) -B ARCH=armv6m PLATFORM=stm32f030r8

f103rb:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f103rb

f401re:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f401re

flash-f103rb:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f103rb flash

flash-f401re:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f401re flash

flash-f030r8:
	$(MAKE) -B ARCH=armv6m PLATFORM=stm32f030r8 flash

flash-jlink-f103rb:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f103rb FLASH_TOOL=jlink flash

flash-jlink-f401re:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f401re FLASH_TOOL=jlink flash

flash-jlink-f030r8:
	$(MAKE) -B ARCH=armv6m PLATFORM=stm32f030r8 FLASH_TOOL=jlink flash

jlink-check:
	@command -v "$(JLINK)" >/dev/null 2>&1 || { \
		echo "error: J-Link Commander not found: $(JLINK)"; \
		echo "Install SEGGER J-Link tools, add JLinkExe to PATH, or pass JLINK=/path/to/JLinkExe."; \
		exit 127; \
	}

$(JLINK_SCRIPT): FORCE $(RK_FLASH_PREREQS)
	@mkdir -p $(dir $@)
	@printf "r\nh\nloadbin %s %s\nverifybin %s %s\nr\ng\nq\n" "$(RK_FLASH_BIN_IMAGE)" "$(FLASH_ADDR)" "$(RK_FLASH_BIN_IMAGE)" "$(FLASH_ADDR)" > $@

ifneq ($(filter stm32f030r8 stm32f103rb stm32f401re,$(PLATFORM)),)
flash: $(RK_FLASH_PREREQS)
ifeq ($(FLASH_TOOL),st-flash)
	@test -f "$(RK_FLASH_BIN_IMAGE)" || { echo "error: image not found: $(RK_FLASH_BIN_IMAGE)"; exit 1; }
	st-flash $(ST_FLASH_FLAGS) write "$(RK_FLASH_BIN_IMAGE)" $(FLASH_ADDR)
else ifeq ($(FLASH_TOOL),openocd)
	@test -f "$(RK_FLASH_ELF_IMAGE)" || { echo "error: image not found: $(RK_FLASH_ELF_IMAGE)"; exit 1; }
	openocd -f $(OPENOCD_INTERFACE) $(OPENOCD_TRANSPORT) -f $(OPENOCD_TARGET) $(if $(OPENOCD_ADAPTER_SPEED),-c "adapter speed $(OPENOCD_ADAPTER_SPEED)") -c "program $(RK_OPENOCD_PROGRAM_ARGS) verify reset exit"
else ifneq ($(filter jlink JLINK JLink J-Link j-link,$(FLASH_TOOL)),)
	@test -f "$(RK_FLASH_BIN_IMAGE)" || { echo "error: image not found: $(RK_FLASH_BIN_IMAGE)"; exit 1; }
	$(MAKE) --no-print-directory jlink-check
	@mkdir -p $(dir $(JLINK_SCRIPT))
	@printf "r\nh\nloadbin %s %s\nverifybin %s %s\nr\ng\nq\n" "$(RK_FLASH_BIN_IMAGE)" "$(FLASH_ADDR)" "$(RK_FLASH_BIN_IMAGE)" "$(FLASH_ADDR)" > $(JLINK_SCRIPT)
	"$(JLINK)" -device $(JLINK_DEVICE) -if $(JLINK_IF) -speed $(JLINK_SPEED) -AutoConnect 1 -CommanderScript $(JLINK_SCRIPT)
else ifeq ($(FLASH_TOOL),stm32programmer)
	@test -f "$(RK_FLASH_BIN_IMAGE)" || { echo "error: image not found: $(RK_FLASH_BIN_IMAGE)"; exit 1; }
	$(STM32_PROGRAMMER_CLI) -c port=SWD -w "$(RK_FLASH_BIN_IMAGE)" $(FLASH_ADDR) -v -rst
else
	$(error Unsupported FLASH_TOOL '$(FLASH_TOOL)')
endif
else
flash:
	$(error flash requires PLATFORM=stm32f030r8, PLATFORM=stm32f103rb, or PLATFORM=stm32f401re)
endif

clean:
	rm -rf build

FORCE:

cppcheck:
	@for arch in $(CPPCHECK_ARCHES); do \
		echo "Cppcheck $$arch"; \
		$(MAKE) --no-print-directory ARCH=$$arch cppcheck-arch; \
	done

cppcheck-arch:
	@$(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_DEFS) $(CPPCHECK_ARCH_DEF) $(INC_DIRS) $(C_SRCS)

cppcheck-report:
	@mkdir -p $(CPPCHECK_REPORT_DIR)
	@status=0; \
	for arch in $(CPPCHECK_ARCHES); do \
		echo "Cppcheck report $$arch -> $(CPPCHECK_REPORT_DIR)/cppcheck-$$arch.txt"; \
		if ! $(MAKE) --no-print-directory ARCH=$$arch cppcheck-report-arch; then \
			status=1; \
		fi; \
	done; \
	exit $$status

cppcheck-report-arch:
	@mkdir -p $(CPPCHECK_REPORT_DIR)
	@{ \
		echo "Cppcheck report"; \
		echo "ARCH=$(ARCH)"; \
		echo "Generated: $$(date -u '+%Y-%m-%dT%H:%M:%SZ')"; \
		echo; \
	} > $(CPPCHECK_REPORT)
	@if $(CPPCHECK) $(CPPCHECK_FLAGS) $(CPPCHECK_DEFS) $(CPPCHECK_ARCH_DEF) $(INC_DIRS) $(C_SRCS) >> $(CPPCHECK_REPORT) 2>&1; then \
		echo "Result: PASS (no unsuppressed cppcheck findings)" >> $(CPPCHECK_REPORT); \
	else \
		rc=$$?; \
		echo "Result: FAIL (cppcheck exit code $$rc)" >> $(CPPCHECK_REPORT); \
		exit $$rc; \
	fi

sizes:
	@for f in $(OBJS); do \
			if [ -f $$f ]; then \
				set -- $$($(SIZE) $$f | awk 'NR==2'); \
				TEXT=$$1; DATA=$$2; BSS=$$3; TOTAL=$$4; \
				OBJNAME=$$(basename $$f); \
				echo "$$OBJNAME: TEXT=$$TEXT DATA=$$DATA BSS=$$BSS TOTAL=$$TOTAL"; \
			else \
				echo "Missing: $$f"; \
			fi; \
		done

-include $(OBJS:.o=.d)

help:
	@echo "RK0 build"
	@echo
	@echo "Usage:"
	@echo "  make PLATFORM=<platform> [ARCH=<arch>] [BUILD=<build>]"
	@echo "  make image PLATFORM=<platform> APP=<source.c> [TARGET=<name>]"
	@echo "  make run   PLATFORM=<platform> [IMAGE=<image>]"
	@echo "  make flash PLATFORM=<board>    [FLASH_TOOL=<tool>]"
	@echo
	@echo "Platforms:"
	@echo "  qemu          ARCH=armv7m (Cortex-M3) or armv6m (Cortex-M0)"
	@echo "  stm32f030r8   NUCLEO-F030R8, Cortex-M0"
	@echo "  stm32f103rb   NUCLEO-F103RB, Cortex-M3"
	@echo "  stm32f401re   NUCLEO-F401RE, Cortex-M4"
	@echo
	@echo "Options:"
	@echo "  BUILD         DEBUG (default), PROFILE, or RELEASE"
	@echo "  FLASH_TOOL    st-flash (default), openocd, jlink, or stm32programmer"
	@echo "  IMAGE         Existing .elf for QEMU; .elf/.hex/.bin as tool permits"
	@echo
	@echo "Other targets:"
	@echo "  qemu-debug    Start QEMU with GDB server on port 1234"
	@echo "  sizes         Show per-object memory usage"
	@echo "  cppcheck      Run static analysis"
	@echo "  clean         Remove build output"

.PHONY: all image clean sizes qemu qemu-debug run f030r8 f103rb f401re flash-f030r8 flash-f103rb flash-f401re flash-jlink-f030r8 flash-jlink-f103rb flash-jlink-f401re flash jlink-check FORCE profile-preempt-same-space transitive-priority-inheritance-mutexes wait-queue-repriority-regression async-ceiling-wait-regression ready-queue-repriority-regression extended-rendezvous-priority-regression gatekeeper-ceiling-pi-regression mesg-alloc-release-regression run-transitive-priority-inheritance-mutexes run-wait-queue-repriority-regression run-async-ceiling-wait-regression run-ready-queue-repriority-regression run-extended-rendezvous-priority-regression run-gatekeeper-ceiling-pi-regression run-mesg-alloc-release-regression public-qemu-benches cppcheck cppcheck-arch cppcheck-report cppcheck-report-arch help
