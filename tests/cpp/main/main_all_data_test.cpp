#include <cstddef>
#include <cstdint>

#include "main_protocol_bridge.hpp"

namespace {

enum {
    MAIN_COMMAND_GET_ALLDATA = 10,
};

}  // namespace

TEST_CASE("main all data saturates oversized float16 fields", "[main][protocol]") {
    Data *data = nullptr;
    REQUIRE(main_protocol_start(&data));

    data->balance_current.value = 4000.0f;

    uint8_t request[] = {101, MAIN_COMMAND_GET_ALLDATA, 2};
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(payload[0] == 101u);
    CHECK(payload[1] == MAIN_COMMAND_GET_ALLDATA);
    CHECK(payload[2] == 2u);

    int32_t index = 3;
    CHECK(buffer_get_uint16(payload, &index) == 0x7fffu);
}

