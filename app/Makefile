# Forward app-local make invocations to the repository build system.

RK0_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

.DEFAULT_GOAL := all

.PHONY: all
all:
	@$(MAKE) -f $(RK0_ROOT)/Makefile all

.PHONY: %
%:
	@$(MAKE) -f $(RK0_ROOT)/Makefile $@
