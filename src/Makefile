# WARNING: If you use this Makefile to build, you need to generate config files
# from settings.xml using this file as well, otherwise Refloat Cfg will not
# load in vesc_tool, see comment below.

SHELL := /bin/sh

VESC_TOOL ?= vesc_tool

TARGET = package_lib

all: $(TARGET)

# Recursively discover project sources so new modules do not require Makefile
# edits, while the existing object+depfile flow keeps the header graph accurate.
define find_local_c_sources
$(strip $(shell find $(1) -type f -name '*.c' -print | LC_ALL=C sort | sed 's#^\./##'))
endef

DISCOVERED_SOURCES := $(call find_local_c_sources,.)
REFLOAT_SOURCES := $(filter-out conf/buffer.c conf/confparser.c conf/confxml.c,$(DISCOVERED_SOURCES))
CONF_GEN_HEADERS = conf/conf_default.h conf/confparser.h conf/confxml.h
CONF_GEN_SOURCES = conf/confparser.c conf/confxml.c
CONF_GEN_FILES = $(CONF_GEN_HEADERS) $(CONF_GEN_SOURCES)
SOURCES = $(REFLOAT_SOURCES) $(CONF_GEN_SOURCES) conf/buffer.c
DEPS = $(SOURCES:.c=.d)

ADD_TO_CLEAN = $(CONF_GEN_FILES) $(DEPS) conf/conf_general.h

ROOT_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(ROOT_DIR)/..)
VESC_C_LIB_PATH = ../vesc_pkg_lib/
include $(VESC_C_LIB_PATH)rules.mk

CFLAGS := -MMD -flto -ggdb -I$(ROOT_DIR) $(CFLAGS)
LDFLAGS += -flto

$(REFLOAT_SOURCES): $(CONF_GEN_HEADERS) conf/conf_general.h

$(CONF_GEN_FILES) &: conf/settings.xml
	$(VESC_TOOL) --xmlConfToCode conf/settings.xml
	# !!! make xml config loading in vesc_tool work with LTO !!!
	sed -i "s/^uint8_t data_/__attribute__((used)) uint8_t data_/g" conf/confxml.c

PACKAGE_NAME := $(shell sed -n '1s/^\(.\{0,20\}\).*/\1/p' ../package_name)
VERSION := $(shell sed -n '1p' ../version)
MAJOR := $(word 1,$(subst ., ,$(VERSION)))
MINOR := $(word 2,$(subst ., ,$(VERSION)))
PATCH_SUFFIX := $(word 3,$(subst ., ,$(VERSION)))
PATCH := $(word 1,$(subst -, ,$(PATCH_SUFFIX)))
SUFFIX := $(shell printf '%s\n' '$(PATCH_SUFFIX)' | sed 's/^[^-]*-*//' | cut -c-20)
GIT_HASH := $(shell git rev-parse --short=8 HEAD | cut -c-8)
GIT_HEAD := $(shell git -C $(REPO_ROOT) rev-parse --git-path HEAD 2>/dev/null)
GIT_REF := $(shell git -C $(REPO_ROOT) symbolic-ref -q HEAD 2>/dev/null)
GIT_REF_FILE := $(if $(GIT_REF),$(shell git -C $(REPO_ROOT) rev-parse --git-path $(GIT_REF) 2>/dev/null))
GIT_PACKED_REFS := $(shell git -C $(REPO_ROOT) rev-parse --git-path packed-refs 2>/dev/null)
GIT_DEPS := $(wildcard $(foreach path,$(GIT_HEAD) $(GIT_REF_FILE) $(GIT_PACKED_REFS),$(abspath $(REPO_ROOT)/$(path))))

conf/conf_general.h: FORCE conf/conf_general.h.in ../package_name ../version $(GIT_DEPS)
	cat conf/conf_general.h.in | \
	sed "s/{{PACKAGE_NAME}}/$(PACKAGE_NAME)/g" | \
	sed "s/{{VERSION}}/$(VERSION)/g" | \
	sed "s/{{MAJOR_VERSION}}/$(MAJOR)/g" | \
	sed "s/{{MINOR_VERSION}}/$(MINOR)/g" | \
	sed "s/{{PATCH_VERSION}}/$(PATCH)/g" | \
	sed "s/{{VERSION_SUFFIX}}/$(SUFFIX)/g" | \
	sed "s/{{GIT_HASH}}/$(GIT_HASH)/g" > $@.tmp
	if ! cmp -s $@.tmp $@; then mv $@.tmp $@; else rm -f $@.tmp; fi

FORCE:

.PHONY: FORCE print-sources print-objects print-artifacts

print-sources:
	@printf '%s\n' $(SOURCES)

print-objects:
	@printf '%s\n' $(OBJECTS)

print-artifacts:
	@printf '%s\n' $(TARGET_ELF) $(TARGET_BIN) $(TARGET_LISP) $(TARGET_LIST)

-include $(DEPS)
