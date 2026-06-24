#include "../c_support.hpp"

namespace {

TEST_CASE("sma growth transition edges", "[c][red]") {
    vesc_if_fake_reset();
    vesc_if_fake_fill_next_malloc(0x7f);

    SMA sma;
    sma_init(&sma);
    sma_configure(&sma, 8.5f, 100.0f);
    REQUIRE(sma.array != NULL);
    REQUIRE(sma.n == 5);
    REQUIRE(sma.allocated_n == 6);

    for (uint8_t i = 0; i < sma.n; ++i) {
        sma_update(&sma, 10.0f);
    }
    CHECK_FLOAT_NEAR(sma.value, 10.0f);

    uint8_t old_n = sma.n;
    sma_configure(&sma, 7.0f, 100.0f);
    REQUIRE(sma.n == old_n);
    REQUIRE(sma.new_n == 6);

    for (uint8_t i = 0; i < old_n; ++i) {
        sma_update(&sma, 10.0f);
    }
    REQUIRE(sma.n == 6);
    REQUIRE(sma.new_n == 0);
    REQUIRE(sma.idx == old_n);
    CHECK_FLOAT_NEAR(sma.array[old_n], 10.0f);
    CHECK_FLOAT_NEAR(sma.value, 10.0f);

    sma_update(&sma, 22.0f);
    REQUIRE(sma.idx == 0);
    CHECK_FLOAT_NEAR(sma.value, 12.0f);

    sma_configure(&sma, 6.0f, 100.0f);
    REQUIRE(sma.new_n == 0);
    REQUIRE(sma.n == 6);

    sma_destroy(&sma);
    REQUIRE(vesc_if_fake_free_calls() == 1);
}

static sigjmp_buf sma_allocation_failure_sigsegv_env;

enum {
    TEST_SMA_SIGSEGV = 11
};

static void catch_sma_allocation_failure_sigsegv(int signal_number) {
    unused(signal_number);
    siglongjmp(sma_allocation_failure_sigsegv_env, 1);
}

TEST_CASE("sma allocation failure update", "[c][red]") {
    vesc_if_fake_reset();
    vesc_if_fake_fail_next_malloc();

    SMA sma;
    sma_init(&sma);
    sma_configure(&sma, 1.0f, 100.0f);
    REQUIRE(sma.array == NULL);
    CHECK_U32(sma.n, 0u);

    SignalHandler previous_handler = signal(TEST_SMA_SIGSEGV, catch_sma_allocation_failure_sigsegv);
    if (sigsetjmp(sma_allocation_failure_sigsegv_env, 1) != 0) {
        signal(TEST_SMA_SIGSEGV, previous_handler);
        FAIL("unexpected signal while exercising C code");
    }

    sma_update(&sma, 12.0f);
    signal(TEST_SMA_SIGSEGV, previous_handler);

    CHECK_FLOAT_NEAR(sma.value, 0.0f);
    CHECK_U32(sma.idx, 0u);
    CHECK_U32(vesc_if_fake_malloc_calls(), 1u);
    CHECK_U32(vesc_if_fake_free_calls(), 0u);
}

TEST_CASE("circular buffer pop index", "[c][red]") {
    BufferItem storage[3] = {{}};
    CircularBuffer cb;
    circular_buffer_init(&cb, sizeof(BufferItem), 3, storage);

    BufferItem a = {1, 11};
    BufferItem b = {2, 22};
    BufferItem c = {3, 33};
    BufferItem out = {};

    circular_buffer_push(&cb, &a);
    circular_buffer_push(&cb, &b);
    circular_buffer_push(&cb, &c);

    REQUIRE(circular_buffer_pop(&cb, 1, &out));
    REQUIRE(buffer_item_eq(out, b));
    REQUIRE(circular_buffer_size(&cb) == 2);
    REQUIRE(circular_buffer_get(&cb, 0, &out));
    REQUIRE(buffer_item_eq(out, a));
    REQUIRE(circular_buffer_get(&cb, 1, &out));
    REQUIRE(buffer_item_eq(out, c));
}

TEST_CASE("lcm payload clamp", "[c][red]") {
    LcmData lcm = {};
    lcm.enabled = true;

    unsigned char short_cfg[] = {11, 22, 33, 44, 55};
    lcm_light_ctrl_request(&lcm, short_cfg, (int) sizeof(short_cfg));
    REQUIRE(lcm.brightness == 11);
    REQUIRE(lcm.brightness_idle == 22);
    REQUIRE(lcm.status_brightness == 33);
    REQUIRE(lcm.payload_size == 2);
    REQUIRE(lcm.payload[0] == 44);
    REQUIRE(lcm.payload[1] == 55);

    unsigned char max_cfg[3 + MAX_LCM_PAYLOAD_LENGTH];
    for (size_t i = 0; i < sizeof(max_cfg); ++i) {
        max_cfg[i] = (unsigned char) i;
    }

    lcm_light_ctrl_request(&lcm, max_cfg, (int) sizeof(max_cfg));
    REQUIRE(lcm.payload_size == MAX_LCM_PAYLOAD_LENGTH);
    REQUIRE(lcm.payload[0] == 3);
    REQUIRE(
        lcm.payload[MAX_LCM_PAYLOAD_LENGTH - 1] == (unsigned char) (2 + MAX_LCM_PAYLOAD_LENGTH)
    );

    unsigned char oversized_cfg[3 + MAX_LCM_PAYLOAD_LENGTH + 5];
    for (size_t i = 0; i < sizeof(oversized_cfg); ++i) {
        oversized_cfg[i] = (unsigned char) (200 + i);
    }

    lcm_light_ctrl_request(&lcm, oversized_cfg, (int) sizeof(oversized_cfg));
    REQUIRE(lcm.payload_size == MAX_LCM_PAYLOAD_LENGTH);
    REQUIRE(lcm.payload[0] == 203);
    REQUIRE(
        lcm.payload[MAX_LCM_PAYLOAD_LENGTH - 1] == (unsigned char) (202 + MAX_LCM_PAYLOAD_LENGTH)
    );
}

TEST_CASE("lcm disabled responses are minimal", "[c]") {
    vesc_if_fake_reset();

    LcmData lcm = {};
    lcm.enabled = false;
    lcm.brightness = 12;
    lcm.brightness_idle = 34;
    lcm.status_brightness = 56;
    lcm.payload_size = 2;
    lcm.payload[0] = 77;
    lcm.payload[1] = 88;
    memcpy(lcm.name, "External", 9);

    uint8_t request[] = {'N', 'e', 'w', '\0'};
    lcm_poll_request(&lcm, request, sizeof(request));
    REQUIRE(strcmp(lcm.name, "External") == 0);

    unsigned char ctrl[] = {1, 2, 3, 4, 5, 6};
    lcm_light_ctrl_request(&lcm, ctrl, (int) sizeof(ctrl));
    CHECK_U32(lcm.brightness, 12u);
    CHECK_U32(lcm.payload_size, 2u);

    State state = {.state = STATE_RUNNING};
    MotorData motor = {};
    lcm_poll_response(&lcm, &state, FS_BOTH, &motor, 45.0f);
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(len, 2u);
    CHECK_U32(payload[0], 101u);
    CHECK_U32(payload[1], COMMAND_LCM_POLL);
    CHECK_U32(lcm.payload_size, 2u);

    lcm_light_info_response(&lcm);
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(len, 3u);
    CHECK_U32(payload[1], COMMAND_LCM_LIGHT_INFO);
    CHECK_U32(payload[2], 0u);

    lcm_device_info_response(&lcm);
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(len, 2u);
    CHECK_U32(payload[1], COMMAND_LCM_DEVICE_INFO);

    lcm_get_battery_response(&lcm);
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(len, 2u);
    CHECK_U32(payload[1], COMMAND_LCM_GET_BATTERY);
}

TEST_CASE("lcm init configure and runtime brightness", "[c]") {
    CfgHwLeds hw = {};
    LcmData lcm = {};

    hw.mode = LED_MODE_INTERNAL;
    lcm_init(&lcm, &hw);
    REQUIRE(!lcm.enabled);
    CHECK_U32(lcm.brightness, 0u);
    CHECK_U32(lcm.brightness_idle, 0u);
    CHECK_U32(lcm.status_brightness, 0u);
    REQUIRE(lcm.lights_off_when_lifted);
    REQUIRE(lcm.name[0] == '\0');

    hw.mode = LED_MODE_EXTERNAL;
    lcm_init(&lcm, &hw);
    REQUIRE(lcm.enabled);

    CfgLeds cfg = {};
    cfg.headlights.brightness = 0.82f;
    cfg.front.brightness = 0.37f;
    cfg.status.brightness_headlights_on = 0.41f;
    cfg.status.brightness_headlights_off = 0.19f;
    cfg.lights_off_when_lifted = false;

    Leds leds;
    memset(&leds, 0, sizeof(leds));
    leds.cfg = &cfg;
    lcm_fakes_set_runtime_status(true, true);
    lcm_configure(&lcm, &leds);
    CHECK_U32(lcm.brightness, 82u);
    CHECK_U32(lcm.brightness_idle, 37u);
    CHECK_U32(lcm.status_brightness, 41u);
    REQUIRE(!lcm.lights_off_when_lifted);

    lcm_fakes_set_runtime_status(true, false);
    lcm_configure(&lcm, &leds);
    CHECK_U32(lcm.brightness, 37u);
    CHECK_U32(lcm.brightness_idle, 37u);
    CHECK_U32(lcm.status_brightness, 19u);

    lcm_fakes_set_runtime_status(false, false);
    lcm_configure(&lcm, &leds);
    CHECK_U32(lcm.brightness, 0u);
    CHECK_U32(lcm.brightness_idle, 0u);
    CHECK_U32(lcm.status_brightness, 0u);

    lcm.enabled = false;
    lcm.brightness = 99;
    lcm_fakes_set_runtime_status(true, true);
    lcm_configure(&lcm, &leds);
    CHECK_U32(lcm.brightness, 99u);
}

static sigjmp_buf lcm_configure_sigsegv_env;

enum {
    TEST_SIGSEGV = 11
};

static void catch_lcm_configure_sigsegv(int signal_number) {
    unused(signal_number);
    siglongjmp(lcm_configure_sigsegv_env, 1);
}

TEST_CASE("lcm configure requires initialized led config", "[c][red]") {
    SignalHandler previous_handler = signal(TEST_SIGSEGV, catch_lcm_configure_sigsegv);

    if (sigsetjmp(lcm_configure_sigsegv_env, 1) != 0) {
        signal(TEST_SIGSEGV, previous_handler);
        FAIL("unexpected signal while exercising C code");
    }

    LcmData lcm = {};
    lcm.enabled = true;
    Leds leds = {};
    lcm_fakes_set_runtime_status(true, false);

    lcm_configure(&lcm, &leds);
    signal(TEST_SIGSEGV, previous_handler);

    CHECK_U32(lcm.brightness, 0u);
    CHECK_U32(lcm.brightness_idle, 0u);
    CHECK_U32(lcm.status_brightness, 0u);
    REQUIRE(lcm.lights_off_when_lifted);
}

TEST_CASE("lcm poll response pitch payload and name edges", "[c]") {
    vesc_if_fake_reset();
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 2.0f, 49.5f, 0, 0);

