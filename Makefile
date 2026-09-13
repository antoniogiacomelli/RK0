# RK0  –  QEMU and STM32F103RB build system

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
endif
KCONFIG := core/inc/kconfig.h
RK_NO_PLATFORM_GOALS := clean help FORCE jlink-check cppcheck cppcheck-arch \
	cppcheck-report cppcheck-report-arch
ifeq ($(strip $(MAKECMDGOALS)),)
RK_PLATFORM_GOALS := __default__
else
RK_PLATFORM_GOALS := $(filter-out $(RK_NO_PLATFORM_GOALS),$(MAKECMDGOALS))
endif
ifeq ($(strip $(PLATFORM)),)
ifneq ($(strip $(RK_PLATFORM_GOALS)),)
$(error PLATFORM is required; use PLATFORM=qemu or PLATFORM=stm32f103rb)
endif
	PLATFORM := qemu
endif
ifeq ($(PLATFORM),stm32f103rb)
ifneq ($(ARCH),armv7m)
ifneq ($(RK_ARCH_EXPLICIT),)
$(error PLATFORM=stm32f103rb is an armv7m board; do not pass ARCH=$(ARCH))
else
override ARCH := armv7m
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
QEMU_SYSCORECLK ?= $(RK_SYSCORECLK_OVERRIDE)
QEMU_SYSTICK_DIV ?=
F103RB_SYSTICK_DIV_DEF := $(if $(strip $(F103RB_SYSTICK_DIV)),-DRK_CONF_SYSTICK_DIV=$(F103RB_SYSTICK_DIV))
QEMU_SYSTICK_DIV_DEF := $(if $(strip $(QEMU_SYSTICK_DIV)),-DRK_CONF_SYSTICK_DIV=$(QEMU_SYSTICK_DIV))
RK_ZERO_SYSCORECLK_VALUES := 0 0U 0UL 0L 0u 0ul 0l (0) (0U) (0UL) (0L) (0u) (0ul) (0l)
RK_ZERO_SYSCORECLK_DEFS := $(addprefix -DRK_CONF_SYSCORECLK=,$(RK_ZERO_SYSCORECLK_VALUES))
RK_KCONFIG_SYSCORECLK := $(shell awk '/^[[:space:]]*\#[[:space:]]*define[[:space:]]+RK_CONF_SYSCORECLK[[:space:]]+/ { print $$3; exit }' $(KCONFIG) 2>/dev/null)
RK_QEMU_EFFECTIVE_SYSCORECLK := $(if $(strip $(QEMU_SYSCORECLK)),$(QEMU_SYSCORECLK),$(RK_KCONFIG_SYSCORECLK))
QEMU_SYSCORECLK_DEF := $(if $(strip $(QEMU_SYSCORECLK)),-DRK_CONF_SYSCORECLK=$(QEMU_SYSCORECLK))
RK_NO_QEMU_CLOCK_GUARD_GOALS := clean help sizes cppcheck cppcheck-arch \
	cppcheck-report cppcheck-report-arch jlink-check FORCE
ifeq ($(strip $(MAKECMDGOALS)),)
RK_QEMU_CLOCK_GUARD_GOALS := __default__
else
RK_QEMU_CLOCK_GUARD_GOALS := $(filter-out $(RK_NO_QEMU_CLOCK_GUARD_GOALS),$(MAKECMDGOALS))
endif
ifeq ($(PLATFORM),qemu)
ifneq ($(strip $(RK_QEMU_CLOCK_GUARD_GOALS)),)
ifneq ($(filter $(RK_ZERO_SYSCORECLK_VALUES),$(strip $(RK_QEMU_EFFECTIVE_SYSCORECLK))),)
$(error QEMU builds require RK_CONF_SYSCORECLK > 0; set it in kconfig.h or pass QEMU_SYSCORECLK=50000000UL)
endif
ifneq ($(filter $(RK_ZERO_SYSCORECLK_DEFS),$(EXTRA_DEFS)),)
$(error QEMU builds require RK_CONF_SYSCORECLK > 0; remove zero RK_CONF_SYSCORECLK from EXTRA_DEFS)
endif
endif
endif

