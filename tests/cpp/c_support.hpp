#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstddef>
#include <cstdint>

#define time_t refloat_time_t
extern "C" {
#include "alert_tracker.h"
#include "atr.h"
#include "balance_filter.h"
#include "bms.h"
#include "booster.h"
#include "brake_tilt.h"
#include "charging.h"
#include "conf/buffer.h"
#include "conf/confparser.h"
#include "conf/confxml.h"
#include "data_recorder.h"
#include "filters/biquad.h"
#include "filters/ema.h"
#include "filters/sma.h"
#include "filters/smooth_setpoint.h"
#include "footpad_sensor.h"
#include "frequency_tracker.h"
#include "haptic_feedback.h"
#include "imu.h"
#include "konami.h"
#include "lcm.h"
#include "led_driver.h"
#include "led_strip.h"
#include "lib/circular_buffer.h"
#include "lib/transitions.h"
#include "lib/utils.h"
#include "motor_control.h"
#include "pid.h"
#include "remote.h"
#include "reverse_stop.h"
#include "state.h"
#include "time.h"
#include "torque_tilt.h"
#include "turn_tilt.h"
#include "vesc_if_fake.h"
}
#undef time_t

using SignalHandler = void (*)(int);

struct BufferItem {
    uint8_t lo;
    uint8_t hi;
};

inline bool buffer_item_eq(BufferItem lhs, BufferItem rhs) {
    return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
}

#define CHECK_FLOAT_NEAR(actual, expected) REQUIRE(!(std::fabs((actual) - (expected)) > 1e-4f))

#define CHECK_U32(actual, expected)                                                                \
    REQUIRE(static_cast<uint32_t>(actual) == static_cast<uint32_t>(expected))

extern "C" {
void feedback_fakes_reset(void);
size_t feedback_fakes_play_tone_calls(void);
size_t feedback_fakes_stop_tone_calls(void);
size_t feedback_fakes_led_confirm_calls(void);
uint16_t feedback_fakes_last_frequency(void);
float feedback_fakes_last_intensity(void);
void lcm_fakes_set_runtime_status(bool enabled, bool headlights_enabled);
}
