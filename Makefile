
# Target architecture selection
# Options: M3, M4, M7
# Default: M3 if not specified
CORTEX ?= M3

# Toolchain definitions
CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
AR = arm-none-eabi-ar
SIZE = arm-none-eabi-size
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
GDB = arm-none-eabi-gdb

# Project directories
SRC_DIR = Src
INC_DIR = Inc
BUILD_DIR = Build
LIB_DIR = Lib
APP_SRC_DIR = app/Src
APP_INC_DIR = app/Inc
LINKER_DIR = Linker

# Architecture-specific settings
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

# Newlib configuration
NEWLIB_FLAGS = -specs=nano.specs -specs=nosys.specs

# Compiler flags - RK0 Kernel
RK0_CFLAGS = $(ARCH_FLAGS)
RK0_CFLAGS += -Wall -Wextra -std=gnu11 
RK0_CFLAGS += -ffunction-sections -fdata-sections
RK0_CFLAGS += -I$(INC_DIR)
RK0_CFLAGS += $(ARCH_DEFS)
RK0_CFLAGS += -Os g
RK0_CFLAGS += $(NEWLIB_FLAGS)

# Compiler flags - Application
APP_CFLAGS = $(ARCH_FLAGS)
APP_CFLAGS += -Wall -Wextra
APP_CFLAGS += -ffunction-sections -fdata-sections
APP_CFLAGS += -I$(INC_DIR) -I$(APP_INC_DIR)
APP_CFLAGS += $(ARCH_DEFS)
APP_CFLAGS += -Os g
APP_CFLAGS += $(NEWLIB_FLAGS)

# Assembler flags
ASFLAGS = $(ARCH_FLAGS)

