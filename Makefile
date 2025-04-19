# RK0 Makefile with QEMU support only

# Target configuration
ARCH        ?= armv6m
CPU         := $(if $(filter $(ARCH),armv7m),cortex-m3,cortex-m0)
MCU_FLAGS   := -mcpu=$(CPU) -mthumb

# Directories
ARCH_DIR    := arch/$(ARCH)/kernel
CORE_DIR    := core
APP_DIR     := app/$(ARCH)
BUILD_DIR   := build/$(ARCH)
LINKER_DIR  := arch/$(ARCH)

# Include directories
INC_DIRS    := -I$(CORE_DIR)/inc -I$(ARCH_DIR)/inc -I$(APP_DIR)/inc

# Tools
CC          := arm-none-eabi-gcc
AS          := arm-none-eabi-gcc
LD          := arm-none-eabi-gcc
OBJCOPY     := arm-none-eabi-objcopy
SIZE        := arm-none-eabi-size
GDB         := arm-none-eabi-gdb
QEMU_ARM    := qemu-system-arm

# Linker script
LINKER_SCRIPT := $(LINKER_DIR)/linker.ld

# Output
TARGET      := rk0_demo
ELF         := $(BUILD_DIR)/$(TARGET).elf
BIN         := $(BUILD_DIR)/$(TARGET).bin
HEX         := $(BUILD_DIR)/$(TARGET).hex

# Source files
C_SRCS :=  $(wildcard $(CORE_DIR)/src/*.c) \
           $(wildcard $(ARCH_DIR)/src/*.c) \
           $(wildcard $(APP_DIR)/src/*.c)

ASM_SRCS := $(wildcard $(ARCH_DIR)/src/*.S)

# Object files
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

# Flags
OPT      := -Og
CFLAGS   := -std=gnu11 $(MCU_FLAGS) -Wall -ffunction-sections -fdata-sections -g $(OPT) $(INC_DIRS)
ASFLAGS  := $(MCU_FLAGS) -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections -g
LDFLAGS  := -nostartfiles -T $(LINKER_SCRIPT) $(MCU_FLAGS) -Wl,-Map=$(ELF:.elf=.map),--cref -Wl,--gc-sections -specs=nano.specs -lc -lm -lnosys

# QEMU targets
QEMU_MACHINE := $(if $(filter $(ARCH),armv6m),microbit,lm3s6965evb)
QEMU_FLAGS := -machine $(QEMU_MACHINE) -nographic
QEMU_DEBUG_FLAGS := $(QEMU_FLAGS) -S -gdb tcp::1234

# Default rule
all: $(BIN) $(HEX)

$(ELF): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary -S $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -S $< $@

qemu: $(BIN)
	$(QEMU_ARM) $(QEMU_FLAGS) -kernel $<

qemu-debug: $(BIN)
	@echo "Start GDB with: $(GDB) $(ELF) -ex 'target remote localhost:1234'"
	$(QEMU_ARM) $(QEMU_DEBUG_FLAGS) -kernel $<

qemu-armv6m:
	$(MAKE) ARCH=armv6m qemu

qemu-armv7m:
	$(MAKE) ARCH=armv7m qemu

qemu-debug-armv6m:
	$(MAKE) ARCH=armv6m qemu-debug

qemu-debug-armv7m:
	$(MAKE) ARCH=armv7m qemu-debug

clean:
	rm -rf build

help:
	@echo "RK0 Makefile Help"
	@echo "-----------------"
	@echo "make               - Build the project (default: ARCH=armv6m)"
	@echo "make ARCH=...      - Build with specific architecture (e.g., armv7m)"
	@echo "make clean          - Remove build artifacts"
	@echo "make qemu           - Run the built binary in QEMU (microbit or lm3s6965evb)"
	@echo "make qemu-armv6m    - Build for armv6m and run in QEMU"
	@echo "make qemu-armv7m    - Build for armv7m and run in QEMU"
	@echo "make qemu-debug     - Run QEMU in GDB debug mode"
	@echo "make qemu-debug-armv6m - Debug build for armv6m in QEMU"
	@echo "make qemu-debug-armv7m - Debug build for armv7m in QEMU"
	@echo "make help           - Show this message"
	@echo ""
	@echo "Startup files should be named 'startup.c' and placed in:"
	@echo "  arch/<arch>/kernel/src/startup.c"
	@echo "Examples:"
	@echo "  arch/armv6m/kernel/src/startup.c"
	@echo "  arch/armv7m/kernel/src/startup.c"

.PHONY: all clean qemu qemu-debug qemu-armv6m qemu-armv7m qemu-debug-armv6m qemu-debug-armv7m help

