#include "firmware_support.hpp"

#include <array>
#include <cstring>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

TEST_CASE("lcm disabled responses are minimal", "[c][firmware]") {
    vesc_if_fake_reset();

    LcmData lcm{};
    lcm.enabled = false;
    lcm.brightness = 12;
    lcm.brightness_idle = 34;
    lcm.status_brightness = 56;
    lcm.payload_size = 2;
    lcm.payload[0] = 77;
    lcm.payload[1] = 88;
    std::memcpy(lcm.name, "External", 9);

    std::array<uint8_t, 4> request = {'N', 'e', 'w', '\0'};
    lcm_poll_request(&lcm, request.data(), request.size());
    REQUIRE(std::strcmp(lcm.name, "External") == 0);

    std::array<unsigned char, 6> ctrl = {1, 2, 3, 4, 5, 6};
    lcm_light_ctrl_request(&lcm, ctrl.data(), static_cast<int>(ctrl.size()));
    CHECK(static_cast<std::uint32_t>(lcm.brightness) == 12u);
    CHECK(static_cast<std::uint32_t>(lcm.payload_size) == 2u);

    State state{};
    state.state = STATE_RUNNING;
    MotorData motor{};
    lcm_poll_response(&lcm, &state, FS_BOTH, &motor, 45.0f);
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(static_cast<std::uint32_t>(len) == 2u);
    CHECK(static_cast<std::uint32_t>(payload[0]) == 101u);
    CHECK(static_cast<std::uint32_t>(payload[1]) == COMMAND_LCM_POLL);
    CHECK(static_cast<std::uint32_t>(lcm.payload_size) == 2u);
}

} // namespace