# Linker flags
LDFLAGS = $(ARCH_FLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,-Map=$(MAP_FILE)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += $(NEWLIB_FLAGS)

# RK0 Kernel source files
RK0_C_SRCS = $(wildcard $(SRC_DIR)/*.c)
RK0_ASM_SRCS = $(wildcard $(SRC_DIR)/*.S)

# RK0 Kernel object files
RK0_C_OBJS = $(RK0_C_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/rk0_%.o)
RK0_ASM_OBJS = $(RK0_ASM_SRCS:$(SRC_DIR)/%.S=$(BUILD_DIR)/rk0_%.o)
RK0_OBJS = $(RK0_C_OBJS) $(RK0_ASM_OBJS)

# Application source files
APP_C_SRCS = $(wildcard $(APP_SRC_DIR)/*.c)
APP_ASM_SRCS = $(wildcard $(APP_SRC_DIR)/*.S)
# Add main.c and startup file
EXTRA_SRCS = main.c startup_cortexm.c

# Application object files
APP_C_OBJS = $(APP_C_SRCS:$(APP_SRC_DIR)/%.c=$(BUILD_DIR)/app_%.o)
APP_ASM_OBJS = $(APP_ASM_SRCS:$(APP_SRC_DIR)/%.S=$(BUILD_DIR)/app_%.o)
EXTRA_OBJS = $(EXTRA_SRCS:%.c=$(BUILD_DIR)/%.o)
APP_OBJS = $(APP_C_OBJS) $(APP_ASM_OBJS) $(EXTRA_OBJS)

# Output files
RK0_LIB = $(LIB_DIR)/librk0.a
ELF_FILE = $(BUILD_DIR)/rk0_app.elf
BIN_FILE = $(BUILD_DIR)/rk0_app.bin
HEX_FILE = $(BUILD_DIR)/rk0_app.hex
MAP_FILE = $(BUILD_DIR)/rk0_app.map
DISASM_FILE = $(BUILD_DIR)/rk0_app.disasm

# Memory layout for target
LDSCRIPT = $(LINKER_DIR)/lm3s6965_flash_$(CORTEX).ld
LDSCRIPT_GENERIC = $(LINKER_DIR)/cortex_m_generic.ld

# QEMU settings
QEMU_ARM = qemu-system-arm
QEMU_FLAGS = -machine lm3s6965evb -nographic -no-reboot
QEMU_DEBUG_FLAGS = $(QEMU_FLAGS) -S -gdb tcp::1234

# Default target
all: print_arch_info rk0_lib app

# Print architecture info
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

$(LINKER_DIR):
	mkdir -p $(LINKER_DIR)

# Generate the linker scripts
$(LDSCRIPT_GENERIC): | $(LINKER_DIR)
	@echo "Generating generic linker script: $@"
	@cp cortex_m_generic.ld $@

$(LDSCRIPT): | $(LINKER_DIR)
	@echo "Generating board specific linker script: $@"
	@cp lm3s6965_flash_$(CORTEX).ld $@

#-----------------
# RK0 Kernel Build
#-----------------

# Target to build RK0 kernel library
rk0_lib: $(RK0_LIB)

# Compile RK0 kernel C files
$(BUILD_DIR)/rk0_%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "CC [RK0] $<"
	$(CC) $(RK0_CFLAGS) -c $< -o $@

# Assemble RK0 kernel ASM files
$(BUILD_DIR)/rk0_%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	@echo "AS [RK0] $<"
	$(AS) $(ASFLAGS) -c $< -o $@

# Create RK0 static library
$(RK0_LIB): $(RK0_OBJS) | $(LIB_DIR)
	@echo "Creating RK0 library: $@"
	$(AR) rcs $@ $(RK0_OBJS)
	$(SIZE) $@

#--------------------
# Application Build
#--------------------

# Target to build application
app: $(ELF_FILE) $(BIN_FILE) $(HEX_FILE) $(DISASM_FILE)

# Compile application C files
$(BUILD_DIR)/app_%.o: $(APP_SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "CC [APP] $<"
	$(CC) $(APP_CFLAGS) -c $< -o $@

# Compile extra C files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "CC [APP] $<"
	$(CC) $(APP_CFLAGS) -c $< -o $@

# Assemble application ASM files
$(BUILD_DIR)/app_%.o: $(APP_SRC_DIR)/%.S | $(BUILD_DIR)
	@echo "AS [APP] $<"
	$(AS) $(ASFLAGS) -c $< -o $@

# Link application with RK0 library
$(ELF_FILE): $(APP_OBJS) $(RK0_LIB) $(LDSCRIPT) $(LDSCRIPT_GENERIC) | $(BUILD_DIR)
	@echo "Linking application with RK0: $@"
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) -L$(LIB_DIR) -lrk0
	$(SIZE) $@

# Create binary file
$(BIN_FILE): $(ELF_FILE)
	@echo "Creating binary: $@"
	$(OBJCOPY) -O binary $< $@

# Create hex file
$(HEX_FILE): $(ELF_FILE)
	@echo "Creating hex: $@"
	$(OBJCOPY) -O ihex $< $@

# Create disassembly file for analysis
$(DISASM_FILE): $(ELF_FILE)
	@echo "Creating disassembly: $@"
	$(OBJDUMP) -D $< > $@

#------------------
# Run in QEMU
#------------------

# Run in QEMU
qemu: $(BIN_FILE)
	$(QEMU_ARM) $(QEMU_FLAGS) -kernel $<

# Run M3 in QEMU
qemu-m3: m3 qemu

# Run M4 in QEMU (Note: lm3s6965evb is an M3 board in QEMU)
qemu-m4: m4 qemu

# Run M7 in QEMU (Note: lm3s6965evb is an M3 board in QEMU)
qemu-m7: m7 qemu

# Debug in QEMU (starts QEMU and waits for GDB to connect)
qemu-debug: $(BIN_FILE)
	@echo "Starting QEMU in debug mode at localhost:1234"
	@echo "Connect with: $(GDB) $(ELF_FILE) -ex 'target remote localhost:1234'"
	$(QEMU_ARM) $(QEMU_DEBUG_FLAGS) -kernel $(BIN_FILE)

# Debug for M3 in QEMU
qemu-debug-m3: m3 qemu-debug


#------------------
# Clean & Rebuild
#------------------

# Clean RK0 kernel build artifacts
clean-rk0:
	@echo "Cleaning RK0 build artifacts..."
	rm -f $(RK0_OBJS) $(RK0_LIB)

# Clean application build artifacts
clean-app:
	@echo "Cleaning application build artifacts..."
	rm -f $(APP_OBJS) $(ELF_FILE) $(BIN_FILE) $(HEX_FILE) $(MAP_FILE) $(DISASM_FILE)

# Clean all build artifacts
clean: clean-rk0 clean-app
	@echo "Cleaning all build artifacts..."
	rm -rf $(BUILD_DIR) $(LIB_DIR) $(LINKER_DIR)

# Rebuild everything
rebuild: clean all


# Start GDB and connect to running QEMU instance
gdb:
	@echo "Connecting to QEMU..."
	$(GDB) $(ELF_FILE) -ex "target remote localhost:1234"



#------------------
# Help
#------------------

# Show help
help:
	@echo "RK0 Kernel and Application Makefile"
	@echo "==================================="
	@echo "Targets:"
	@echo "  all        - Build RK0 library and application (default)"
	@echo "  rk0_lib    - Build only the RK0 library"
	@echo "  app        - Build only the application (uses previously built RK0 library)"
	@echo "  m3         - Build for Cortex-M3 without FPU (default)"
	@echo "  m4         - Build for Cortex-M4 with FPU"
	@echo "  m7         - Build for Cortex-M7 with FPU"
	@echo "  qemu       - Run the application in QEMU"
	@echo "  qemu-m3    - Build for Cortex-M3 and run in QEMU"
	@echo "  qemu-debug   - Run QEMU in debug mode, waiting for GDB to connect"
	@echo "  gdb          - Connect GDB to a running QEMU instance"
	@echo "  clean-rk0  - Clean RK0 kernel build artifacts"
	@echo "  clean-app  - Clean application build artifacts"
	@echo "  clean      - Clean all build artifacts"
	@echo "  rebuild    - Clean and rebuild everything"
	@echo "  help       - Show this help"
	@echo ""
	@echo "File outputs:"
	@echo "  $(RK0_LIB)       - RK0 static library"
	@echo "  $(ELF_FILE)      - ELF executable"
	@echo "  $(BIN_FILE)      - Binary file for flashing"
	@echo "  $(HEX_FILE)      - HEX file for flashing"
	@echo "  $(MAP_FILE)      - Memory map file"
	@echo "  $(DISASM_FILE)   - Disassembly file for analysis"

.PHONY: all rk0_lib app m3 m4 m7 print_arch_info qemu qemu-m3 qemu-m4 qemu-m7 clean-rk0 clean-app clean rebuild help
