#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include "conf/confparser.h"
#include "conf/confxml.h"
}

#define CHECK_FLOAT_CLOSE(actual, expected) CHECK(std::fabs((actual) - (expected)) < 1e-4f)

TEST_CASE("generated config parser sets defaults and signature", "[generated-config]") {
    RefloatConfig cfg{};
    confparser_set_defaults_refloatconfig(&cfg);

    CHECK_FLOAT_CLOSE(cfg.kp, 20.0f);
    CHECK_FLOAT_CLOSE(cfg.atr.filter.time_constant, 0.3f);
    CHECK_FLOAT_CLOSE(cfg.torque_tilt.filter.off_speed_time_constant, 0.16f);
    CHECK(cfg.leds.on);
    CHECK(cfg.leds.headlights_on);
    CHECK(cfg.hardware.leds.mode == LED_MODE_OFF);
    CHECK(cfg.hardware.leds.status.color_order == LED_COLOR_GRB);
    CHECK(!cfg.bms.enabled);
    CHECK(cfg.meta.is_default);

    std::array<uint8_t, SERIALIZED_CONFIG_LENGTH> buffer{};
    const int32_t len = confparser_serialize_refloatconfig(buffer.data(), &cfg);
    REQUIRE(len == SERIALIZED_CONFIG_LENGTH);
    CHECK(buffer[0] == ((REFLOATCONFIG_SIGNATURE >> 24) & 0xffu));
    CHECK(buffer[1] == ((REFLOATCONFIG_SIGNATURE >> 16) & 0xffu));
    CHECK(buffer[2] == ((REFLOATCONFIG_SIGNATURE >> 8) & 0xffu));
    CHECK(buffer[3] == (REFLOATCONFIG_SIGNATURE & 0xffu));
}

TEST_CASE("generated config parser round trips representative values", "[generated-config]") {
    RefloatConfig cfg{};
    confparser_set_defaults_refloatconfig(&cfg);

    cfg.kp = 31.5f;
    cfg.ki = 0.01234f;
    cfg.atr.filter.on_speed_limit = 12.25f;
    cfg.torque_tilt.filter.time_constant = 0.45f;
    cfg.remote.filter.time_constant = 0.35f;
    cfg.inputtilt_deadband = 0.125f;
    cfg.leds.front.mode = LED_ANIM_RAINBOW_ROLL;
    cfg.leds.front.color1 = COLOR_FERRARI;
    cfg.leds.front.brightness = 0.75f;
    cfg.leds.status.idle_timeout = 4321u;
    cfg.hardware.leds.mode = LED_MODE_BOTH;
    cfg.hardware.leds.front.count = 33u;
    cfg.hardware.leds.front.reverse = true;
    cfg.haptic.duty.frequency = 713u;
    cfg.haptic.current_threshold = 0.42f;
    cfg.bms.enabled = true;
    cfg.bms.cell_lv_threshold = 2.95f;
    cfg.meta.is_default = false;

    std::array<uint8_t, SERIALIZED_CONFIG_LENGTH> buffer{};
    REQUIRE(confparser_serialize_refloatconfig(buffer.data(), &cfg) == SERIALIZED_CONFIG_LENGTH);

    RefloatConfig parsed{};
    REQUIRE(confparser_deserialize_refloatconfig(buffer.data(), &parsed));
    CHECK_FLOAT_CLOSE(parsed.kp, 31.5f);
    CHECK_FLOAT_CLOSE(parsed.ki, 0.01234f);
    CHECK_FLOAT_CLOSE(parsed.atr.filter.on_speed_limit, 12.25f);
    CHECK_FLOAT_CLOSE(parsed.torque_tilt.filter.time_constant, 0.45f);
    CHECK_FLOAT_CLOSE(parsed.remote.filter.time_constant, 0.35f);
    CHECK_FLOAT_CLOSE(parsed.inputtilt_deadband, 0.125f);
    CHECK(parsed.leds.front.mode == LED_ANIM_RAINBOW_ROLL);
    CHECK(parsed.leds.front.color1 == COLOR_FERRARI);
    CHECK_FLOAT_CLOSE(parsed.leds.front.brightness, 0.75f);
    CHECK(parsed.leds.status.idle_timeout == 4321u);
    CHECK(parsed.hardware.leds.mode == LED_MODE_BOTH);
    CHECK(parsed.hardware.leds.front.count == 33u);
    CHECK(parsed.hardware.leds.front.reverse);
    CHECK(parsed.haptic.duty.frequency == 713u);
    CHECK_FLOAT_CLOSE(parsed.haptic.current_threshold, 0.42f);
    CHECK(parsed.bms.enabled);
    CHECK_FLOAT_CLOSE(parsed.bms.cell_lv_threshold, 2.95f);
    CHECK(!parsed.meta.is_default);

    std::array<uint8_t, SERIALIZED_CONFIG_LENGTH> second{};
    REQUIRE(confparser_serialize_refloatconfig(second.data(), &parsed) == SERIALIZED_CONFIG_LENGTH);
    CHECK(std::memcmp(buffer.data(), second.data(), buffer.size()) == 0);
}

TEST_CASE("generated config parser rejects bad signature", "[generated-config]") {
    RefloatConfig cfg{};
    confparser_set_defaults_refloatconfig(&cfg);

    std::array<uint8_t, SERIALIZED_CONFIG_LENGTH> buffer{};
    REQUIRE(confparser_serialize_refloatconfig(buffer.data(), &cfg) == SERIALIZED_CONFIG_LENGTH);

    buffer[0] ^= 0x80u;

    RefloatConfig parsed{};
    parsed.kp = -1.0f;
    CHECK_FALSE(confparser_deserialize_refloatconfig(buffer.data(), &parsed));
    CHECK_FLOAT_CLOSE(parsed.kp, -1.0f);
}

TEST_CASE("generated config xml blob is populated", "[generated-config]") {
    REQUIRE(DATA_REFLOATCONFIG__SIZE > 1024);

    const uint32_t unpacked_size = ((uint32_t) data_refloatconfig_[0] << 24) |
                                   ((uint32_t) data_refloatconfig_[1] << 16) |
                                   ((uint32_t) data_refloatconfig_[2] << 8) |
                                   (uint32_t) data_refloatconfig_[3];
    CHECK(unpacked_size > DATA_REFLOATCONFIG__SIZE);

    uint32_t nonzero_count = 0;
    for (uint32_t i = 0; i < 64 && i < DATA_REFLOATCONFIG__SIZE; ++i) {
        if (data_refloatconfig_[i] != 0) {
            ++nonzero_count;
        }
    }
    CHECK(nonzero_count > 32);
    CHECK(data_refloatconfig_[DATA_REFLOATCONFIG__SIZE - 1] != 0);
}
