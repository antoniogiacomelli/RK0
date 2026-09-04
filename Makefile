# RK0 - ARMv6-M / STM32F030R8 build system

ARCH ?= armv6m
ifdef arch
ARCH := $(arch)
endif
PLATFORM ?= stm32f030r8
ifdef platform
PLATFORM := $(platform)
endif

ifneq ($(ARCH),armv6m)
$(error "Only ARCH=armv6m is supported on this branch.")
endif

# Per-platform settings: CPU, ABI, linker script and board/emulator defs.
CPU   := cortex-m0
FLOAT := soft
ifeq ($(PLATFORM),qemu)
QEMU_MACHINE := microbit
QEMU_EXTRA_FLAGS :=
PLATFORM_DEFS := -DQEMU_MACHINE_MICROBIT
TRACE_SUPPORT_DEF := -URK_CONF_TRACE_SUPPORTED -DRK_CONF_TRACE_SUPPORTED=OFF
LINKER_SCRIPT ?= arch/armv6m/linker.ld
else ifneq ($(filter stm32f030r8 stm32f030r8b f030r8 f030r8b,$(PLATFORM)),)
override PLATFORM := stm32f030r8
QEMU_MACHINE :=
QEMU_EXTRA_FLAGS :=
PLATFORM_DEFS := -DSTM32F030x8 -DRK_MCU_F030R8 -D__NVIC_PRIO_BITS=2
TRACE_SUPPORT_DEF :=
LINKER_SCRIPT ?= arch/armv6m/linker-stm32f030r8.ld
else
$(error "Unsupported PLATFORM=$(PLATFORM). Use PLATFORM=stm32f030r8 or PLATFORM=qemu.")
endif

MCU_FLAGS := -mcpu=$(CPU) -mfloat-abi=$(FLOAT) -mthumb
EXTRA_DEFS ?=

# PROJECT LAYOUT
ARCH_DIR   := arch/$(ARCH)/kernel
CORE_DIR   := core
APP_DIR    := app
BUILD_DIR  := build/$(ARCH)/$(PLATFORM)
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

# STM32 flashing
FLASH_ADDR ?= 0x08000000
FLASH_TOOL ?= st-flash
ST_FLASH_FLAGS ?= --connect-under-reset --reset
OPENOCD_INTERFACE ?= interface/stlink.cfg
OPENOCD_TARGET ?= target/stm32f0x.cfg
OPENOCD_TRANSPORT ?=
OPENOCD_ADAPTER_SPEED ?=
STM32_PROGRAMMER_CLI ?= STM32_Programmer_CLI
JLINK ?= JLinkExe
ifeq ($(strip $(JLINK)),)
JLINK := JLinkExe
endif
JLINK_DEVICE ?= STM32F030R8
JLINK_IF ?= SWD
JLINK_SPEED ?= 4000
JLINK_SCRIPT := $(BUILD_DIR)/flash.jlink

CPPCHECK ?= cppcheck
CPPCHECK_ARCHES ?= armv6m
CPPCHECK_SUPPRESSIONS := cppcheck.suppressions
CPPCHECK_REPORT_DIR ?= build/cppcheck
CPPCHECK_REPORT := $(CPPCHECK_REPORT_DIR)/cppcheck-$(ARCH).txt
CPPCHECK_FLAGS := --quiet --enable=all --check-level=exhaustive \
                  --std=c99 --language=c --inline-suppr \
                  --suppressions-list=$(CPPCHECK_SUPPRESSIONS) \
                  --error-exitcode=1 --platform=unix32
CPPCHECK_DEFS := -D__GNUC__ -D'__has_builtin(x)=0' $(PLATFORM_DEFS) $(TRACE_SUPPORT_DEF)
CPPCHECK_ARCH_DEF := -D__ARM_ARCH_6M__

RK0_TELEMETRY ?= OFF
RK0_TELEMETRY_URL ?= https://antoniogiacomelli.com/


