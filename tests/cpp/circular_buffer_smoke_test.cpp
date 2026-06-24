#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include "circular_buffer.h"

void led_driver_fake_reset(void);
std::size_t led_driver_fake_setup_calls(void);
}

TEST_CASE("circular buffer keeps the newest items after wrapping", "[circular-buffer]") {
    std::array<uint8_t, 3> backing{};
    CircularBuffer buffer{};
    circular_buffer_init(&buffer, sizeof(uint8_t), backing.size(), backing.data());

    const uint8_t first = 10;
    const uint8_t second = 20;
    const uint8_t third = 30;
    const uint8_t fourth = 40;

    circular_buffer_push(&buffer, &first);
    circular_buffer_push(&buffer, &second);
    circular_buffer_push(&buffer, &third);
    circular_buffer_push(&buffer, &fourth);

    REQUIRE(circular_buffer_size(&buffer) == 3);

    uint8_t item = 0;
    REQUIRE(circular_buffer_get(&buffer, 0, &item));
    CHECK(item == second);
    REQUIRE(circular_buffer_get(&buffer, 1, &item));
    CHECK(item == third);
    REQUIRE(circular_buffer_get(&buffer, 2, &item));
    CHECK(item == fourth);
    CHECK_FALSE(circular_buffer_get(&buffer, 3, &item));
}

TEST_CASE("circular buffer clear resets size and rejects reads", "[circular-buffer]") {
    std::array<uint8_t, 2> backing{};
    CircularBuffer buffer{};
    circular_buffer_init(&buffer, sizeof(uint8_t), backing.size(), backing.data());

    const uint8_t item = 7;
    circular_buffer_push(&buffer, &item);
    REQUIRE(circular_buffer_size(&buffer) == 1);

    circular_buffer_clear(&buffer);

    uint8_t output = 99;
    CHECK(circular_buffer_size(&buffer) == 0);
    CHECK_FALSE(circular_buffer_get(&buffer, 0, &output));
    CHECK(output == 99);
}

TEST_CASE("C++ smoke tests can link existing C fakes", "[fakes]") {
    led_driver_fake_reset();

    CHECK(led_driver_fake_setup_calls() == 0);
}
