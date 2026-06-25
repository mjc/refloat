typedef struct {
    LcmData lcm;
    CfgLeds cfg;
    Leds leds;
    State state;
    MotorData motor;
} LcmFixture;

static LcmFixture lcm_fixture_start(void) {
    vesc_if_fake_reset();

    LcmFixture fixture = {0};
    fixture.lcm.enabled = true;
    fixture.state.state = STATE_RUNNING;
    fixture.leds.cfg = &fixture.cfg;
    return fixture;
}

static bool test_lcm_light_ctrl_payload_clamps(void) {
    LcmFixture fixture = lcm_fixture_start();

    unsigned char short_cfg[] = {11, 22, 33, 44, 55};
    lcm_light_ctrl_request(&fixture.lcm, short_cfg, (int) sizeof(short_cfg));
    EXPECT_TRUE(fixture.lcm.brightness == 11);
    EXPECT_TRUE(fixture.lcm.brightness_idle == 22);
    EXPECT_TRUE(fixture.lcm.status_brightness == 33);
    EXPECT_TRUE(fixture.lcm.payload_size == 2);
    EXPECT_TRUE(fixture.lcm.payload[0] == 44);
    EXPECT_TRUE(fixture.lcm.payload[1] == 55);

    unsigned char max_cfg[3 + MAX_LCM_PAYLOAD_LENGTH];
    for (size_t i = 0; i < sizeof(max_cfg); ++i) {
        max_cfg[i] = (unsigned char) i;
    }

    lcm_light_ctrl_request(&fixture.lcm, max_cfg, (int) sizeof(max_cfg));
    EXPECT_TRUE(fixture.lcm.payload_size == MAX_LCM_PAYLOAD_LENGTH);
    EXPECT_TRUE(fixture.lcm.payload[0] == 3);
    EXPECT_TRUE(
        fixture.lcm.payload[MAX_LCM_PAYLOAD_LENGTH - 1] == (unsigned char) (2 + MAX_LCM_PAYLOAD_LENGTH)
    );

    unsigned char oversized_cfg[3 + MAX_LCM_PAYLOAD_LENGTH + 5];
    for (size_t i = 0; i < sizeof(oversized_cfg); ++i) {
        oversized_cfg[i] = (unsigned char) (200 + i);
    }

    lcm_light_ctrl_request(&fixture.lcm, oversized_cfg, (int) sizeof(oversized_cfg));
    EXPECT_TRUE(fixture.lcm.payload_size == MAX_LCM_PAYLOAD_LENGTH);
    EXPECT_TRUE(fixture.lcm.payload[0] == 203);
    EXPECT_TRUE(
        fixture.lcm.payload[MAX_LCM_PAYLOAD_LENGTH - 1] == (unsigned char) (202 + MAX_LCM_PAYLOAD_LENGTH)
    );

    return true;
}

static bool test_lcm_disabled_responses_are_minimal(void) {
    LcmFixture fixture = lcm_fixture_start();
    fixture.lcm.enabled = false;
    fixture.lcm.brightness = 12;
    fixture.lcm.brightness_idle = 34;
    fixture.lcm.status_brightness = 56;
    fixture.lcm.payload_size = 2;
    fixture.lcm.payload[0] = 77;
    fixture.lcm.payload[1] = 88;
    memcpy(fixture.lcm.name, "External", 9);

    uint8_t request[] = {'N', 'e', 'w', '\0'};
    lcm_poll_request(&fixture.lcm, request, sizeof(request));
    EXPECT_TRUE(strcmp(fixture.lcm.name, "External") == 0);

    unsigned char ctrl[] = {1, 2, 3, 4, 5, 6};
    lcm_light_ctrl_request(&fixture.lcm, ctrl, (int) sizeof(ctrl));
    EXPECT_EQ_U32(fixture.lcm.brightness, 12u);
    EXPECT_EQ_U32(fixture.lcm.payload_size, 2u);

    lcm_poll_response(&fixture.lcm, &fixture.state, FS_BOTH, &fixture.motor, 45.0f);
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 2u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_POLL);
    EXPECT_EQ_U32(fixture.lcm.payload_size, 2u);

    lcm_light_info_response(&fixture.lcm);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 3u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_LIGHT_INFO);
    EXPECT_EQ_U32(payload[2], 0u);

    lcm_device_info_response(&fixture.lcm);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 2u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_DEVICE_INFO);

    lcm_get_battery_response(&fixture.lcm);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 2u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_GET_BATTERY);

    return true;
}

