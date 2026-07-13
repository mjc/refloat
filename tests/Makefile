SHELL := /bin/sh

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
TEST_BIN := refloat-c-tests
LEDS_TEST_BIN := refloat-led-tests
MAIN_TEST_BIN := refloat-main-tests
BUILD_DIR := .build
COVERAGE_BUILD_DIR := .coverage
COVERAGE_REPORT_DIR := $(COVERAGE_BUILD_DIR)/report
COVERAGE_TEST_BIN := $(COVERAGE_BUILD_DIR)/refloat-c-tests
COVERAGE_LEDS_TEST_BIN := $(COVERAGE_BUILD_DIR)/refloat-led-tests
COVERAGE_MAIN_TEST_BIN := $(COVERAGE_BUILD_DIR)/refloat-main-tests
SANITIZER_BUILD_DIR := .sanitize
SANITIZER_TEST_BIN := $(SANITIZER_BUILD_DIR)/refloat-c-tests
SANITIZER_LEDS_TEST_BIN := $(SANITIZER_BUILD_DIR)/refloat-led-tests
SANITIZER_MAIN_TEST_BIN := $(SANITIZER_BUILD_DIR)/refloat-main-tests
GCOVR ?= gcovr
GCOV ?= $(shell realpath "$$($(CC) -print-file-name=libgcc.a)" | sed 's@/lib/gcc/.*@/bin/gcov@')
COVERAGE_MIN_LINE ?= 100
COVERAGE_MIN_BRANCH ?= 99
CC ?= cc
GENERATED_CONF := $(ROOT_DIR)/src/conf/conf_general.h $(ROOT_DIR)/src/conf/confparser.c $(ROOT_DIR)/src/conf/confxml.c
GENERATED_PROJECT_SOURCES := src/conf/confparser.c src/conf/confxml.c
GIT_HEAD := $(shell git -C $(ROOT_DIR) rev-parse --git-path HEAD 2>/dev/null)
GIT_REF := $(shell git -C $(ROOT_DIR) symbolic-ref -q HEAD 2>/dev/null)
GIT_REF_FILE := $(if $(GIT_REF),$(shell git -C $(ROOT_DIR) rev-parse --git-path $(GIT_REF) 2>/dev/null))
GIT_PACKED_REFS := $(shell git -C $(ROOT_DIR) rev-parse --git-path packed-refs 2>/dev/null)
GIT_DEPS := $(wildcard $(GIT_HEAD) $(GIT_REF_FILE) $(GIT_PACKED_REFS))

CPPFLAGS += -D_GNU_SOURCE -DIS_VESC_LIB -include $(ROOT_DIR)/tests/vesc_if_fake.h -I$(ROOT_DIR)/tests -I$(ROOT_DIR)/tests/stubs -I$(ROOT_DIR)/src -I$(ROOT_DIR)/src/lib -I$(ROOT_DIR)/src/filters -I$(ROOT_DIR)/vesc_pkg_lib
LEDS_CPPFLAGS := $(CPPFLAGS) -D__time_t_defined
CFLAGS += -std=gnu11 -Wall -Wextra -Werror -Wno-error=array-bounds
CFLAGS += -Wno-error=pointer-to-int-cast -Wno-error=maybe-uninitialized
LDLIBS += -lm

define objects_for
$(addprefix $(BUILD_DIR)/$(1)/,$(patsubst %.c,%.o,$(2)))
endef

