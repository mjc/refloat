#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

extern "C" {
#include "circular_buffer.h"

void led_driver_fake_reset(void);
std::size_t led_driver_fake_setup_calls(void);
}

struct BufferItem {
    uint8_t lo;
    uint8_t hi;
};

static_assert(sizeof(BufferItem) == 2);

static bool operator==(const BufferItem &lhs, const BufferItem &rhs) {
    return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
}

struct CircularBufferFixture {
    std::array<BufferItem, 3> storage{};
    CircularBuffer buffer{};

    CircularBufferFixture() {
        circular_buffer_init(&buffer, sizeof(BufferItem), storage.size(), storage.data());
    }

    void push(BufferItem item) {
        circular_buffer_push(&buffer, &item);
    }

    [[nodiscard]] BufferItem get(size_t index) const {
        BufferItem item{0, 0};
        REQUIRE(circular_buffer_get(&buffer, index, &item));
        return item;
    }
};

struct IterateCapture {
    std::array<BufferItem, 3> items{};
    std::size_t count{};
};

static void collect_item(const void *item, void *data) {
    auto *capture = static_cast<IterateCapture *>(data);
    REQUIRE(capture->count < capture->items.size());
    capture->items[capture->count++] = *static_cast<const BufferItem *>(item);
}

TEST_CASE("circular buffer keeps the newest items after wrapping", "[circular-buffer]") {
    CircularBufferFixture fixture;

    fixture.push({1, 11});
    fixture.push({2, 22});
    fixture.push({3, 33});
    fixture.push({4, 44});

    REQUIRE(circular_buffer_size(&fixture.buffer) == 3);
    CHECK(fixture.get(0) == BufferItem{2, 22});
    CHECK(fixture.get(1) == BufferItem{3, 33});
    CHECK(fixture.get(2) == BufferItem{4, 44});

    BufferItem sentinel{0xaa, 0xbb};
    CHECK_FALSE(circular_buffer_get(&fixture.buffer, 3, &sentinel));
    CHECK(sentinel == BufferItem{0xaa, 0xbb});
}

TEST_CASE("circular buffer iterate and clear preserve sentinel behavior", "[circular-buffer]") {
    CircularBufferFixture fixture;

    fixture.push({1, 11});
    fixture.push({2, 22});
    fixture.push({3, 33});

    IterateCapture capture{};
    circular_buffer_iterate(&fixture.buffer, collect_item, &capture);
    CHECK(capture.count == 3);
    CHECK(capture.items[0] == BufferItem{1, 11});

    circular_buffer_clear(&fixture.buffer);
    CHECK(circular_buffer_size(&fixture.buffer) == 0);

    BufferItem sentinel{0xaa, 0xbb};
    CHECK_FALSE(circular_buffer_get(&fixture.buffer, 0, &sentinel));
    CHECK(sentinel == BufferItem{0xaa, 0xbb});
}

TEST_CASE(
    "circular buffer iterates item-size-scaled entries",
    "[circular-buffer][!shouldfail]"
) {
    CircularBufferFixture fixture;

    fixture.push({1, 11});
    fixture.push({2, 22});
    fixture.push({3, 33});

    IterateCapture capture{};
    circular_buffer_iterate(&fixture.buffer, collect_item, &capture);

    REQUIRE(capture.count == 3);
    CHECK(capture.items[0] == BufferItem{1, 11});
    CHECK(capture.items[1] == BufferItem{2, 22});
    CHECK(capture.items[2] == BufferItem{3, 33});
}

TEST_CASE("circular buffer tail pop removes oldest items in order", "[circular-buffer]") {
    CircularBufferFixture fixture;

    fixture.push({1, 11});
    fixture.push({2, 22});
    fixture.push({3, 33});

    BufferItem popped{0, 0};
    CHECK(circular_buffer_pop(&fixture.buffer, 0, &popped));
    CHECK(popped == BufferItem{1, 11});
    CHECK(circular_buffer_size(&fixture.buffer) == 2);
    CHECK(fixture.get(0) == BufferItem{2, 22});
    CHECK(fixture.get(1) == BufferItem{3, 33});

    CHECK(circular_buffer_pop(&fixture.buffer, 0, &popped));
    CHECK(popped == BufferItem{2, 22});
    CHECK(circular_buffer_pop(&fixture.buffer, 0, &popped));
    CHECK(popped == BufferItem{3, 33});
    CHECK(circular_buffer_size(&fixture.buffer) == 0);
}

TEST_CASE("circular buffer pop removes the requested index", "[circular-buffer][!shouldfail]") {
    CircularBufferFixture fixture;

    fixture.push({1, 11});
    fixture.push({2, 22});
    fixture.push({3, 33});

    BufferItem popped{0, 0};
    CHECK(circular_buffer_pop(&fixture.buffer, 1, &popped));
    CHECK(popped == BufferItem{2, 22});
    CHECK(circular_buffer_size(&fixture.buffer) == 2);
    CHECK(fixture.get(0) == BufferItem{1, 11});
    CHECK(fixture.get(1) == BufferItem{3, 33});
}

TEST_CASE("circular buffer size reflects internal positions", "[circular-buffer]") {
    CircularBufferFixture fixture;

    CHECK(circular_buffer_size(&fixture.buffer) == 0);

    fixture.buffer.head = 2;
    fixture.buffer.tail = 1;
    fixture.buffer.empty = false;
    CHECK(circular_buffer_size(&fixture.buffer) == 1);

    fixture.buffer.head = 0;
    fixture.buffer.tail = 2;
    fixture.buffer.empty = false;
    CHECK(circular_buffer_size(&fixture.buffer) == 1);

    fixture.buffer.head = 0;
    fixture.buffer.tail = 0;
    fixture.buffer.empty = false;
    CHECK(circular_buffer_size(&fixture.buffer) == 3);

    fixture.buffer.head = 0;
    fixture.buffer.tail = 0;
    fixture.buffer.empty = true;
    CHECK(circular_buffer_size(&fixture.buffer) == 0);
}

TEST_CASE("test fakes link into C++ suites", "[fakes]") {
    led_driver_fake_reset();

    CHECK(led_driver_fake_setup_calls() == 0);
}
