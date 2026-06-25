SHELL := /bin/sh

VESC_TOOL ?= vesc_tool
MINIFY_QML ?= 1
OLDVT ?= 0

PACKAGE_ARTIFACT := refloat.vescpkg
PACKAGE_README := package_README-gen.md
PACKAGE_QML := ui.qml
PACKAGE_LIB := src/package_lib.bin
PACKAGE_INPUTS := pkgdesc.qml lisp/package.lisp lisp/bms.lisp $(PACKAGE_README) $(PACKAGE_QML) $(PACKAGE_LIB)

PACKAGE_NAME := $(shell sed -n '1s/^\(.\{0,20\}\).*/\1/p' package_name)
VERSION := $(shell sed -n '1p' version)
GIT_COMMIT := $(shell git rev-parse --short HEAD)
GIT_HEAD := $(shell git rev-parse --git-path HEAD 2>/dev/null)
GIT_REF := $(shell git symbolic-ref -q HEAD 2>/dev/null)
GIT_REF_FILE := $(if $(GIT_REF),$(shell git rev-parse --git-path $(GIT_REF) 2>/dev/null))
GIT_DEPS := $(GIT_HEAD) $(GIT_REF_FILE)

ifeq ($(strip $(MINIFY_QML)),1)
MINIFY_CMD := ./rjsmin.py
else
MINIFY_CMD := cat
endif

.DELETE_ON_ERROR:

all: package

package: test-gated-package

.NOTPARALLEL: test-gated-package
test-gated-package: clean-package-artifacts check $(PACKAGE_ARTIFACT)

check:
	$(MAKE) -C tests check
	$(MAKE) -C tests check-qml-behavior

check-vesc-tool:
	@if ! command -v "$(VESC_TOOL)" >/dev/null 2>&1 && [ ! -x "$(VESC_TOOL)" ]; then \
		echo "VESC_TOOL '$(VESC_TOOL)' was not found; set VESC_TOOL=/path/to/vesc_tool" >&2; \
		exit 127; \
	fi

package-only: $(PACKAGE_ARTIFACT)

$(PACKAGE_ARTIFACT): check-vesc-tool $(PACKAGE_INPUTS)
ifeq ($(OLDVT), 1)
	"$(VESC_TOOL)" --buildPkg "$(PACKAGE_ARTIFACT):lisp/package.lisp:$(PACKAGE_QML):0:$(PACKAGE_README):Refloat"
else
	"$(VESC_TOOL)" --buildPkgFromDesc pkgdesc.qml
endif

$(PACKAGE_LIB): FORCE
	$(MAKE) -C src package_lib

$(PACKAGE_README): FORCE package_README.md version $(GIT_DEPS)
	cp package_README.md $@
	echo "" >> $@
	echo "### Build Info" >> $@
	echo "- Version: $(VERSION)" >> $@
	echo "- Build Date: `date --rfc-3339=seconds`" >> $@
	echo "- Git Commit: #$(GIT_COMMIT)" >> $@

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
		'  make -C tests help  inspect the host-test build surface' \
		'  make -C src help    inspect the package-lib build surface' \
		'  make clean        remove generated package and firmware artifacts' \
		'' \
		'Options:' \
		'  VESC_TOOL=/path/to/vesc_tool' \
		'  OLDVT=1           use old VESC Tool package builder' \
		'  MINIFY_QML=0      package readable QML'

FORCE:

.PHONY: all check check-vesc-tool package test-gated-package package-only clean clean-package-artifacts help FORCE
