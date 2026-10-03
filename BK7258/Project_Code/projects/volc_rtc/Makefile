VOLC_MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(VOLC_MAKEFILE_DIR)/../..)
VOLC_PROJECT_NAME := $(notdir $(VOLC_MAKEFILE_DIR))
BUILD_LOGGER := $(REPO_ROOT)/tools/make_build_logger.sh
PYTHON_ENV := $(VOLC_MAKEFILE_DIR)/build/Dependencies/PythonEnv

SOC_TARGETS := $(notdir $(basename $(wildcard $(REPO_ROOT)/bk_idk/middleware/soc/*/*.defconfig)))
FORWARD_TARGETS := $(SOC_TARGETS) all clean help doc menuconfig

.DEFAULT_GOAL := help
.PHONY: $(FORWARD_TARGETS)

$(FORWARD_TARGETS):
	+@PATH="$(PYTHON_ENV)/bin:$$PATH" ARMINO_PYTHON_ENV_PATH="$(PYTHON_ENV)" "$(BUILD_LOGGER)" "$(VOLC_MAKEFILE_DIR)" -C "$(REPO_ROOT)" "$@" PROJECT="$(VOLC_PROJECT_NAME)" BUILD_DIR="$(VOLC_MAKEFILE_DIR)/build"

.DEFAULT:
	+@PATH="$(PYTHON_ENV)/bin:$$PATH" ARMINO_PYTHON_ENV_PATH="$(PYTHON_ENV)" "$(BUILD_LOGGER)" "$(VOLC_MAKEFILE_DIR)" -C "$(REPO_ROOT)" "$@" PROJECT="$(VOLC_PROJECT_NAME)" BUILD_DIR="$(VOLC_MAKEFILE_DIR)/build"
