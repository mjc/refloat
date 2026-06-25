#pragma once

// Firmware headers define their own time_t; keep that out of libc/C++ headers.
#define time_t refloat_time_t
extern "C" {
#include "conf/buffer.h"
#include "footpad_sensor.h"
#include "lcm.h"
#include "motor_data.h"
#include "state.h"
#include "vesc_if_fake.h"
}
#undef time_t