# Per-arch/platform settings: CPU, ABI, linker script and board/emulator defs.
ifeq ($(ARCH),armv7m)
FLOAT := soft
ifeq ($(PLATFORM),qemu)
CPU   := cortex-m3
QEMU_MACHINE := lm3s6965evb
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DQEMU_MACHINE_LM3S6965EVB $(QEMU_SYSCORECLK_DEF) $(QEMU_SYSTICK_DIV_DEF)
BOARD_UNDEFS := -USTM32F103xB -URK_MCU_F103RB -URK_CONF_STM32F103_HSECLK -URK_CONF_STM32F103_HSE_BYPASS -URK_CONF_STM32F103_HSE_DIV2
TRACE_SUPPORT_DEF :=
LINKER_SCRIPT ?= arch/armv7m/linker.ld
else ifeq ($(PLATFORM),stm32f103rb)
CPU   := cortex-m3
QEMU_MACHINE :=
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DSTM32F103xB -DRK_MCU_F103RB -D__NVIC_PRIO_BITS=4 -DRK_CONF_SYSCORECLK=$(F103RB_SYSCORECLK) $(F103RB_SYSTICK_DIV_DEF) -DRK_CONF_STM32F103_HSECLK=$(F103RB_HSECLK) -DRK_CONF_STM32F103_HSE_BYPASS=$(F103RB_HSE_BYPASS) -DRK_CONF_STM32F103_HSE_DIV2=$(F103RB_HSE_DIV2)
BOARD_UNDEFS :=
TRACE_SUPPORT_DEF :=
LINKER_SCRIPT ?= arch/armv7m/linker-stm32f103rb.ld
else
$(error "Unsupported PLATFORM=$(PLATFORM) for ARCH=armv7m. Use PLATFORM=qemu or PLATFORM=stm32f103rb.")
endif
else ifeq ($(ARCH),armv6m)
ifneq ($(PLATFORM),qemu)
$(error "ARCH=armv6m currently supports PLATFORM=qemu only.")
endif
CPU   := cortex-m0
FLOAT := soft
QEMU_MACHINE := microbit
QEMU_EXTRA_FLAGS :=
QEMU_MACHINE_DEF := -DQEMU_MACHINE_MICROBIT $(QEMU_SYSCORECLK_DEF) $(QEMU_SYSTICK_DIV_DEF)
BOARD_UNDEFS := -USTM32F103xB -URK_MCU_F103RB -URK_CONF_STM32F103_HSECLK -URK_CONF_STM32F103_HSE_BYPASS -URK_CONF_STM32F103_HSE_DIV2
TRACE_SUPPORT_DEF := -URK_CONF_TRACE_SUPPORTED -DRK_CONF_TRACE_SUPPORTED=OFF
LINKER_SCRIPT ?= arch/armv6m/linker.ld
else
$(error "Only ARCH=armv7m or ARCH=armv6m.")
endif

MCU_FLAGS := -mcpu=$(CPU) -mfloat-abi=$(FLOAT) -mthumb
EXTRA_DEFS ?=

# PROJECT LAYOUT
ARCH_DIR   := arch/$(ARCH)/kernel
CORE_DIR   := core
APP_DIR    := app
BUILD_DIR  ?= build/$(ARCH)/$(PLATFORM)
LINKER_DIR := arch/$(ARCH)

INC_DIRS := -I$(ARCH_DIR)/inc -I$(CORE_DIR)/inc -I$(APP_DIR)/inc
APP_MAIN ?= $(APP_DIR)/src/application.c
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

# LINKER SCRIPT
LINKER_SCRIPT ?= $(LINKER_DIR)/linker.ld

