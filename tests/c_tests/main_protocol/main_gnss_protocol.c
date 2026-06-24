#include "conf/buffer.h"
#include "data.h"

bool init(lib_info *info);

enum {
    COMMAND_INFO = 0,
    COMMAND_REALTIME_DATA = 33,
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

typedef struct {
    uint8_t *request;
    size_t request_len;
} MainGnssInvokeGuard;

static bool run_main_gnss_invoke(void *ctx) {
    MainGnssInvokeGuard *guard = ctx;
    vesc_if_fake_invoke_app_data_handler(guard->request, guard->request_len);
    return true;
}

static bool main_gnss_invoke_without_signal(uint8_t *request, size_t request_len) {
    MainGnssInvokeGuard guard = {.request = request, .request_len = request_len};
    return test_expect_no_signal(SIGSEGV, run_main_gnss_invoke, &guard);
}

static bool main_gnss_start(lib_info *info, Data **data) {
    vesc_if_fake_reset();

    EXPECT_TRUE(init(info));
    EXPECT_TRUE(info->arg != NULL);
    EXPECT_TRUE(info->stop_fun != NULL);
    vesc_if_fake_set_arg(info->arg);

    *data = info->arg;
    (*data)->float_conf.hardware.leds.mode = LED_MODE_OFF;
    (*data)->data_record.enabled = false;
    (*data)->time.now = 123456u;
    return true;
}

static uint32_t main_gnss_mask2(void) {
    return RT_MASK2_GNSS_LAT | RT_MASK2_GNSS_LON | RT_MASK2_GNSS_ALTITUDE |
        RT_MASK2_GNSS_SPEED | RT_MASK2_GNSS_ACCURACY | RT_MASK2_GNSS_LAST_UPDATE;
}

static bool check_main_info_gnss_flags(const uint8_t *payload, size_t len, uint32_t flags) {
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 60u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], COMMAND_INFO);
    EXPECT_EQ_U32(payload[2], 2u);
    EXPECT_EQ_U32(payload[3], 0u);
    int32_t index = 55;
    EXPECT_EQ_U32(buffer_get_uint32(payload, &index), flags);
    return true;
}

static void main_gnss_realtime_request(uint8_t request[11], uint8_t precision) {
    int32_t index = 0;
    request[index++] = 101;
    request[index++] = COMMAND_REALTIME_DATA;
    request[index++] = precision;
    buffer_append_uint32(request, 0, &index);
    buffer_append_uint32(request, main_gnss_mask2(), &index);
}

static bool check_main_realtime_gnss_prefix(
    const uint8_t *payload, size_t len, uint8_t precision, uint32_t now, int32_t *index
) {
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], COMMAND_REALTIME_DATA);
    EXPECT_EQ_U32(payload[2], precision);
    *index = 3;
    EXPECT_EQ_U32(buffer_get_uint32(payload, index), 0u);
    EXPECT_EQ_U32(buffer_get_uint32(payload, index), main_gnss_mask2());
    EXPECT_EQ_U32(buffer_get_uint32(payload, index), now);
    EXPECT_TRUE(len >= (size_t) *index);
    return true;
}

static bool check_main_info_optional_gnss(bool missing_hook) {
    uint8_t info_request[] = {101, COMMAND_INFO, 2, 0};

    lib_info info = {0};
    Data *data = NULL;
    EXPECT_TRUE(main_gnss_start(&info, &data));
    (void) data;
    if (missing_hook) {
        vesc_if_fake_set_mc_gnss_missing();
    } else {
        vesc_if_fake_set_mc_gnss_returns_null(true);
    }

    XEXPECT_TRUE(main_gnss_invoke_without_signal(info_request, sizeof(info_request)));
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(check_main_info_gnss_flags(payload, len, 0u));
    info.stop_fun(info.arg);
    return true;
}

static bool check_main_realtime_optional_gnss(bool missing_hook) {
    uint8_t rt_request[11] = {0};
    main_gnss_realtime_request(rt_request, 1);

    lib_info info = {0};
    Data *data = NULL;
    EXPECT_TRUE(main_gnss_start(&info, &data));
    if (missing_hook) {
        vesc_if_fake_set_mc_gnss_missing();
    } else {
        vesc_if_fake_set_mc_gnss_returns_null(true);
    }

    XEXPECT_TRUE(main_gnss_invoke_without_signal(rt_request, sizeof(rt_request)));
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 15u);
    int32_t index = 0;
    EXPECT_TRUE(check_main_realtime_gnss_prefix(payload, len, 1, data->time.now, &index));
    info.stop_fun(info.arg);
    return true;
}

