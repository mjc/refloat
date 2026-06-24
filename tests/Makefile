SHELL := /bin/sh

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
GENERATED_CONF := $(ROOT_DIR)/src/conf/conf_general.h $(ROOT_DIR)/src/conf/confparser.c $(ROOT_DIR)/src/conf/confxml.c
GIT_HEAD := $(shell git -C $(ROOT_DIR) rev-parse --git-path HEAD 2>/dev/null)
GIT_REF := $(shell git -C $(ROOT_DIR) symbolic-ref -q HEAD 2>/dev/null)
GIT_REF_FILE := $(if $(GIT_REF),$(shell git -C $(ROOT_DIR) rev-parse --git-path $(GIT_REF) 2>/dev/null))
GIT_PACKED_REFS := $(shell git -C $(ROOT_DIR) rev-parse --git-path packed-refs 2>/dev/null)
GIT_DEPS := $(wildcard $(foreach path,$(GIT_HEAD) $(GIT_REF_FILE) $(GIT_PACKED_REFS),$(abspath $(ROOT_DIR)/$(path))))

all: check

generated_conf: $(GENERATED_CONF)

$(GENERATED_CONF) &: $(ROOT_DIR)/src/conf/settings.xml $(ROOT_DIR)/src/conf/conf_general.h.in $(ROOT_DIR)/package_name $(ROOT_DIR)/version $(GIT_DEPS)
	$(MAKE) -C $(ROOT_DIR)/src conf/conf_general.h conf/confparser.c conf/confxml.c

check:
	cd $(ROOT_DIR) && cmake --workflow --preset host-debug

check-qml-behavior:
	cd $(ROOT_DIR) && \
		qt_bin="$$(dirname "$$(command -v qmltestrunner)")" && \
		qt_root="$$(dirname "$$qt_bin")" && \
		QML2_IMPORT_PATH="$$qt_root/lib/qt-6/qml:$${QML2_IMPORT_PATH:-}" \
		QML_IMPORT_PATH="$$qt_root/lib/qt-6/qml:$${QML_IMPORT_PATH:-}" \
		QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/qml

check-smoke: check-qml-behavior

clean:
	rm -rf $(ROOT_DIR)/build/host-debug

.PHONY: all check check-qml-behavior check-smoke clean
.PHONY: generated_conf
