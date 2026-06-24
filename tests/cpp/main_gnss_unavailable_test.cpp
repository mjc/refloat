#include <cstddef>
#include <cstdint>

#include "main_protocol_bridge.hpp"
#include "signal_guard.hpp"

namespace {

constexpr int kMainCommandSigsegv = 11;
constexpr int kMainCommandInfo = 0;
constexpr int kMainCommandRealtimeData = 33;
constexpr uint32_t kRtMask2GnssLat = 1u << 9;
constexpr uint32_t kRtMask2GnssLon = 1u << 10;
constexpr uint32_t kRtMask2GnssAltitude = 1u << 11;
constexpr uint32_t kRtMask2GnssSpeed = 1u << 12;
constexpr uint32_t kRtMask2GnssAccuracy = 1u << 13;
constexpr uint32_t kRtMask2GnssLastUpdate = 1u << 14;

static sigjmp_buf main_gnss_optional_env;

static void catch_main_gnss_optional_signal(int signal_number) {
    (void) signal_number;
    siglongjmp(main_gnss_optional_env, 1);
}

static bool main_gnss_invoke_without_signal(uint8_t *request, size_t request_len) {
    return run_without_signal(
        kMainCommandSigsegv,
        &main_gnss_optional_env,
        catch_main_gnss_optional_signal,
        [&]() { vesc_if_fake_invoke_app_data_handler(request, request_len); }
    );
}

static void prepare_main_protocol(Data **data) {
    REQUIRE(main_protocol_start(data));
    (*data)->float_conf.hardware.leds.mode = LED_MODE_OFF;
    (*data)->data_record.enabled = false;
    (*data)->time.now = 123456u;
}

}  // namespace

#if REFLOAT_GNSS_UNAVAILABLE_SCENARIO == 1

TEST_CASE("main info handles optional gnss unavailable", "[main][gnss][xfail]") {
    Data *data = nullptr;
    prepare_main_protocol(&data);

    uint8_t info_request[] = {101, static_cast<uint8_t>(kMainCommandInfo), 2, 0};
    size_t len = 0;
    const uint8_t *payload = nullptr;

    vesc_if_fake_set_mc_gnss_missing();
    CHECK(main_gnss_invoke_without_signal(info_request, sizeof(info_request)));

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 60u);
    int32_t index = 55;
    CHECK(buffer_get_uint32(payload, &index) == 0u);
}

#elif REFLOAT_GNSS_UNAVAILABLE_SCENARIO == 2

TEST_CASE("main realtime handles optional gnss unavailable", "[main][gnss][xfail]") {
    Data *data = nullptr;
    prepare_main_protocol(&data);

    const uint32_t mask2 = kRtMask2GnssLat | kRtMask2GnssLon | kRtMask2GnssAltitude |
        kRtMask2GnssSpeed | kRtMask2GnssAccuracy | kRtMask2GnssLastUpdate;

    uint8_t rt_request[11] = {101, static_cast<uint8_t>(kMainCommandRealtimeData), 1};
    int32_t index = 3;
    buffer_append_uint32(rt_request, 0u, &index);
    buffer_append_uint32(rt_request, mask2, &index);

    vesc_if_fake_set_mc_gnss_returns_null(true);
    CHECK(main_gnss_invoke_without_signal(rt_request, sizeof(rt_request)));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != nullptr);
    CHECK(len == 15u);
    index = 3;
    CHECK(buffer_get_uint32(payload, &index) == 0u);
    CHECK(buffer_get_uint32(payload, &index) == mask2);
    CHECK(buffer_get_uint32(payload, &index) == data->time.now);
}

#else

#error "REFLOAT_GNSS_UNAVAILABLE_SCENARIO must be 1 or 2"

#endif
