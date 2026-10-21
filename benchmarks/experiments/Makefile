# Run from the repository root: make -f benchmarks/experiments/Makefile run
EXPERIMENT_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
RK0_ROOT := $(abspath $(EXPERIMENT_DIR)/../..)
ARCH ?= armv7m
BUILD ?= PROFILE
PLATFORM ?= qemu
SAMPLES ?= 5
WORK ?= 4000
INTERFERENCE ?= 40000
ITERATIONS ?= 256
CAPACITIES ?= 1 4
CHAIN_DEPTHS ?= 1 2 3
CHAIN_DEPTH ?= 2
CHAIN_CEILING ?= 3
E3_MODE ?= composition
E2_MODE ?= ablation
CUSTODY_VARIANT ?= COMPOSED
CUSTODY_CAPACITIES ?= 1 4 8
RATIOS ?= 1 10 40
TIMEOUT ?= 20
PYTHON ?= python3
EXPERIMENTS ?= E1 E2 E3
MODE ?= pip
CAPACITY ?= 4
CEILING ?= 3
EXPERIMENT ?= E1
EXP_OUTPUT ?= build/experiments/$(ARCH)/$(BUILD)
TASK_COUNT := 5
COMMON_DEFS = -DRK_CONF_TRACE=OFF -DRK_CONF_N_USRTASKS_MAX=$(TASK_COUNT)U \
 -DRK_CONF_MUTEX=ON -DRK_CONF_ASYNCH_MESG=ON -DRK_CONF_SYNCH_MESG=ON \
 -DRK_CONF_SYSTICK_DIV=1000UL -DEXP_SAMPLES=$(SAMPLES)U \
 -DEXP_WORK=$(WORK)UL -DEXP_INTERFERENCE=$(INTERFERENCE)UL \
 -DEXP_ITERATIONS=$(ITERATIONS)U
E1_MODE_pip := 0
E1_MODE_ceiling := 1
E1_MODE_invocation := 2
E1_MODE_unprotected := 3

.PHONY: run run-e2-custody build help
run:
	$(PYTHON) "$(EXPERIMENT_DIR)run.py" --arch $(ARCH) --build $(BUILD) \
	 --samples $(SAMPLES) --work $(WORK) --interference $(INTERFERENCE) \
	 --iterations $(ITERATIONS) --capacities $(CAPACITIES) --timeout $(TIMEOUT) \
	 --chain-depths $(CHAIN_DEPTHS) \
	 --output "$(EXP_OUTPUT)" $(if $(filter 1 ON on yes,$(CONTROL)),--inversion-control) $(EXPERIMENTS)

run-e2-custody:
	$(PYTHON) "$(EXPERIMENT_DIR)run_e2_custody.py" --arch $(ARCH) --build $(BUILD) \
	 --samples $(SAMPLES) --work $(WORK) --ratios $(RATIOS) \
	 --capacities $(CUSTODY_CAPACITIES) --timeout $(TIMEOUT) \
	 --output "$(EXP_OUTPUT)/e2-custody"

ifeq ($(EXPERIMENT),E1)
APP_SOURCE := benchmarks/experiments/e1_serialized.c
VARIANT := e1_$(MODE)
VARIANT_DEFS := -DEXP_E1_MODE=$(E1_MODE_$(MODE))
ifeq ($(E1_MODE_$(MODE)),)
$(error Unknown E1 MODE=$(MODE))
endif
else ifeq ($(EXPERIMENT),E2)
ifeq ($(E2_MODE),ablation)
APP_SOURCE := benchmarks/experiments/e2_async.c
VARIANT := e2_n$(CAPACITY)_c$(CEILING)
VARIANT_DEFS := -DEXP_CAPACITY=$(CAPACITY)U -DEXP_CEILING=$(CEILING)U
else ifeq ($(E2_MODE),custody)
CUSTODY_ID_COMPOSED := 0
CUSTODY_ID_RK0_NOC := 1
CUSTODY_ID_RK0_CEIL := 2
CUSTODY_ID_STATIC_PRIORITY := 3
ifeq ($(CUSTODY_ID_$(CUSTODY_VARIANT)),)
$(error Unknown CUSTODY_VARIANT=$(CUSTODY_VARIANT))
endif
APP_SOURCE := benchmarks/experiments/e2_custody.c
VARIANT := e2_custody_$(CUSTODY_VARIANT)_n$(CAPACITY)_w$(WORK)_i$(INTERFERENCE)
VARIANT_DEFS := -DEXP_CAPACITY=$(CAPACITY)U -DEXP_E2_VARIANT=$(CUSTODY_ID_$(CUSTODY_VARIANT)) \
 -I$(EXPERIMENT_DIR)
