#include "c_tests/test_support.h"

#include <float.h>
#include <math.h>

static bool test_buffer_integer_roundtrip_property(void) {
    uint8_t buffer[12] = {0};
    uint32_t seed = 0xb0ffe7u;

    for (size_t i = 0; i < 256; ++i) {
        int16_t signed16 = (int16_t) test_xorshift32(&seed);
        uint16_t unsigned16 = (uint16_t) test_xorshift32(&seed);
        int32_t signed32 = (int32_t) test_xorshift32(&seed);
        uint32_t unsigned32 = test_xorshift32(&seed);
        int32_t index = 0;

        buffer_append_int16(buffer, signed16, &index);
        buffer_append_uint16(buffer, unsigned16, &index);
        buffer_append_int32(buffer, signed32, &index);
        buffer_append_uint32(buffer, unsigned32, &index);
        EXPECT_EQ_SIZE((size_t) index, sizeof(buffer));

        index = 0;
        EXPECT_TRUE(buffer_get_int16(buffer, &index) == signed16);
        EXPECT_EQ_U32(buffer_get_uint16(buffer, &index), unsigned16);
        EXPECT_TRUE(buffer_get_int32(buffer, &index) == signed32);
        EXPECT_EQ_U32(buffer_get_uint32(buffer, &index), unsigned32);
        EXPECT_EQ_SIZE((size_t) index, sizeof(buffer));
    }

    return true;
}

static bool test_buffer_float_encoding_property(void) {
    static const float values[] = {
        0.0f,
        -0.0f,
        0.0001f,
        -0.0001f,
        0.25f,
        -0.25f,
        1.0f,
        -1.0f,
        18.75f,
        -18.75f,
        123.4f,
        -123.4f,
    };
    uint8_t buffer[8] = {0};

    EXPECT_EQ_U32(to_float16(0.0f), 0u);
    EXPECT_EQ_U32(to_float16(-0.0f), 0x8000u);
    EXPECT_TRUE(to_float16(6.0e-8f) != 0u);
    EXPECT_EQ_U32(to_float16(NAN), 0u);
    EXPECT_EQ_U32(to_float16(INFINITY), 0u);
    EXPECT_EQ_U32(to_float16(-INFINITY), 0u);
    EXPECT_EQ_U32(to_float16(FLT_MAX), 0x7fffu);

    int32_t float16_index = 0;
    buffer_append_float16(buffer, NAN, 1.0f, &float16_index);
    EXPECT_EQ_U32(buffer[0], 0u);
    EXPECT_EQ_U32(buffer[1], 0u);

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        EXPECT_FLOAT_NEAR_EPS(from_float16(to_float16(values[i])), values[i], 0.1f);
    }

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        int32_t index = 0;
        buffer_append_float32_auto(buffer, values[i], &index);
        EXPECT_EQ_SIZE((size_t) index, sizeof(uint32_t));
        index = 0;
        EXPECT_FLOAT_NEAR_EPS(buffer_get_float32_auto(buffer, &index), values[i], 0.001f);
        EXPECT_EQ_SIZE((size_t) index, sizeof(uint32_t));
    }

    return true;
}

static bool test_buffer_strings_and_scaled_float32(void) {
    uint8_t buffer[64] = {0};
    int32_t index = 0;

    buffer_append_string(buffer, "abc", &index);
    EXPECT_EQ_U32(buffer[0], 3u);
    EXPECT_TRUE(memcmp(&buffer[1], "abc", 3) == 0);
    EXPECT_EQ_U32(index, 4u);

    index = 0;
    buffer_append_string_max(buffer, "abcdef", &index, 3);
    EXPECT_EQ_U32(buffer[0], 3u);
    EXPECT_TRUE(memcmp(&buffer[1], "abc", 3) == 0);
    EXPECT_EQ_U32(index, 4u);

    index = 0;
    buffer_append_string_max(buffer, "xy", &index, 4);
    EXPECT_EQ_U32(buffer[0], 2u);
    EXPECT_TRUE(memcmp(&buffer[1], "xy", 2) == 0);
    EXPECT_EQ_U32(index, 3u);

    index = 0;
    buffer_append_string_fixed(buffer, "xy", &index, 4);
    EXPECT_TRUE(memcmp(buffer, "xy\0\0", 4) == 0);
    EXPECT_EQ_U32(index, 4u);

    index = 0;
    buffer_append_string_fixed(buffer, "abcd", &index, 4);
    EXPECT_TRUE(memcmp(buffer, "abcd", 4) == 0);
    EXPECT_EQ_U32(index, 4u);

    index = 0;
    buffer_append_float32(buffer, -1.25f, 100.0f, &index);
    EXPECT_EQ_U32(index, sizeof(uint32_t));
    index = 0;
    EXPECT_FLOAT_NEAR(buffer_get_float32(buffer, 100.0f, &index), -1.25f);
    EXPECT_EQ_U32(index, sizeof(uint32_t));

    static const struct {
        float number;
        float scale;
        int32_t expected;
    } boundary_cases[] = {
        {NAN, 1.0f, 0},
        {INFINITY, 1.0f, INT32_MAX},
        {-INFINITY, 1.0f, INT32_MIN},
        {1.0e30f, 1.0f, INT32_MAX},
        {-1.0e30f, 1.0f, INT32_MIN},
        {1.0f, NAN, 0},
    };
    for (size_t i = 0; i < sizeof(boundary_cases) / sizeof(boundary_cases[0]); ++i) {
        index = 0;
        buffer_append_float32(buffer, boundary_cases[i].number, boundary_cases[i].scale, &index);
        EXPECT_EQ_U32(index, sizeof(uint32_t));
        index = 0;
        EXPECT_TRUE(buffer_get_int32(buffer, &index) == boundary_cases[i].expected);
    }

    return true;
}