    LcmData lcm = {};
    lcm.enabled = true;
    lcm.brightness = 11;
    lcm.brightness_idle = 22;
    lcm.status_brightness = 33;
    lcm.lights_off_when_lifted = true;

    uint8_t empty_name[] = {};
    memcpy(lcm.name, "Previous", 9);
    lcm_poll_request(&lcm, empty_name, 0);
    REQUIRE(strcmp(lcm.name, "Previous") == 0);

    uint8_t long_name[MAX_LCM_NAME_LENGTH + 4];
    memset(long_name, 'A', sizeof(long_name));
    long_name[MAX_LCM_NAME_LENGTH - 1] = '\0';
    lcm_poll_request(&lcm, long_name, sizeof(long_name));
    REQUIRE(strlen(lcm.name) == MAX_LCM_NAME_LENGTH - 1);

    unsigned char ctrl[] = {44, 55, 66, 77, 88, 99};
    lcm_light_ctrl_request(&lcm, ctrl, (int) sizeof(ctrl));
    CHECK_U32(lcm.brightness, 44u);
    CHECK_U32(lcm.payload_size, 3u);

    State state = {.state = STATE_READY, .mode = MODE_NORMAL};
    MotorData motor = {};
    motor.erpm = 123.0f;
    lcm_poll_response(&lcm, &state, FS_RIGHT, &motor, -27.4f);

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[1], COMMAND_LCM_POLL);
    CHECK_U32(payload[4], 27u);
    CHECK_U32(payload[11], 44u);
    CHECK_U32(payload[12], 55u);
    CHECK_U32(payload[13], 66u);
    CHECK_U32(payload[14], 77u);
    CHECK_U32(payload[15], 88u);
    CHECK_U32(payload[16], 99u);
    CHECK_U32(lcm.payload_size, 0u);

    lcm.lights_off_when_lifted = false;
    lcm_poll_response(&lcm, &state, FS_NONE, &motor, 62.0f);
    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[4], 0u);
    CHECK_U32(len, 14u);
}