static bool test_lcm_init_configure_and_runtime_brightness(void) {
    CfgHwLeds hw = {0};
    LcmData lcm = {0};

    hw.mode = LED_MODE_INTERNAL;
    lcm_init(&lcm, &hw);
    EXPECT_TRUE(!lcm.enabled);
    EXPECT_EQ_U32(lcm.brightness, 0u);
    EXPECT_EQ_U32(lcm.brightness_idle, 0u);
    EXPECT_EQ_U32(lcm.status_brightness, 0u);
    EXPECT_TRUE(lcm.lights_off_when_lifted);
    EXPECT_TRUE(lcm.name[0] == '\0');

    hw.mode = LED_MODE_EXTERNAL;
    lcm_init(&lcm, &hw);
    EXPECT_TRUE(lcm.enabled);

    LcmFixture fixture = lcm_fixture_start();
    fixture.cfg.headlights.brightness = 0.82f;
    fixture.cfg.front.brightness = 0.37f;
    fixture.cfg.status.brightness_headlights_on = 0.41f;
    fixture.cfg.status.brightness_headlights_off = 0.19f;
    fixture.cfg.lights_off_when_lifted = false;
    fixture.leds.cfg = &fixture.cfg;
    lcm_fakes_set_runtime_status(true, true);
    lcm_configure(&lcm, &fixture.leds);
    EXPECT_EQ_U32(lcm.brightness, 82u);
    EXPECT_EQ_U32(lcm.brightness_idle, 37u);
    EXPECT_EQ_U32(lcm.status_brightness, 41u);
    EXPECT_TRUE(!lcm.lights_off_when_lifted);

    lcm_fakes_set_runtime_status(true, false);
    lcm_configure(&lcm, &fixture.leds);
    EXPECT_EQ_U32(lcm.brightness, 37u);
    EXPECT_EQ_U32(lcm.brightness_idle, 37u);
    EXPECT_EQ_U32(lcm.status_brightness, 19u);

    lcm_fakes_set_runtime_status(false, false);
    lcm_configure(&lcm, &fixture.leds);
    EXPECT_EQ_U32(lcm.brightness, 0u);
    EXPECT_EQ_U32(lcm.brightness_idle, 0u);
    EXPECT_EQ_U32(lcm.status_brightness, 0u);

    lcm.enabled = false;
    lcm.brightness = 99;
    lcm_fakes_set_runtime_status(true, true);
    lcm_configure(&lcm, &fixture.leds);
    EXPECT_EQ_U32(lcm.brightness, 99u);

    return true;
}

typedef struct {
    LcmData *lcm;
    Leds *leds;
} LcmConfigureGuard;

static bool run_lcm_configure(void *ctx) {
    LcmConfigureGuard *guard = ctx;
    lcm_configure(guard->lcm, guard->leds);
    return true;
}

static bool test_lcm_configure_requires_initialized_led_config(void) {
    LcmFixture fixture = lcm_fixture_start();
    fixture.lcm.enabled = true;
    lcm_fakes_set_runtime_status(true, false);

    LcmConfigureGuard guard = {.lcm = &fixture.lcm, .leds = &fixture.leds};
    EXPECT_TRUE(test_expect_no_signal(SIGSEGV, run_lcm_configure, &guard));

    EXPECT_EQ_U32(fixture.lcm.brightness, 0u);
    EXPECT_EQ_U32(fixture.lcm.brightness_idle, 0u);
    EXPECT_EQ_U32(fixture.lcm.status_brightness, 0u);
    EXPECT_TRUE(fixture.lcm.lights_off_when_lifted);

    return true;
}

