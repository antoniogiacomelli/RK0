# Simple Makefile for RK0 Real-Time Kernel

# Target architecture selection
# Options: M3, M4, M7
# Default: M3 if not specified
CORTEX ?= M3

# Toolchain definitions
CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
AR = arm-none-eabi-ar
SIZE = arm-none-eabi-size

# Project directories
SRC_DIR = Src
INC_DIR = Inc
BUILD_DIR = Build
LIB_DIR = Lib

# Architecture-specific settings (without info messages)
ifeq ($(CORTEX),M3)
  ARCH_FLAGS = -mcpu=cortex-m3 -mthumb
  ARCH_DEFS = -D__CORTEX_M3 -D__FPU_PRESENT=0
else ifeq ($(CORTEX),M4)
  ARCH_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
  ARCH_DEFS = -D__CORTEX_M4 -D__FPU_PRESENT=1
else ifeq ($(CORTEX),M7)
  ARCH_FLAGS = -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
  ARCH_DEFS = -D__CORTEX_M7 -D__FPU_PRESENT=1
else
  # Default to M3 if an invalid option is provided
  CORTEX = M3
  ARCH_FLAGS = -mcpu=cortex-m3 -mthumb
  ARCH_DEFS = -D__CORTEX_M3 -D__FPU_PRESENT=0
endif

# Compiler flags
CFLAGS = $(ARCH_FLAGS)
CFLAGS += -Wall -Wextra
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -I$(INC_DIR)  
CFLAGS += $(ARCH_DEFS)
CFLAGS += -Os -g

# Newlib configuration
# Use nano.specs for smaller footprint version of newlib
NEWLIB_FLAGS = -specs=nano.specs -specs=nosys.specs

# Support for standard C library
# Add -u _printf_float if you need floating point printf support
CFLAGS += $(NEWLIB_FLAGS)

# Assembler flags
ASFLAGS = $(ARCH_FLAGS)

# Source files
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.s)

# Object files
C_OBJS = $(C_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ASM_OBJS = $(ASM_SRCS:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
OBJS = $(C_OBJS) $(ASM_OBJS)

# Output library
KERNEL_LIB = $(LIB_DIR)/librk0.a

# Default target
all: print_arch_info $(KERNEL_LIB)

# Print architecture info (only for build targets)
print_arch_info:
	@echo "Building for Cortex-$(CORTEX)$(if $(findstring M3,$(CORTEX)), without FPU, with FPU)"

# Architecture-specific targets
m3:
	@$(MAKE) CORTEX=M3

m4:
	@$(MAKE) CORTEX=M4

m7:
	@$(MAKE) CORTEX=M7

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	@echo "AS $<"
	$(AS) $(ASFLAGS) -c $< -o $@

# Build the kernel library
$(KERNEL_LIB): $(OBJS) | $(LIB_DIR)
	@echo "Creating library: $@"
	$(AR) rcs $@ $(OBJS)
	$(SIZE) $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# Rebuild everything
rebuild: clean all

# Show help
help:
	@echo "RK0 Kernel Makefile"
	@echo "==================="
	@echo "Targets:"
	@echo "  all      - Build the kernel library (default for Cortex-M3)"
	@echo "  m3       - Build for Cortex-M3 without FPU (default)"
	@echo "  m4       - Build for Cortex-M4 with FPU"
	@echo "  m7       - Build for Cortex-M7 with FPU"
	@echo "  clean    - Remove build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  help     - Show this help"
	@echo ""
	@echo "You can also specify the architecture manually:"
	@echo "  make CORTEX=M3    - Build for Cortex-M3 without FPU (default)"
	@echo "  make CORTEX=M4    - Build for Cortex-M4 with FPU"
	@echo "  make CORTEX=M7    - Build for Cortex-M7 with FPU"

.PHONY: all clean rebuild help m3 m4 m7 print_arch_info