static bool test_main_gnss_protocol(void) {
    lib_info info = {0};
    Data *d = NULL;
    EXPECT_TRUE(main_gnss_start(&info, &d));

    uint8_t info_request[] = {101, COMMAND_INFO, 2, 0};
    size_t len = 0;
    const uint8_t *payload = NULL;

    vesc_if_fake_set_gnss(12.3456789, -98.7654321, 123.4f, 5.0f, 0.89f, 0u);
    EXPECT_TRUE(vesc_if_fake_mc_gnss_calls() == 0);
    vesc_if_fake_invoke_app_data_handler(info_request, sizeof(info_request));

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(check_main_info_gnss_flags(payload, len, 0u));
    EXPECT_TRUE(vesc_if_fake_mc_gnss_calls() == 1);

    vesc_if_fake_set_gnss(12.3456789, -98.7654321, 123.4f, 5.0f, 0.89f, 4321u);
    vesc_if_fake_invoke_app_data_handler(info_request, sizeof(info_request));

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(check_main_info_gnss_flags(payload, len, 0x00000004u));
    EXPECT_TRUE(vesc_if_fake_mc_gnss_calls() == 2);

    uint8_t rt_request_f16[11] = {0};
    main_gnss_realtime_request(rt_request_f16, 0);

    vesc_if_fake_invoke_app_data_handler(rt_request_f16, sizeof(rt_request_f16));

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(len, 41u);
    int32_t index = 0;
    EXPECT_TRUE(check_main_realtime_gnss_prefix(payload, len, 0, d->time.now, &index));
    EXPECT_FLOAT_NEAR(buffer_get_float64_be(payload, &index), 12.3456789);
    EXPECT_FLOAT_NEAR(buffer_get_float64_be(payload, &index), -98.7654321);
    EXPECT_EQ_U32(buffer_get_uint16(payload, &index), to_float16(123.4f));
    EXPECT_EQ_U32(buffer_get_uint16(payload, &index), to_float16(18.0f));
    EXPECT_EQ_U32(buffer_get_uint16(payload, &index), to_float16(0.89f));
    EXPECT_EQ_U32(buffer_get_uint32(payload, &index), 4321u);
    EXPECT_EQ_U32(index, len);
    EXPECT_TRUE(vesc_if_fake_mc_gnss_calls() == 3);

    uint8_t rt_request_f32[11] = {0};
    main_gnss_realtime_request(rt_request_f32, 1);

    vesc_if_fake_invoke_app_data_handler(rt_request_f32, sizeof(rt_request_f32));

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(len, 47u);
    EXPECT_TRUE(check_main_realtime_gnss_prefix(payload, len, 1, d->time.now, &index));
    EXPECT_FLOAT_NEAR(buffer_get_float64_be(payload, &index), 12.3456789);
    EXPECT_FLOAT_NEAR(buffer_get_float64_be(payload, &index), -98.7654321);
    EXPECT_FLOAT_NEAR(buffer_get_float32_auto(payload, &index), 123.4f);
    EXPECT_FLOAT_NEAR(buffer_get_float32_auto(payload, &index), 18.0f);
    EXPECT_FLOAT_NEAR(buffer_get_float32_auto(payload, &index), 0.89f);
    EXPECT_EQ_U32(buffer_get_uint32(payload, &index), 4321u);
    EXPECT_EQ_U32(index, len);
    EXPECT_TRUE(vesc_if_fake_mc_gnss_calls() == 4);

    size_t gnss_calls = vesc_if_fake_mc_gnss_calls();
    uint8_t short_request[] = {101, COMMAND_REALTIME_DATA, 0, 0};
    vesc_if_fake_invoke_app_data_handler(short_request, sizeof(short_request));
    EXPECT_EQ_U32(vesc_if_fake_mc_gnss_calls(), gnss_calls);

    info.stop_fun(info.arg);
    return true;
}

static bool test_main_info_handles_optional_gnss_unavailable(void) {
    EXPECT_TRUE(check_main_info_optional_gnss(true));
    EXPECT_TRUE(check_main_info_optional_gnss(false));
    return true;
}

static bool test_main_realtime_handles_optional_gnss_unavailable(void) {
    EXPECT_TRUE(check_main_realtime_optional_gnss(true));
    EXPECT_TRUE(check_main_realtime_optional_gnss(false));
    return true;
}