TEST_CASE("lcm poll request respects name length", "[c][red]") {
    LcmData lcm = {};
    lcm.enabled = true;
    memset(lcm.name, 0, sizeof(lcm.name));

    uint8_t request[] = {'A', 'B', 'C', 'Z'};
    lcm_poll_request(&lcm, request, 3);

    REQUIRE(strcmp(lcm.name, "ABC") == 0);
}

TEST_CASE("lcm poll response saturates byte fields", "[c][red]") {
    vesc_if_fake_reset();

    LcmData lcm = {};
    lcm.enabled = true;
    lcm.lights_off_when_lifted = true;

    State state = {.state = STATE_READY, .mode = MODE_NORMAL};
    MotorData motor = {};

    lcm_poll_response(&lcm, &state, FS_NONE, &motor, 300.0f);

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(len, 14u);
    CHECK_U32(payload[1], COMMAND_LCM_POLL);
    CHECK_U32(payload[4], UINT8_MAX);

    state.state = STATE_RUNNING;
    motor.duty_cycle.value = 3.0f;
    lcm_poll_response(&lcm, &state, FS_NONE, &motor, 0.0f);

    payload = vesc_if_fake_last_app_data(&len);
    REQUIRE(payload != NULL);
    CHECK_U32(payload[4], 100u);
}

TEST_CASE("lcm battery response nonfinite values are stable", "[c][red]") {
    const float values[] = {NAN, INFINITY, -INFINITY};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        vesc_if_fake_reset();
        vesc_if_fake_set_battery_level(values[i]);

        LcmData lcm = {};
        lcm.enabled = true;

        lcm_get_battery_response(&lcm);

        size_t len = 0;
        const uint8_t *payload = vesc_if_fake_last_app_data(&len);
        REQUIRE(payload != NULL);
        CHECK_U32(len, 6u);
        CHECK_U32(payload[0], 101u);
        CHECK_U32(payload[1], COMMAND_LCM_GET_BATTERY);

        int32_t index = 2;
        CHECK_U32(buffer_get_uint32(payload, &index), 0u);
        CHECK_U32(index, 6u);
    }
}

}  // namespace
