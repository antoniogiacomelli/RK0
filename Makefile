# RK0  –  QEMU‑only build system (lm3s6965evb)

ARCH ?= armv7m

# stop immediately if someone asks for anything else
ifeq ($(ARCH),armv7m)
CPU := cortex-m3
FLOAT := soft
else
$(error "Only ARCH=armv7m for QEMU.")
endif

MCU_FLAGS := -mcpu=$(CPU) -mfloat-abi=$(FLOAT) -mthumb

# PROJECT LAYOUT
ARCH_DIR   := arch/$(ARCH)/kernel
CORE_DIR   := core
APP_DIR    := app
BUILD_DIR  := build/$(ARCH)
LINKER_DIR := arch/$(ARCH)

INC_DIRS := -I$(ARCH_DIR)/inc -I$(CORE_DIR)/inc -I$(APP_DIR)/inc

# FOOLCHAIN
CC       := arm-none-eabi-gcc
AS       := arm-none-eabi-gcc
LD       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size
GDB      := arm-none-eabi-gdb # or gdb-multiarch
QEMU_ARM := qemu-system-arm
SHELL	 := /bin/bash

# LINKAH SCRIPT
LINKER_SCRIPT := $(LINKER_DIR)/linker.ld

# OUTPUT
TARGET := rk0_demo
ELF    := $(BUILD_DIR)/$(TARGET).elf
BIN    := $(BUILD_DIR)/$(TARGET).bin
HEX    := $(BUILD_DIR)/$(TARGET).hex
MAP    := $(ELF:.elf=.map)

# SOURCES
C_SRCS   := $(wildcard $(CORE_DIR)/src/*.c) \
            $(wildcard $(ARCH_DIR)/src/*.c) \
            $(wildcard $(APP_DIR)/src/*.c)

ASM_SRCS := $(wildcard $(ARCH_DIR)/src/*.S)

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

# FLAGS
# Use this for optimising for size
#OPT	:= -Os
# Use this for debug
OPT     := -O0 -g
CFLAGS  := -std=gnu11 $(MCU_FLAGS) -Wall -Wextra -Wsign-compare -Wsign-conversion -pedantic -ffunction-sections -fdata-sections -g $(OPT) $(INC_DIRS)
ASFLAGS := $(MCU_FLAGS) -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections -g
LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
           -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
           -specs=nano.specs -lc  

# QEMU
QEMU_MACHINE     := lm3s6965evb
QEMU_FLAGS       := -machine $(QEMU_MACHINE) -nographic
QEMU_DEBUG_FLAGS := $(QEMU_FLAGS) -S -gdb tcp::1234

# TARGETS
all: $(BIN) $(HEX) sizes

$(ELF): $(OBJS)
	@echo "Linking $(notdir $@)"
	$(LD) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

# C objects
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ASM objects
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# Binary / Hex
$(BIN): $(ELF) ; $(OBJCOPY) -O binary -S $< $@
$(HEX): $(ELF) ; $(OBJCOPY) -O ihex   -S $< $@

# QEMU run / debug
qemu:        $(BIN) ; $(QEMU_ARM) $(QEMU_FLAGS)       -kernel $<
qemu-debug:  $(ELF) ; $(QEMU_ARM) $(QEMU_DEBUG_FLAGS) -kernel $<
clean:
	rm -rf build

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

help:
	@echo "  make              :  build (ELF / BIN / HEX)"
	@echo "  make qemu         :  run image in QEMU (lm3s6965evb)"
	@echo "  make qemu-debug   :  run QEMU & open GDB server (localhost:1234)"
	@echo "  make clean        :  remove build directory"
	@echo "  make sizes        :  report size per-object"

.PHONY: all clean sizes qemu qemu-debug help