# OUTPUT
TARGET ?= rk0_demo
ELF    := $(BUILD_DIR)/$(TARGET).elf
BIN    := $(BUILD_DIR)/$(TARGET).bin
HEX    := $(BUILD_DIR)/$(TARGET).hex
MAP    := $(ELF:.elf=.map)

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
ST_FLASH_FLAGS ?= --connect-under-reset --reset
OPENOCD_INTERFACE ?= interface/stlink.cfg
OPENOCD_TARGET ?= target/stm32f1x.cfg
OPENOCD_TRANSPORT ?=
OPENOCD_ADAPTER_SPEED ?=
STM32_PROGRAMMER_CLI ?= STM32_Programmer_CLI
JLINK ?= JLinkExe
ifeq ($(strip $(JLINK)),)
JLINK := JLinkExe
endif
JLINK_DEVICE ?= STM32F103RB
JLINK_IF ?= SWD
JLINK_SPEED ?= 4000
JLINK_SCRIPT := $(BUILD_DIR)/flash.jlink

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


BUILD ?= DEBUG

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

profile-preempt-same-space:
	$(MAKE) ARCH=$(ARCH) PLATFORM=$(PLATFORM) QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" \
		BUILD=PROFILE \
		APP_MAIN=app/examples/05_profile_preempt.c \
		TARGET=rk0_profile_preempt \
		EXTRA_DEFS='-DNDEBUG'

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
		EXTRA_DEFS='$(EXTRA_DEFS) -DRK_QEMU_UNIT_TEST'

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

define RUN_PUBLIC_QEMU_BENCH
	@mkdir -p "$(QEMU_BENCH_LOG_DIR)"
	@log="$(QEMU_BENCH_LOG_DIR)/$(1).log"; \
	echo "Run $(1) ($(ARCH))"; \
	set +e; \
	$(QEMU_TIMEOUT) "$(QEMU_BENCH_TIMEOUT)s" $(MAKE) --no-print-directory -B \
		ARCH=$(ARCH) PLATFORM=qemu QEMU_SYSCORECLK="$(QEMU_SYSCORECLK)" BUILD_DIR="$(2)" APP_MAIN="$(3)" TARGET="$(4)" \
		EXTRA_DEFS="$(5)" qemu > "$$log" 2>&1; \
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
	$(call RUN_PUBLIC_QEMU_BENCH,async-ceiling-wait-regression,build/$(ARCH)_async_ceiling_wait,app/examples/08_async_ceiling_wait.c,rk0_async_ceiling_wait,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=4U -DRK_CONF_CALLOUT_TIMER=ON -DRK_CONF_MESG_QUEUE=ON -DRK_CONF_ASYNCH_MESG=ON,AC PASS async ceiling waiters and send transfer)

run-ready-queue-repriority-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,ready-queue-repriority-regression,build/$(ARCH)_ready_queue_repriority,app/examples/09_ready_queue_repriority.c,rk0_ready_queue_repriority,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=4U -DRK_CONF_MUTEX=ON -DRK_CONF_SEMAPHORE=ON,RQ PASS ready queue repriority)

run-extended-rendezvous-priority-regression:
	$(call RUN_PUBLIC_QEMU_BENCH,extended-rendezvous-priority-regression,build/$(ARCH)_extended_rendezvous_priority,app/examples/10_extended_rendezvous_priority.c,rk0_extended_rendezvous_priority,-DRK_QEMU_UNIT_TEST -DRK_CONF_N_USRTASKS_MAX=3U -DRK_CONF_SYNCH_MESG=ON,XR PASS extended rendezvous priority adoption)

public-qemu-benches: run-transitive-priority-inheritance-mutexes run-wait-queue-repriority-regression run-async-ceiling-wait-regression run-ready-queue-repriority-regression run-extended-rendezvous-priority-regression

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
qemu: $(ELF)
	@if [ "$(RK0_TELEMETRY)" = "ON" ]; then \
		curl -fsS -m 1 -o /dev/null "$(RK0_TELEMETRY_URL)" || true; \
	fi
	$(QEMU_ARM) $(QEMU_FLAGS) -kernel $<

qemu-debug: $(ELF)
	$(QEMU_ARM) $(QEMU_DEBUG_FLAGS) -kernel $<
else
qemu:
	$(error qemu requires PLATFORM=qemu)

qemu-debug:
	$(error qemu-debug requires PLATFORM=qemu)
endif

f103rb:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f103rb

flash-f103rb:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f103rb flash

flash-jlink-f103rb:
	$(MAKE) -B ARCH=armv7m PLATFORM=stm32f103rb FLASH_TOOL=jlink flash

