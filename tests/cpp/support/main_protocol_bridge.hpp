#pragma once

#include <cstddef>
#include <cstdint>

#define time_t refloat_time_t
extern "C" {
#include "conf/buffer.h"
#include "data.h"
#include "vesc_if_fake.h"

bool init(lib_info *info);
}
#undef time_t

#include <catch2/catch_test_macros.hpp>

static inline bool main_protocol_start(Data **data) {
    vesc_if_fake_reset();

    lib_info info{};
    REQUIRE(init(&info));
    REQUIRE(info.arg != nullptr);
    REQUIRE(info.stop_fun != nullptr);
    vesc_if_fake_set_arg(info.arg);

    *data = static_cast<Data *>(info.arg);
    return true;
}
