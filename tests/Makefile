SHELL := /bin/sh

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
TEST_BIN := refloat-c-tests
LEDS_TEST_BIN := refloat-led-tests
MAIN_TEST_BIN := refloat-main-tests
CC ?= cc
GENERATED_CONF := $(ROOT_DIR)/src/conf/conf_general.h $(ROOT_DIR)/src/conf/confparser.c $(ROOT_DIR)/src/conf/confxml.c
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

COMMON_SOURCES = \
	$(ROOT_DIR)/src/conf/buffer.c \
	$(ROOT_DIR)/src/lib/circular_buffer.c \
	$(ROOT_DIR)/src/lib/utils.c \
	$(ROOT_DIR)/src/state.c \
	$(ROOT_DIR)/src/time.c \
	$(ROOT_DIR)/src/filters/biquad.c \
	$(ROOT_DIR)/src/filters/ema.c \
	$(ROOT_DIR)/src/filters/sma.c \
	$(ROOT_DIR)/src/filters/smooth_setpoint.c \
	$(ROOT_DIR)/src/pid.c \
	$(ROOT_DIR)/src/frequency_tracker.c \
	$(ROOT_DIR)/src/lib/transitions.c \
	$(ROOT_DIR)/src/lcm.c \
	$(ROOT_DIR)/src/data_recorder.c \
	$(ROOT_DIR)/src/motor_data.c \
	$(ROOT_DIR)/src/motor_control.c \
	$(ROOT_DIR)/src/footpad_sensor.c \
	$(ROOT_DIR)/src/remote.c \
	$(ROOT_DIR)/src/imu.c \
	$(ROOT_DIR)/src/balance_filter.c \
	$(ROOT_DIR)/src/charging.c \
	$(ROOT_DIR)/src/bms.c \
	$(ROOT_DIR)/src/led_strip.c \
	$(ROOT_DIR)/src/led_driver.c \
	$(ROOT_DIR)/src/atr.c \
	$(ROOT_DIR)/src/booster.c \
	$(ROOT_DIR)/src/brake_tilt.c \
	$(ROOT_DIR)/src/alert_tracker.c \
	$(ROOT_DIR)/src/haptic_feedback.c \
	$(ROOT_DIR)/src/reverse_stop.c \
	$(ROOT_DIR)/src/torque_tilt.c \
	$(ROOT_DIR)/src/turn_tilt.c \
	$(ROOT_DIR)/src/konami.c \
	$(ROOT_DIR)/src/conf/confparser.c \
	$(ROOT_DIR)/src/conf/confxml.c

SOURCES = \
	vesc_if_fake.c \
	feedback_fakes.c \
	lcm_fakes.c \
	c_tests.c \
	$(COMMON_SOURCES)

MAIN_SOURCES = \
	vesc_if_fake.c \
	feedback_fakes.c \
	lcm_fakes.c \
	leds_main_fakes.c \
	main_protocol_tests.c \
	$(COMMON_SOURCES) \
	main_protocol_wrapper.c

LEDS_SOURCES = \
	vesc_if_fake.c \
	led_driver_fake.c \
	led_tests/tests.c \
	$(ROOT_DIR)/src/conf/buffer.c \
	$(ROOT_DIR)/src/lib/utils.c \
	$(ROOT_DIR)/src/state.c \
	$(ROOT_DIR)/src/led_strip.c \
	$(ROOT_DIR)/src/leds.c

all: $(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)

generated_conf: $(GENERATED_CONF)

$(GENERATED_CONF) &: $(ROOT_DIR)/src/conf/settings.xml $(ROOT_DIR)/src/conf/conf_general.h.in $(ROOT_DIR)/package_name $(ROOT_DIR)/version $(GIT_DEPS)
	$(MAKE) -C $(ROOT_DIR)/src conf/conf_general.h conf/confparser.c conf/confxml.c

$(TEST_BIN): generated_conf $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

$(LEDS_TEST_BIN): $(LEDS_SOURCES)
	$(CC) $(LEDS_CPPFLAGS) $(CFLAGS) -o $@ $(LEDS_SOURCES) $(LDLIBS)

$(MAIN_TEST_BIN): generated_conf $(MAIN_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(MAIN_SOURCES) $(LDLIBS)

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
	rm -f $(TEST_BIN) $(LEDS_TEST_BIN) $(MAIN_TEST_BIN)

.PHONY: all check check-qml-behavior check-smoke clean
.PHONY: generated_conf check-main

check-main: $(MAIN_TEST_BIN)
	./$(MAIN_TEST_BIN)
