SHELL := /bin/sh

VESC_TOOL ?= vesc_tool
MINIFY_QML ?= 1
OLDVT ?= 0

PACKAGE_ARTIFACT := refloat.vescpkg
PACKAGE_README := package_README-gen.md
PACKAGE_QML := ui.qml
PACKAGE_LIB := src/package_lib.bin
PACKAGE_INPUTS := pkgdesc.qml lisp/package.lisp lisp/bms.lisp $(PACKAGE_README) $(PACKAGE_QML) $(PACKAGE_LIB)
# Keep aligned with VESC firmware's LISP_MAX_SIZE (128 KiB - 8 bytes).
PACKAGE_LISP_MAX_SIZE ?= 131064
# Conservative guard for the complete package artifact, including metadata.
PACKAGE_MAX_SIZE ?= 131064

PACKAGE_NAME := $(shell sed -n '1s/^\(.\{0,20\}\).*/\1/p' package_name)
VERSION := $(shell sed -n '1p' version)
PACKAGE_SOURCE_PATHS := Makefile package_name version pkgdesc.qml lisp src package_README.md ui.qml.in vesc_pkg_lib
PACKAGE_SOURCE_COMMIT := $(shell git log -1 --format=%h -- $(PACKAGE_SOURCE_PATHS))
PACKAGE_SOURCE_DATE := $(shell git log -1 --format=%cI -- $(PACKAGE_SOURCE_PATHS))
GIT_HEAD := $(shell git rev-parse --git-path HEAD 2>/dev/null)
GIT_REF := $(shell git symbolic-ref -q HEAD 2>/dev/null)
GIT_REF_FILE := $(if $(GIT_REF),$(shell git rev-parse --git-path $(GIT_REF) 2>/dev/null))
GIT_PACKED_REFS := $(shell git rev-parse --git-path packed-refs 2>/dev/null)
GIT_DEPS := $(wildcard $(GIT_HEAD) $(GIT_REF_FILE) $(GIT_PACKED_REFS))

ifeq ($(OLDVT), 1)
PACKAGE_BUILD_ARGS := --buildPkg "$(PACKAGE_ARTIFACT):lisp/package.lisp:$(PACKAGE_QML):0:$(PACKAGE_README):Refloat"
else
PACKAGE_BUILD_ARGS := --buildPkgFromDesc pkgdesc.qml
endif

ifeq ($(strip $(MINIFY_QML)),1)
MINIFY_CMD := ./rjsmin.py
else
MINIFY_CMD := cat
endif

.DELETE_ON_ERROR:

all: package

package: test-gated-package

.NOTPARALLEL: test-gated-package
test-gated-package: clean-package-artifacts check $(PACKAGE_ARTIFACT) check-package-size

check:
	$(MAKE) -C tests check
	$(MAKE) -C tests check-qml-behavior
	$(MAKE) -C tests check-python-contracts
	$(MAKE) -C tests check-lisp

check-vesc-tool:
	@if ! command -v "$(VESC_TOOL)" >/dev/null 2>&1 && [ ! -x "$(VESC_TOOL)" ]; then \
		echo "VESC_TOOL '$(VESC_TOOL)' was not found; set VESC_TOOL=/path/to/vesc_tool" >&2; \
		exit 127; \
	fi

package-only: $(PACKAGE_ARTIFACT)

check-package-size: $(PACKAGE_ARTIFACT)
	@size=$$(wc -c < "$(PACKAGE_ARTIFACT)"); \
	if [ "$$size" -gt "$(PACKAGE_MAX_SIZE)" ]; then \
		echo "$(PACKAGE_ARTIFACT) is $$size bytes; VESC limit is $(PACKAGE_MAX_SIZE) bytes" >&2; \
		exit 1; \
	fi; \
	echo "$(PACKAGE_ARTIFACT): $$size/$(PACKAGE_MAX_SIZE) bytes"

$(PACKAGE_ARTIFACT): check-vesc-tool $(PACKAGE_INPUTS)
	@set -e; \
	report=$$(mktemp); \
	trap 'rm -f "$$report"' EXIT; \
	if ! "$(VESC_TOOL)" $(PACKAGE_BUILD_ARGS) >"$$report" 2>&1; then cat "$$report"; exit 1; fi; \
	cat "$$report"; \
	lisp_size=$$(awk '/Lisp data size/ {print $$5; exit}' "$$report"); \
	if [ -n "$$lisp_size" ] && [ "$$lisp_size" -gt "$(PACKAGE_LISP_MAX_SIZE)" ]; then \
		echo "Lisp payload exceeds VESC limit: $$lisp_size/$(PACKAGE_LISP_MAX_SIZE) bytes" >&2; exit 1; \
	fi

$(PACKAGE_LIB): FORCE
	$(MAKE) -C src package_lib

$(PACKAGE_README): FORCE package_README.md version $(GIT_DEPS)
	cp package_README.md $@
	echo "" >> $@
	echo "### Build Info" >> $@
	echo "- Version: $(VERSION)" >> $@
	echo "- Build Date: $(PACKAGE_SOURCE_DATE)" >> $@
	echo "- Git Commit: #$(PACKAGE_SOURCE_COMMIT)" >> $@

$(PACKAGE_QML): ui.qml.in package_name version
	cat $< | \
	sed "s/{{PACKAGE_NAME}}/$(PACKAGE_NAME)/g" | \
	sed "s/{{VERSION}}/$(VERSION)/g" | \
	$(MINIFY_CMD) > $@

clean:
	$(MAKE) clean-package-artifacts
	$(MAKE) -C src clean

clean-package-artifacts:
	rm -f $(PACKAGE_ARTIFACT) $(PACKAGE_README) $(PACKAGE_QML)

help:
	@printf '%s\n' \
		'Targets:' \
		'  make              run tests, then build refloat.vescpkg' \
		'  make check        run the full automated test gate' \
		'  make package      same as make' \
		'  make package-only build refloat.vescpkg without running tests' \
		'  make check-package-size verify the VESC package size limit' \
		'  make -C tests help  inspect the host-test build surface' \
		'  make -C src help    inspect the package-lib build surface' \
		'  make clean        remove generated package and firmware artifacts' \
		'' \
		'Options:' \
		'  VESC_TOOL=/path/to/vesc_tool' \
		'  PACKAGE_LISP_MAX_SIZE=131064  override the firmware Lisp payload ceiling' \
		'  PACKAGE_MAX_SIZE=131064       override the conservative artifact ceiling' \
		'  OLDVT=1           use old VESC Tool package builder' \
		'  MINIFY_QML=0      package readable QML'

FORCE:

.PHONY: all check check-vesc-tool package test-gated-package package-only check-package-size clean clean-package-artifacts help FORCE
