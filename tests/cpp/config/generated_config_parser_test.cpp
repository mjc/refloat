#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

extern "C" {
#include "conf/confparser.h"
}

namespace {

TEST_CASE("generated config parser sets defaults and signature", "[generated-config]") {
    RefloatConfig cfg{};
    confparser_set_defaults_refloatconfig(&cfg);

    CHECK(cfg.kp == Catch::Approx(20.0f));
    CHECK(cfg.atr.filter.time_constant == Catch::Approx(0.3f));
    CHECK(cfg.leds.on);
    CHECK(cfg.hardware.leds.mode == LED_MODE_OFF);
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

} // namespace
