SHELL := /bin/sh

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
TEST_BIN := refloat-c-tests
LEDS_TEST_BIN := refloat-led-tests
MAIN_TEST_BIN := refloat-main-tests
BUILD_DIR := .build
CC ?= cc
GENERATED_CONF := $(ROOT_DIR)/src/conf/conf_general.h $(ROOT_DIR)/src/conf/confparser.c $(ROOT_DIR)/src/conf/confxml.c
GENERATED_PROJECT_SOURCES := src/conf/confparser.c src/conf/confxml.c
GIT_HEAD := $(shell git -C $(ROOT_DIR) rev-parse --git-path HEAD 2>/dev/null)
GIT_REF := $(shell git -C $(ROOT_DIR) symbolic-ref -q HEAD 2>/dev/null)
GIT_REF_FILE := $(if $(GIT_REF),$(shell git -C $(ROOT_DIR) rev-parse --git-path $(GIT_REF) 2>/dev/null))
GIT_PACKED_REFS := $(shell git -C $(ROOT_DIR) rev-parse --git-path packed-refs 2>/dev/null)
GIT_DEPS := $(wildcard $(foreach path,$(GIT_HEAD) $(GIT_REF_FILE) $(GIT_PACKED_REFS),$(abspath $(ROOT_DIR)/$(path))))

CPPFLAGS += -D_GNU_SOURCE -DIS_VESC_LIB -include $(ROOT_DIR)/tests/vesc_if_fake.h -I$(ROOT_DIR)/tests -I$(ROOT_DIR)/tests/stubs -I$(ROOT_DIR)/src -I$(ROOT_DIR)/src/lib -I$(ROOT_DIR)/src/filters -I$(ROOT_DIR)/vesc_pkg_lib
LEDS_CPPFLAGS := $(CPPFLAGS) -D__time_t_defined
CFLAGS += -std=gnu11 -Wall -Wextra -Werror -Wno-error=array-bounds
CFLAGS += -Wno-error=pointer-to-int-cast -Wno-error=maybe-uninitialized
LDLIBS += -lm

# Discover project sources automatically, then let compiler-generated depfiles
# handle the header graph for each object.
define find_repo_c_sources
$(strip $(shell cd $(ROOT_DIR) && find $(1) -type f -name '*.c' -print | LC_ALL=C sort))
endef

define objects_for
$(addprefix $(BUILD_DIR)/$(1)/,$(patsubst %.c,%.o,$(2)))
endef

SRC_DISCOVERED_SOURCES := $(call find_repo_c_sources,src)
COMMON_PROJECT_SOURCES := \
	$(filter-out src/main.c src/leds.c $(GENERATED_PROJECT_SOURCES),$(SRC_DISCOVERED_SOURCES)) \
	$(GENERATED_PROJECT_SOURCES)
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
	$(COMMON_PROJECT_SOURCES) \
	main_protocol_wrapper.c

LEDS_SOURCES := \
	vesc_if_fake.c \
	led_driver_fake.c \
	led_tests/tests.c \
	$(LEDS_PROJECT_SOURCES)

TEST_OBJECTS := $(call objects_for,c,$(TEST_SOURCES))
MAIN_OBJECTS := $(call objects_for,main,$(MAIN_SOURCES))
LEDS_OBJECTS := $(call objects_for,leds,$(LEDS_SOURCES))
DEPFILES := $(TEST_OBJECTS:.o=.d) $(MAIN_OBJECTS:.o=.d) $(LEDS_OBJECTS:.o=.d)
TRACKED_BUILD_ARTIFACTS := $(sort $(TEST_OBJECTS) $(MAIN_OBJECTS) $(LEDS_OBJECTS) $(DEPFILES))

VPATH := $(CURDIR) $(ROOT_DIR)

all: $(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)

generated_conf: $(GENERATED_CONF)

sync-build-artifacts:
	@mkdir -p $(BUILD_DIR)
	@find $(BUILD_DIR) -type f \( -name '*.o' -o -name '*.d' \) -print | \
	while IFS= read -r path; do \
		case " $(TRACKED_BUILD_ARTIFACTS) " in \
			*" $$path "*) ;; \
			*) rm -f "$$path" ;; \
		esac; \
	done

$(GENERATED_CONF) &: $(ROOT_DIR)/src/conf/settings.xml $(ROOT_DIR)/src/conf/conf_general.h.in $(ROOT_DIR)/package_name $(ROOT_DIR)/version $(GIT_DEPS)
	$(MAKE) -C $(ROOT_DIR)/src conf/conf_general.h conf/confparser.c conf/confxml.c

$(TEST_OBJECTS) $(MAIN_OBJECTS): | generated_conf sync-build-artifacts
$(LEDS_OBJECTS): | sync-build-artifacts

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

check-qml-behavior:
	cd $(ROOT_DIR) && \
		qt_bin="$$(dirname "$$(command -v qmltestrunner)")" && \
		qt_root="$$(dirname "$$qt_bin")" && \
		QML2_IMPORT_PATH="$$qt_root/lib/qt-6/qml:$${QML2_IMPORT_PATH:-}" \
		QML_IMPORT_PATH="$$qt_root/lib/qt-6/qml:$${QML_IMPORT_PATH:-}" \
		QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/qml

check-smoke: check-qml-behavior

clean:
	rm -rf $(BUILD_DIR) $(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)

-include $(DEPFILES)

.PHONY: all check check-qml-behavior check-smoke clean sync-build-artifacts
.PHONY: generated_conf check-main

check-main: $(MAIN_TEST_BIN)
	./$(MAIN_TEST_BIN)