BUILD ?= DEBUG

ifeq ($(BUILD),RELEASE)
	OPT     := -Os
	CFLAGS  := -std=gnu99 $(MCU_FLAGS)  -DNDEBUG  -Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic -Werror -ffunction-sections -fdata-sections $(OPT) $(INC_DIRS) $(PLATFORM_DEFS) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF)
	ASFLAGS := $(MCU_FLAGS) -DNDEBUG -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections $(PLATFORM_DEFS) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF)
	LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
    	       -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
        	   -specs=nano.specs -lc
else
# Use this for debug
	OPT     := -O0
	CFLAGS  := -std=gnu99 $(MCU_FLAGS) $(PLATFORM_DEFS) -Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic -Werror  -ffunction-sections -fdata-sections -fstack-usage -g $(OPT) $(INC_DIRS) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF)
	ASFLAGS := $(MCU_FLAGS) -D__KDEF_STACKOVFLW -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections -g $(PLATFORM_DEFS) $(EXTRA_DEFS) $(TRACE_SUPPORT_DEF)
	LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
    	       -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
        	   -specs=nano.specs -lc
endif

# TARGETS
all: $(BIN) $(HEX) sizes

$(ELF): $(OBJS)
	@echo "Linking $(notdir $@)"
	$(LD) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

# C objects
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

# ASM objects
$(BUILD_DIR)/%.o: %.S
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

f030r8:
	$(MAKE) ARCH=armv6m PLATFORM=stm32f030r8

flash-f030r8:
	$(MAKE) ARCH=armv6m PLATFORM=stm32f030r8 flash

flash-jlink-f030r8:
	$(MAKE) ARCH=armv6m PLATFORM=stm32f030r8 FLASH_TOOL=jlink flash

jlink-check:
	@command -v "$(JLINK)" >/dev/null 2>&1 || { \
		echo "error: J-Link Commander not found: $(JLINK)"; \
		echo "Install SEGGER J-Link tools, add JLinkExe to PATH, or pass JLINK=/path/to/JLinkExe."; \
		exit 127; \
	}

$(JLINK_SCRIPT): FORCE $(BIN)
	@mkdir -p $(dir $@)
	@printf "r\nh\nloadbin %s %s\nverifybin %s %s\nr\ng\nq\n" "$(BIN)" "$(FLASH_ADDR)" "$(BIN)" "$(FLASH_ADDR)" > $@

ifeq ($(PLATFORM),stm32f030r8)
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
	$(error flash requires PLATFORM=stm32f030r8)
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
	@echo "  make              :  build STM32F030R8 image (ELF / BIN / HEX)"
	@echo "  make f030r8       :  build for STM32F030R8"
	@echo "  make flash-f030r8 :  build and flash STM32F030R8"
	@echo "  make flash PLATFORM=stm32f030r8 FLASH_TOOL=openocd : flash with OpenOCD"
	@echo "  make flash PLATFORM=stm32f030r8 FLASH_TOOL=jlink  : flash with SEGGER J-Link"
	@echo "  make flash PLATFORM=stm32f030r8 FLASH_TOOL=jlink JLINK=/path/to/JLinkExe : use a custom J-Link path"
	@echo "  make flash-jlink-f030r8 : build and flash STM32F030R8 with SEGGER J-Link"
	@echo "  make PLATFORM=qemu qemu : run ARMv6-M microbit image in QEMU"
	@echo "  make qemu-debug   :  run QEMU & open GDB server (localhost:1234)"
	@echo "  make cppcheck     :  run cppcheck static analysis for armv6m"
	@echo "  make cppcheck-report : write per-arch cppcheck reports under build/cppcheck"
	@echo "  make clean        :  remove build directory"

.PHONY: all clean sizes qemu qemu-debug f030r8 flash-f030r8 flash-jlink-f030r8 flash jlink-check FORCE cppcheck cppcheck-arch cppcheck-report cppcheck-report-arch help