OBS_DIR := build/e2-observers/$(ARCH)/$(BUILD)/$(VARIANT)
OBS_NATIVE := $(addprefix $(OBS_DIR)/,kmesg.c kmesgq.c ksch.c)
CASE_NATIVE := $(filter-out core/src/kmesg.c core/src/kmesgq.c core/src/ksch.c,\
 $(patsubst $(RK0_ROOT)/%,%,$(wildcard $(RK0_ROOT)/core/src/*.c)))
CASE_ARCH := $(patsubst $(RK0_ROOT)/%,%,$(wildcard $(RK0_ROOT)/arch/$(ARCH)/kernel/src/*.c))
CASE_SUPPORT := $(filter-out app/src/application.c,\
 $(patsubst $(RK0_ROOT)/%,%,$(wildcard $(RK0_ROOT)/app/src/*.c)))
CASE_SOURCES := $(CASE_NATIVE) $(OBS_NATIVE) $(CASE_ARCH) $(CASE_SUPPORT) $(APP_SOURCE)
else
$(error E2_MODE must be ablation or custody)
endif
else ifeq ($(EXPERIMENT),E3)
ifeq ($(E3_MODE),composition)
APP_SOURCE := benchmarks/experiments/e3_composition.c
VARIANT := e3_composition
VARIANT_DEFS :=
else ifeq ($(E3_MODE),chain)
CHAIN_TASKS_1 := 5
CHAIN_TASKS_2 := 7
CHAIN_TASKS_3 := 9
TASK_COUNT := $(CHAIN_TASKS_$(CHAIN_DEPTH))
ifeq ($(TASK_COUNT),)
$(error CHAIN_DEPTH must be 1, 2, or 3)
endif
APP_SOURCE := benchmarks/experiments/e3_chain.c
VARIANT := e3_chain_d$(CHAIN_DEPTH)_c$(CHAIN_CEILING)
VARIANT_DEFS := -DEXP_CHAIN_DEPTH=$(CHAIN_DEPTH)U -DEXP_CHAIN_CEILING=$(CHAIN_CEILING)U
else
$(error E3_MODE must be composition or chain)
endif
else
$(error Unknown EXPERIMENT=$(EXPERIMENT))
endif

build:
	$(if $(OBS_DIR),$(PYTHON) "$(EXPERIMENT_DIR)observe_e2.py" "$(RK0_ROOT)/$(OBS_DIR)",@true)
	$(MAKE) -C "$(RK0_ROOT)" -B PLATFORM=$(PLATFORM) ARCH=$(ARCH) BUILD=$(BUILD) \
	 BUILD_DIR="$(EXP_OUTPUT)/$(VARIANT)" APP="$(APP_SOURCE)" TARGET=$(VARIANT) \
	 EXTRA_DEFS='$(COMMON_DEFS) $(VARIANT_DEFS)' \
	 $(if $(CASE_SOURCES),C_SRCS='$(CASE_SOURCES)') image

help:
	@echo 'E2 architectural comparison: make -f benchmarks/experiments/Makefile run-e2-custody'
	@echo 'run: E1 PIP/ceiling + separate invocation; E2 bounded async; E3 composition and PIP-chain costs'
	@echo 'make -f benchmarks/experiments/Makefile run ARCH=armv6m SAMPLES=5'
	@echo 'make -f benchmarks/experiments/Makefile build EXPERIMENT=E1 MODE=invocation'
	@echo 'make -f benchmarks/experiments/Makefile build EXPERIMENT=E3 E3_MODE=chain CHAIN_DEPTH=3'
	@echo 'Optional: CONTROL=1, CAPACITIES="1 4 8", CHAIN_DEPTHS="1 2 3", EXPERIMENTS="E1 E3", BUILD=DEBUG'
