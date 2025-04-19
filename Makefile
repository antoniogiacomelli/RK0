# RK0  –  QEMU‑only build system (ARMv7‑M / Cortex‑M3)

ARCH ?= armv7m

# stop immediately if someone asks for anything else
ifeq ($(ARCH),armv7m)
CPU := cortex-m3          
else
$(error “Only ARCH=armv7m for QEMU.”)
endif

MCU_FLAGS := -mcpu=$(CPU) -mthumb

# PROJECT LAYOUT
ARCH_DIR   := arch/$(ARCH)/kernel
CORE_DIR   := core
APP_DIR    := app/$(ARCH)
BUILD_DIR  := build/$(ARCH)
LINKER_DIR := arch/$(ARCH)

INC_DIRS := -I$(CORE_DIR)/inc -I$(ARCH_DIR)/inc -I$(APP_DIR)/inc

# FOOLCHAIN
CC       := arm-none-eabi-gcc
AS       := arm-none-eabi-gcc
LD       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size
GDB      := arm-none-eabi-gdb # or gdb-multiarch
QEMU_ARM := qemu-system-arm

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
OPT     := -Os
CFLAGS  := -std=gnu11 $(MCU_FLAGS) -Wall -Wextra -pedantic -ffunction-sections -fdata-sections -g $(OPT) $(INC_DIRS)
ASFLAGS := $(MCU_FLAGS) -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections -g
LDFLAGS := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) \
           -Wl,-Map=$(MAP),--cref -Wl,--gc-sections \
           -specs=nano.specs -lc -lnosys

# QEMU
QEMU_MACHINE     := lm3s6965evb
QEMU_FLAGS       := -machine $(QEMU_MACHINE) -nographic
QEMU_DEBUG_FLAGS := $(QEMU_FLAGS) -S -gdb tcp::1234

# TARGETS
all: $(BIN) $(HEX)

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
qemu-debug:  $(BIN) ; $(QEMU_ARM) $(QEMU_DEBUG_FLAGS) -kernel $<
gdb-help:
	@echo "Start GDB with: $(GDB) $(ELF) -ex 'target remote localhost:1234'"


clean:
	rm -rf build

help:
	@echo "  make              :  build (ELF / BIN / HEX)"
	@echo "  make qemu         :  run image in QEMU (lm3s6965evb)"
	@echo "  make qemu-debug   :  run QEMU & open GDB server (localhost:1234)"
	@echo "  make clean        :  remove build directory"

.PHONY: all clean qemu qemu-debug gdb-help help
