#include <cstddef>
#include <cstdint>
#include <cmath>

#include "main_protocol_bridge.hpp"

namespace {

enum {
    MAIN_COMMAND_INFO = 0,
    MAIN_COMMAND_REALTIME_DATA = 33,
    RT_MASK2_GNSS_LAT = 1u << 9,
    RT_MASK2_GNSS_LON = 1u << 10,
    RT_MASK2_GNSS_ALTITUDE = 1u << 11,
    RT_MASK2_GNSS_SPEED = 1u << 12,
    RT_MASK2_GNSS_ACCURACY = 1u << 13,
    RT_MASK2_GNSS_LAST_UPDATE = 1u << 14,
};

static double buffer_get_float64_be(const uint8_t *buffer, int32_t *index) {
    union {
        uint64_t u;
        double d;
    } un = {0};

    un.u = ((uint64_t) buffer[*index + 0] << 56) | ((uint64_t) buffer[*index + 1] << 48) |
        ((uint64_t) buffer[*index + 2] << 40) | ((uint64_t) buffer[*index + 3] << 32) |
        ((uint64_t) buffer[*index + 4] << 24) | ((uint64_t) buffer[*index + 5] << 16) |
        ((uint64_t) buffer[*index + 6] << 8) | ((uint64_t) buffer[*index + 7]);
    *index += 8;
    return un.d;
}

}  // namespace

static constexpr double kFloatEps = 1e-6;

#define CHECK_FLOAT_CLOSE(actual, expected) CHECK(std::fabs((actual) - (expected)) < kFloatEps)

TEST_CASE("main gnss protocol", "[main][gnss]") {
    Data *data = nullptr;
    REQUIRE(main_protocol_start(&data));

    data->float_conf.hardware.leds.mode = LED_MODE_OFF;
    data->data_record.enabled = false;
    data->time.now = 123456u;

    uint8_t info_request[] = {101, MAIN_COMMAND_INFO, 2, 0};
    size_t len = 0;
    const uint8_t *payload = nullptr;

    vesc_if_fake_set_gnss(12.3456789, -98.7654321, 123.4f, 5.0f, 0.89f, 0u);
    CHECK(vesc_if_fake_mc_gnss_calls() == 0);
    vesc_if_fake_invoke_app_data_handler(info_request, sizeof(info_request));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 60u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_INFO);
    CHECK(payload[2] == 2u);
    CHECK(payload[3] == 0u);
    int32_t index = 55;
    CHECK(buffer_get_uint32(payload, &index) == 0u);
    CHECK(vesc_if_fake_mc_gnss_calls() == 1);

    vesc_if_fake_set_gnss(12.3456789, -98.7654321, 123.4f, 5.0f, 0.89f, 4321u);
    vesc_if_fake_invoke_app_data_handler(info_request, sizeof(info_request));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 60u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_INFO);
    CHECK(payload[2] == 2u);
    CHECK(payload[3] == 0u);
    index = 55;
    CHECK(buffer_get_uint32(payload, &index) == 0x00000004u);
    CHECK(vesc_if_fake_mc_gnss_calls() == 2);

    const uint32_t mask2 = RT_MASK2_GNSS_LAT | RT_MASK2_GNSS_LON |
        RT_MASK2_GNSS_ALTITUDE | RT_MASK2_GNSS_SPEED | RT_MASK2_GNSS_ACCURACY |
        RT_MASK2_GNSS_LAST_UPDATE;

    uint8_t rt_request_f16[11] = {0};
    index = 0;
    rt_request_f16[index++] = 101;
    rt_request_f16[index++] = MAIN_COMMAND_REALTIME_DATA;
    rt_request_f16[index++] = 0;
    buffer_append_uint32(rt_request_f16, 0, &index);
    buffer_append_uint32(rt_request_f16, mask2, &index);

    vesc_if_fake_invoke_app_data_handler(rt_request_f16, sizeof(rt_request_f16));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 41u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_REALTIME_DATA);
    CHECK(payload[2] == 0u);

    index = 3;
    CHECK(buffer_get_uint32(payload, &index) == 0u);
    CHECK(buffer_get_uint32(payload, &index) == mask2);
    CHECK(buffer_get_uint32(payload, &index) == data->time.now);
    CHECK_FLOAT_CLOSE(buffer_get_float64_be(payload, &index), 12.3456789);
    CHECK_FLOAT_CLOSE(buffer_get_float64_be(payload, &index), -98.7654321);
    CHECK(buffer_get_uint16(payload, &index) == to_float16(123.4f));
    CHECK(buffer_get_uint16(payload, &index) == to_float16(18.0f));
    CHECK(buffer_get_uint16(payload, &index) == to_float16(0.89f));
    CHECK(buffer_get_uint32(payload, &index) == 4321u);
    CHECK(index == static_cast<int32_t>(len));
    CHECK(vesc_if_fake_mc_gnss_calls() == 3);

    uint8_t rt_request_f32[11] = {0};
    index = 0;
    rt_request_f32[index++] = 101;
    rt_request_f32[index++] = MAIN_COMMAND_REALTIME_DATA;
    rt_request_f32[index++] = 1;
    buffer_append_uint32(rt_request_f32, 0, &index);
    buffer_append_uint32(rt_request_f32, mask2, &index);

    vesc_if_fake_invoke_app_data_handler(rt_request_f32, sizeof(rt_request_f32));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 47u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_REALTIME_DATA);
    CHECK(payload[2] == 1u);

    index = 3;
    CHECK(buffer_get_uint32(payload, &index) == 0u);
    CHECK(buffer_get_uint32(payload, &index) == mask2);
    CHECK(buffer_get_uint32(payload, &index) == data->time.now);
    CHECK_FLOAT_CLOSE(buffer_get_float64_be(payload, &index), 12.3456789);
    CHECK_FLOAT_CLOSE(buffer_get_float64_be(payload, &index), -98.7654321);
    CHECK_FLOAT_CLOSE(buffer_get_float32_auto(payload, &index), 123.4f);
    CHECK_FLOAT_CLOSE(buffer_get_float32_auto(payload, &index), 18.0f);
    CHECK_FLOAT_CLOSE(buffer_get_float32_auto(payload, &index), 0.89f);
    CHECK(buffer_get_uint32(payload, &index) == 4321u);
    CHECK(index == static_cast<int32_t>(len));
    CHECK(vesc_if_fake_mc_gnss_calls() == 4);

    size_t gnss_calls = vesc_if_fake_mc_gnss_calls();
    uint8_t short_request[] = {101, MAIN_COMMAND_REALTIME_DATA, 0, 0};
    vesc_if_fake_invoke_app_data_handler(short_request, sizeof(short_request));
    CHECK(vesc_if_fake_mc_gnss_calls() == gnss_calls);
}
