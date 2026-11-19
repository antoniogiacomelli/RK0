# Resource-sharing paper experiments, measured with DWT_CYCCNT on NUCLEO-F103RB.
# Invoke from the repository root: make -f benchmarks/resource_sharing/Makefile
RS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
RS_DIR := $(dir $(RS_MAKEFILE))
RK0_ROOT := $(abspath $(RS_DIR)/../..)

PLATFORM ?= stm32f103rb
ifneq ($(PLATFORM),stm32f103rb)
$(error This suite requires PLATFORM=stm32f103rb; QEMU counts are not hardware cycles)
endif
BUILD ?= PROFILE
ifeq ($(filter $(BUILD),PROFILE DEBUG RELEASE),)
$(error BUILD must be PROFILE, DEBUG or RELEASE)
endif
SAMPLES ?= 5
BATCH ?= 256
WORK ?= 4000
MEDIUM_WORK ?= 40000
CORE_HZ ?= 72000000
TICK_HZ ?= 1000
FLASH_TOOL ?= openocd
RS_FLASH_EXT = $(if $(filter openocd,$(FLASH_TOOL)),elf,bin)
PYTHON ?= python3
BENCHES ?= e1 e2 e3
RS_BUILD_ROOT ?= build/armv7m/stm32f103rb/resource_sharing/$(BUILD)
RS_REV := $(shell git -C "$(RK0_ROOT)" describe --always --dirty --abbrev=40 2>/dev/null || echo unknown)
RS_OPT := $(if $(filter PROFILE,$(BUILD)),-O2,$(if $(filter RELEASE,$(BUILD)),-Os,-O0))
RS_DEFS := -DRK_CONF_TRACE=OFF -DRK_CONF_N_USRTASKS_MAX=8U \
    -DRK_CONF_MUTEX=ON -DRK_CONF_SYNCH_MESG=ON -DRK_CONF_ASYNCH_MESG=ON \
    -DRK_CONF_ERR_CHECK=OFF -DRK_CONF_FAULT=OFF \
    -DRS_SAMPLES=$(SAMPLES)U -DRS_BATCH=$(BATCH)U \
    -DRS_WORK=$(WORK)U -DRS_MEDIUM_WORK=$(MEDIUM_WORK)U \
    -DRS_REVISION=\"$(RS_REV)\" -DRS_BUILD_NAME=\"$(BUILD)\" -DRS_OPT_NAME=\"$(RS_OPT)\"

.DEFAULT_GOAL := build
.PHONY: build run help build-e1 build-e2 build-e3 flash-e1 flash-e2 flash-e3

build: $(addprefix build-,$(BENCHES))

# Force a fresh build: make does not otherwise track command-line configuration.
define BUILD_EXPERIMENT
	$(MAKE) -C "$(RK0_ROOT)" -B ARCH=armv7m PLATFORM=stm32f103rb BUILD=$(BUILD) \
	    BUILD_DIR="$(RS_BUILD_ROOT)/$(1)" APP="benchmarks/resource_sharing/$(1).c" \
	    TARGET="rk0_rs_$(1)" F103RB_SYSCORECLK=$(CORE_HZ)UL \
	    F103RB_SYSTICK_DIV=$(TICK_HZ)UL EXTRA_DEFS='$(RS_DEFS)' all
endef

build-e1:
	$(call BUILD_EXPERIMENT,e1)
build-e2:
	$(call BUILD_EXPERIMENT,e2)
build-e3:
	$(call BUILD_EXPERIMENT,e3)

define FLASH_EXPERIMENT
	$(MAKE) -C "$(RK0_ROOT)" PLATFORM=stm32f103rb FLASH_TOOL=$(FLASH_TOOL) \
	    IMAGE="$(RS_BUILD_ROOT)/$(1)/rk0_rs_$(1).$(RS_FLASH_EXT)" flash
endef

flash-e1: build-e1
	$(call FLASH_EXPERIMENT,e1)
flash-e2: build-e2
	$(call FLASH_EXPERIMENT,e2)
flash-e3: build-e3
	$(call FLASH_EXPERIMENT,e3)

run:
	$(PYTHON) "$(RS_DIR)/run.py" --port "$(SERIAL_PORT)" --flash-tool "$(FLASH_TOOL)" \
	    --build "$(BUILD)" --samples "$(SAMPLES)" --batch "$(BATCH)" \
	    --work "$(WORK)" --medium-work "$(MEDIUM_WORK)" \
	    --core-hz "$(CORE_HZ)" --tick-hz "$(TICK_HZ)" \
	    --build-root "$(RS_BUILD_ROOT)" \
	    $(if $(LOG_DIR),--output "$(LOG_DIR)") $(BENCHES)

help:
	@echo 'NUCLEO-F103RB resource-sharing experiments (DWT CPU cycles)'
	@echo '  make -f benchmarks/resource_sharing/Makefile build'
	@echo '  make -f benchmarks/resource_sharing/Makefile run SERIAL_PORT=/dev/ttyACM0'
	@echo '  make -f benchmarks/resource_sharing/Makefile flash-e2 FLASH_TOOL=openocd'
	@echo 'Options: BENCHES="e1 e2 e3" SAMPLES=5 BATCH=256 WORK=4000 MEDIUM_WORK=40000'
	@echo '         CORE_HZ=72000000 TICK_HZ=1000 BUILD=PROFILE LOG_DIR=<directory>'