SRC_TOP_LEVEL_SOURCES := $(patsubst $(ROOT_DIR)/%,%,$(wildcard $(ROOT_DIR)/src/*.c))
SRC_LIB_SOURCES := $(patsubst $(ROOT_DIR)/%,%,$(wildcard $(ROOT_DIR)/src/lib/*.c))
SRC_FILTER_SOURCES := $(patsubst $(ROOT_DIR)/%,%,$(wildcard $(ROOT_DIR)/src/filters/*.c))
COMMON_PROJECT_SOURCES := \
	$(filter-out src/main.c src/leds.c src/conf/buffer.c $(GENERATED_PROJECT_SOURCES),$(sort $(SRC_TOP_LEVEL_SOURCES) $(SRC_LIB_SOURCES) $(SRC_FILTER_SOURCES))) \
	$(GENERATED_PROJECT_SOURCES) \
	src/conf/buffer.c
LEDS_PROJECT_SOURCES := src/conf/buffer.c src/lib/utils.c src/state.c src/led_strip.c src/leds.c

TEST_SOURCES := \
	vesc_if_fake.c \
	feedback_fakes.c \
	lcm_fakes.c \
	c_tests.c \
	$(COMMON_PROJECT_SOURCES)

MAIN_SOURCES := \
	vesc_if_fake.c \
	feedback_fakes.c \
	lcm_fakes.c \
	leds_main_fakes.c \
	main_protocol_tests.c \
	$(COMMON_PROJECT_SOURCES)

LEDS_SOURCES := \
	vesc_if_fake.c \
	led_driver_fake.c \
	led_tests/tests.c \
	$(LEDS_PROJECT_SOURCES)

TEST_OBJECTS := $(call objects_for,c,$(TEST_SOURCES))
MAIN_OBJECTS := $(call objects_for,main,$(MAIN_SOURCES))
LEDS_OBJECTS := $(call objects_for,leds,$(LEDS_SOURCES))
DEPFILES := $(TEST_OBJECTS:.o=.d) $(MAIN_OBJECTS:.o=.d) $(LEDS_OBJECTS:.o=.d)

VPATH := $(CURDIR) $(ROOT_DIR)

all: $(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)

generated_conf: $(GENERATED_CONF)

$(GENERATED_CONF) &: $(ROOT_DIR)/src/conf/settings.xml $(ROOT_DIR)/src/conf/conf_general.h.in $(ROOT_DIR)/package_name $(ROOT_DIR)/version $(GIT_DEPS)
	$(MAKE) -C $(ROOT_DIR)/src conf/conf_general.h conf/confparser.c conf/confxml.c

$(TEST_OBJECTS) $(MAIN_OBJECTS): | generated_conf

$(BUILD_DIR)/c/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/main/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/leds/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(LEDS_CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(TEST_BIN): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LDLIBS) -o $@

$(LEDS_TEST_BIN): $(LEDS_OBJECTS)
	$(CC) $(LEDS_OBJECTS) $(LDLIBS) -o $@

$(MAIN_TEST_BIN): $(MAIN_OBJECTS)
	$(CC) $(MAIN_OBJECTS) $(LDLIBS) -o $@

check: $(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)
	./$(TEST_BIN)
	./$(LEDS_TEST_BIN)
	./$(MAIN_TEST_BIN)

coverage: check-lisp
	@command -v $(GCOVR) >/dev/null 2>&1 || \
		{ printf '%s\n' 'coverage requires gcovr (set GCOVR to its executable if it is not on PATH)' >&2; exit 1; }
	rm -rf $(COVERAGE_BUILD_DIR)
	$(MAKE) BUILD_DIR=$(COVERAGE_BUILD_DIR) \
		TEST_BIN=$(COVERAGE_TEST_BIN) \
		LEDS_TEST_BIN=$(COVERAGE_LEDS_TEST_BIN) \
		MAIN_TEST_BIN=$(COVERAGE_MAIN_TEST_BIN) \
		CFLAGS='$(CFLAGS) -O1 --coverage' \
		LDLIBS='$(LDLIBS) --coverage' check
	mkdir -p $(COVERAGE_REPORT_DIR)
	$(GCOVR) --gcov-executable $(GCOV) --root $(ROOT_DIR) \
		--object-directory $(CURDIR)/$(COVERAGE_BUILD_DIR) \
		--filter '$(ROOT_DIR)/src/.*\.c$$' \
		--exclude '$(ROOT_DIR)/src/conf/(confparser|confxml)\.c$$' \
		--exclude-unreachable-branches \
		--exclude-branches-by-pattern '^\s*(SEND_APP_DATA|log_(error|msg)\(|led_strip_animate\(|.*system_time_age\()' \
		--fail-under-line $(COVERAGE_MIN_LINE) \
		--fail-under-branch $(COVERAGE_MIN_BRANCH) \
		--txt $(COVERAGE_REPORT_DIR)/coverage.txt \
		--json-summary $(COVERAGE_REPORT_DIR)/coverage.json \
		--print-summary
	@python3 -c 'import json; d = json.load(open("$(COVERAGE_REPORT_DIR)/coverage.json")); assert d["line_total"] > 0 and d["branch_total"] > 0, "coverage report is empty"'

sanitize:
	rm -rf $(SANITIZER_BUILD_DIR)
	REFLOAT_SKIP_XFAIL=1 \
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
	$(MAKE) BUILD_DIR=$(SANITIZER_BUILD_DIR) \
		TEST_BIN=$(SANITIZER_TEST_BIN) \
		LEDS_TEST_BIN=$(SANITIZER_LEDS_TEST_BIN) \
		MAIN_TEST_BIN=$(SANITIZER_MAIN_TEST_BIN) \
		CFLAGS='$(CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
		LDLIBS='$(LDLIBS) -fsanitize=address,undefined' check

check-qml-behavior:
	cd $(ROOT_DIR) && \
		qt_bin="$$(dirname "$$(command -v qmltestrunner)")" && \
		qt_root="$$(dirname "$$qt_bin")" && \
		QML2_IMPORT_PATH="$$qt_root/lib/qt-6/qml:$${QML2_IMPORT_PATH:-}" \
		QML_IMPORT_PATH="$$qt_root/lib/qt-6/qml:$${QML_IMPORT_PATH:-}" \
		QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/qml

check-python-contracts:
	cd $(ROOT_DIR) && python3 tests/python/test_contracts.py

check-lisp:
	cd $(ROOT_DIR) && python3 tests/lisp_tests.py

help:
	@printf '%s\n' \
		'Targets:' \
		'  make              build the host test binaries' \
		'  make check        build and run all host tests' \
		'  make coverage     run host tests and write line/branch coverage reports' \
		'  COVERAGE_MIN_LINE=100 COVERAGE_MIN_BRANCH=100  set coverage floors' \
		'  make sanitize     run host tests under AddressSanitizer and UBSan' \
		'  make check-main   run only the main protocol host tests' \
		'  make check-qml-behavior run the QML behavior check' \
		'  make check-python-contracts run Python helper contract checks' \
		'  make check-lisp      run package and BMS Lisp behavior checks' \
		'  make generated_conf generate the config sources used by the host tests' \
		'  make clean        remove test build outputs'

clean:
	rm -rf $(BUILD_DIR) $(COVERAGE_BUILD_DIR) $(SANITIZER_BUILD_DIR) \
		$(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)

-include $(DEPFILES)

.PHONY: all check coverage sanitize check-qml-behavior check-python-contracts check-lisp clean help
.PHONY: generated_conf check-main

check-main: $(MAIN_TEST_BIN)
	./$(MAIN_TEST_BIN)