jlink-check:
	@command -v "$(JLINK)" >/dev/null 2>&1 || { \
		echo "error: J-Link Commander not found: $(JLINK)"; \
		echo "Install SEGGER J-Link tools, add JLinkExe to PATH, or pass JLINK=/path/to/JLinkExe."; \
		exit 127; \
	}

$(JLINK_SCRIPT): FORCE $(BIN)
	@mkdir -p $(dir $@)
	@printf "r\nh\nloadbin %s %s\nverifybin %s %s\nr\ng\nq\n" "$(BIN)" "$(FLASH_ADDR)" "$(BIN)" "$(FLASH_ADDR)" > $@

ifeq ($(PLATFORM),stm32f103rb)
flash: $(ELF) $(BIN)
ifeq ($(FLASH_TOOL),st-flash)
	st-flash $(ST_FLASH_FLAGS) write $(BIN) $(FLASH_ADDR)
else ifeq ($(FLASH_TOOL),openocd)
	openocd -f $(OPENOCD_INTERFACE) $(OPENOCD_TRANSPORT) -f $(OPENOCD_TARGET) $(if $(OPENOCD_ADAPTER_SPEED),-c "adapter speed $(OPENOCD_ADAPTER_SPEED)") -c "program $(ELF) verify reset exit"
else ifneq ($(filter jlink JLINK JLink J-Link j-link,$(FLASH_TOOL)),)
	$(MAKE) --no-print-directory jlink-check
	$(MAKE) --no-print-directory $(JLINK_SCRIPT)
	"$(JLINK)" -device $(JLINK_DEVICE) -if $(JLINK_IF) -speed $(JLINK_SPEED) -AutoConnect 1 -CommanderScript $(JLINK_SCRIPT)
else ifeq ($(FLASH_TOOL),stm32programmer)
	$(STM32_PROGRAMMER_CLI) -c port=SWD -w $(BIN) $(FLASH_ADDR) -v -rst
else
	$(error Unsupported FLASH_TOOL '$(FLASH_TOOL)')
endif
else
flash:
	$(error flash requires PLATFORM=stm32f103rb)
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
	@echo "  make PLATFORM=qemu ARCH=armv7m QEMU_SYSCORECLK=50000000UL : build lm3s6965evb QEMU image"
	@echo "  make PLATFORM=qemu ARCH=armv6m QEMU_SYSCORECLK=50000000UL : build microbit QEMU image"
	@echo "  make PLATFORM=qemu ARCH=armv7m QEMU_SYSCORECLK=50000000UL qemu : run QEMU image"
	@echo "  make PLATFORM=stm32f103rb : build STM32F103RB image"
	@echo "  make PLATFORM=stm32f103rb flash : build and flash STM32F103RB with SEGGER J-Link"
	@echo "  make flash PLATFORM=stm32f103rb : flash with SEGGER J-Link"
	@echo "  make flash PLATFORM=stm32f103rb FLASH_TOOL=openocd : flash with OpenOCD"
	@echo "  make flash PLATFORM=stm32f103rb FLASH_TOOL=st-flash : flash with st-flash"
	@echo "  make PLATFORM=qemu QEMU_SYSCORECLK=50000000UL qemu-debug : run QEMU & open GDB server (localhost:1234)"
	@echo "  make PLATFORM=qemu profile-preempt-same-space : build app/examples/05_profile_preempt.c"

	@echo "  make clean        :  remove build directory"

.PHONY: all clean sizes qemu qemu-debug f103rb flash-f103rb flash-jlink-f103rb flash jlink-check FORCE profile-preempt-same-space transitive-priority-inheritance-mutexes wait-queue-repriority-regression async-ceiling-wait-regression ready-queue-repriority-regression extended-rendezvous-priority-regression run-transitive-priority-inheritance-mutexes run-wait-queue-repriority-regression run-async-ceiling-wait-regression run-ready-queue-repriority-regression run-extended-rendezvous-priority-regression public-qemu-benches cppcheck cppcheck-arch cppcheck-report cppcheck-report-arch help