static bool test_lcm_poll_response_pitch_payload_and_name_edges(void) {
    LcmFixture fixture = lcm_fixture_start();
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 2.0f, 49.5f, 0, 0);
    fixture.lcm.brightness = 11;
    fixture.lcm.brightness_idle = 22;
    fixture.lcm.status_brightness = 33;
    fixture.lcm.lights_off_when_lifted = true;

    uint8_t empty_name[] = {0};
    memcpy(fixture.lcm.name, "Previous", 9);
    lcm_poll_request(&fixture.lcm, empty_name, 0);
    EXPECT_TRUE(strcmp(fixture.lcm.name, "Previous") == 0);

    uint8_t long_name[MAX_LCM_NAME_LENGTH + 4];
    memset(long_name, 'A', sizeof(long_name));
    long_name[MAX_LCM_NAME_LENGTH - 1] = '\0';
    lcm_poll_request(&fixture.lcm, long_name, sizeof(long_name));
    EXPECT_TRUE(strlen(fixture.lcm.name) == MAX_LCM_NAME_LENGTH - 1);

    unsigned char ctrl[] = {44, 55, 66, 77, 88, 99};
    lcm_light_ctrl_request(&fixture.lcm, ctrl, (int) sizeof(ctrl));
    EXPECT_EQ_U32(fixture.lcm.brightness, 44u);
    EXPECT_EQ_U32(fixture.lcm.payload_size, 3u);

    fixture.state = (State) {.state = STATE_READY, .mode = MODE_NORMAL};
    fixture.motor.erpm = 123.0f;
    lcm_poll_response(&fixture.lcm, &fixture.state, FS_RIGHT, &fixture.motor, -27.4f);

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_POLL);
    EXPECT_EQ_U32(payload[4], 27u);
    EXPECT_EQ_U32(payload[11], 44u);
    EXPECT_EQ_U32(payload[12], 55u);
    EXPECT_EQ_U32(payload[13], 66u);
    EXPECT_EQ_U32(payload[14], 77u);
    EXPECT_EQ_U32(payload[15], 88u);
    EXPECT_EQ_U32(payload[16], 99u);
    EXPECT_EQ_U32(fixture.lcm.payload_size, 0u);

    fixture.lcm.lights_off_when_lifted = false;
    lcm_poll_response(&fixture.lcm, &fixture.state, FS_NONE, &fixture.motor, 62.0f);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[4], 0u);
    EXPECT_EQ_U32(len, 14u);

    return true;
}

static bool test_lcm_poll_request_name_length_bound(void) {
    LcmFixture fixture = lcm_fixture_start();
    memset(fixture.lcm.name, 0, sizeof(fixture.lcm.name));

    uint8_t request[] = {'A', 'B', 'C', 'Z'};
    lcm_poll_request(&fixture.lcm, request, 3);

    EXPECT_TRUE(strcmp(fixture.lcm.name, "ABC") == 0);

    return true;
}

static bool test_lcm_poll_response_saturates_byte_fields(void) {
    LcmFixture fixture = lcm_fixture_start();
    fixture.lcm.lights_off_when_lifted = true;

    fixture.state = (State) {.state = STATE_READY, .mode = MODE_NORMAL};
    lcm_poll_response(&fixture.lcm, &fixture.state, FS_NONE, &fixture.motor, 300.0f);

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 14u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_POLL);
    EXPECT_EQ_U32(payload[4], UINT8_MAX);

    fixture.state.state = STATE_RUNNING;
    fixture.motor.duty_cycle.value = 3.0f;
    lcm_poll_response(&fixture.lcm, &fixture.state, FS_NONE, &fixture.motor, 0.0f);

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[4], 100u);

    return true;
}

static bool test_lcm_battery_response_nonfinite_values_are_stable(void) {
    const float values[] = {NAN, INFINITY, -INFINITY};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        LcmFixture fixture = lcm_fixture_start();
        vesc_if_fake_set_battery_level(values[i]);

        lcm_get_battery_response(&fixture.lcm);

        size_t len = 0;
        const uint8_t *payload = vesc_if_fake_last_app_data(&len);
        EXPECT_TRUE(payload != NULL);
        EXPECT_EQ_U32(len, 6u);
        EXPECT_EQ_U32(payload[0], 101u);
        EXPECT_EQ_U32(payload[1], COMMAND_LCM_GET_BATTERY);

        int32_t index = 2;
        EXPECT_EQ_U32(buffer_get_uint32(payload, &index), 0u);
        EXPECT_EQ_U32(index, 6u);
    }

    return true;
}
