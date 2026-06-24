#include <cstddef>
#include <cstdint>

#include "main_protocol_bridge.hpp"

namespace {

enum {
    MAIN_COMMAND_INFO = 0,
    MAIN_COMMAND_LIGHTS_CONTROL = 20,
    MAIN_COMMAND_ALERTS_LIST = 35,
    MAIN_COMMAND_ALERTS_CONTROL = 36,
};

static bool main_protocol_invoke(const uint8_t *packet, size_t len) {
    vesc_if_fake_invoke_app_data_handler(const_cast<uint8_t *>(packet), len);
    return true;
}

}  // namespace

TEST_CASE("main lights control protocol", "[main][protocol]") {
    Data *data = nullptr;
    REQUIRE(main_protocol_start(&data));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    CHECK(len == 0u);

    uint8_t set_lights[7] = {101, MAIN_COMMAND_LIGHTS_CONTROL};
    int32_t index = 2;
    buffer_append_uint32(set_lights, 0x00000003u, &index);
    set_lights[index++] = 0x01u;
    CHECK(index == static_cast<int32_t>(sizeof(set_lights)));
    REQUIRE(main_protocol_invoke(set_lights, sizeof(set_lights)));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 3u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_LIGHTS_CONTROL);
    CHECK(payload[2] == 0x01u);

    index = 2;
    buffer_append_uint32(set_lights, 0x00000002u, &index);
    set_lights[index++] = 0x02u;
    REQUIRE(main_protocol_invoke(set_lights, sizeof(set_lights)));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 3u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_LIGHTS_CONTROL);
    CHECK(payload[2] == 0x03u);
}

TEST_CASE("main alerts protocol", "[main][protocol]") {
    Data *data = nullptr;
    REQUIRE(main_protocol_start(&data));

    data->time.now = 1234u;
    alert_tracker_add(&data->alert_tracker, &data->time, ALERT_FW_FAULT, FAULT_CODE_NONE);
    alert_tracker_finalize(&data->alert_tracker, &data->time);
    CHECK(data->alert_tracker.fatal_error);

    uint8_t list_request[6] = {101, MAIN_COMMAND_ALERTS_LIST};
    int32_t index = 2;
    buffer_append_uint32(list_request, 0u, &index);
    CHECK(index == static_cast<int32_t>(sizeof(list_request)));
    REQUIRE(main_protocol_invoke(list_request, sizeof(list_request)));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 19u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_ALERTS_LIST);

    index = 2;
    CHECK(buffer_get_uint32(payload, &index) == 0x00000001u);
    CHECK(buffer_get_uint32(payload, &index) == 0x00000000u);
    CHECK(payload[index++] == FAULT_CODE_NONE);
    CHECK(payload[index++] == 1u);
    CHECK(buffer_get_uint32(payload, &index) == 1234u);
    CHECK(payload[index++] == ALERT_FW_FAULT);
    CHECK(payload[index++] == 1u);
    CHECK(payload[index++] == FAULT_CODE_NONE);
    CHECK(index == static_cast<int32_t>(len));

    uint8_t filtered_request[6] = {101, MAIN_COMMAND_ALERTS_LIST};
    index = 2;
    buffer_append_uint32(filtered_request, 1234u, &index);
    REQUIRE(main_protocol_invoke(filtered_request, sizeof(filtered_request)));
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 12u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_ALERTS_LIST);
    CHECK(payload[11] == 0u);

    uint8_t clear_request[] = {101, MAIN_COMMAND_ALERTS_CONTROL, 1};
    REQUIRE(main_protocol_invoke(clear_request, sizeof(clear_request)));
    CHECK(!data->alert_tracker.fatal_error);
}

TEST_CASE("main invalid command protocol", "[main][protocol]") {
    Data *data = nullptr;
    REQUIRE(main_protocol_start(&data));

    uint8_t info_request[] = {101, MAIN_COMMAND_INFO, 2, 0};
    REQUIRE(main_protocol_invoke(info_request, sizeof(info_request)));
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 60u);

    uint8_t bad_magic[] = {100, MAIN_COMMAND_INFO, 2, 0};
    REQUIRE(main_protocol_invoke(bad_magic, sizeof(bad_magic)));
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 60u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_INFO);

    uint8_t unknown_command[] = {101, 250};
    REQUIRE(main_protocol_invoke(unknown_command, sizeof(unknown_command)));
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 60u);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_INFO);
}
