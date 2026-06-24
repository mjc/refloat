static bool test_sma_growth_transition_edges(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fill_next_malloc(0x7f);

    SMA sma;
    sma_init(&sma);
    sma_configure(&sma, 8.5f, 100.0f);
    EXPECT_TRUE(sma.array != NULL);
    EXPECT_TRUE(sma.n == 5);
    EXPECT_TRUE(sma.allocated_n == 6);

    for (uint8_t i = 0; i < sma.n; ++i) {
        sma_update(&sma, 10.0f);
    }
    EXPECT_FLOAT_NEAR(sma.value, 10.0f);

    uint8_t old_n = sma.n;
    sma_configure(&sma, 7.0f, 100.0f);
    EXPECT_TRUE(sma.n == old_n);
    EXPECT_TRUE(sma.new_n == 6);

    for (uint8_t i = 0; i < old_n; ++i) {
        sma_update(&sma, 10.0f);
    }
    EXPECT_TRUE(sma.n == 6);
    EXPECT_TRUE(sma.new_n == 0);
    EXPECT_TRUE(sma.idx == old_n);
    EXPECT_FLOAT_NEAR(sma.array[old_n], 10.0f);
    EXPECT_FLOAT_NEAR(sma.value, 10.0f);

    sma_update(&sma, 22.0f);
    EXPECT_TRUE(sma.idx == 0);
    EXPECT_FLOAT_NEAR(sma.value, 12.0f);

    sma_configure(&sma, 6.0f, 100.0f);
    EXPECT_TRUE(sma.new_n == 0);
    EXPECT_TRUE(sma.n == 6);

    sma_destroy(&sma);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);

    return true;
}

typedef struct {
    SMA *sma;
    float value;
} SmaUpdateGuard;

static bool run_sma_update(void *ctx) {
    SmaUpdateGuard *guard = ctx;
    sma_update(guard->sma, guard->value);
    return true;
}

static bool test_sma_allocation_failure_update(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fail_next_malloc();

    SMA sma;
    sma_init(&sma);
    sma_configure(&sma, 1.0f, 100.0f);
    EXPECT_TRUE(sma.array == NULL);
    EXPECT_EQ_U32(sma.n, 0u);

    SmaUpdateGuard guard = {.sma = &sma, .value = 12.0f};
    EXPECT_TRUE(test_expect_no_signal(SIGSEGV, run_sma_update, &guard));

    EXPECT_FLOAT_NEAR(sma.value, 0.0f);
    EXPECT_EQ_U32(sma.idx, 0u);
    EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_free_calls(), 0u);

    return true;
}

static bool test_circular_buffer_pop_index(void) {
    BufferItem storage[3] = {{0}};
    CircularBuffer cb;
    circular_buffer_init(&cb, sizeof(BufferItem), 3, storage);

    BufferItem a = {1, 11};
    BufferItem b = {2, 22};
    BufferItem c = {3, 33};
    BufferItem out = {0};

    circular_buffer_push(&cb, &a);
    circular_buffer_push(&cb, &b);
    circular_buffer_push(&cb, &c);

    EXPECT_TRUE(circular_buffer_pop(&cb, 1, &out));
    EXPECT_TRUE(buffer_item_eq(out, b));
    EXPECT_TRUE(circular_buffer_size(&cb) == 2);
    EXPECT_TRUE(circular_buffer_get(&cb, 0, &out));
    EXPECT_TRUE(buffer_item_eq(out, a));
    EXPECT_TRUE(circular_buffer_get(&cb, 1, &out));
    EXPECT_TRUE(buffer_item_eq(out, c));

    return true;
}

static bool test_lcm_light_ctrl_payload_clamps(void) {
    LcmData lcm = {0};
    lcm.enabled = true;

    unsigned char short_cfg[] = {11, 22, 33, 44, 55};
    lcm_light_ctrl_request(&lcm, short_cfg, (int) sizeof(short_cfg));
    EXPECT_TRUE(lcm.brightness == 11);
    EXPECT_TRUE(lcm.brightness_idle == 22);
    EXPECT_TRUE(lcm.status_brightness == 33);
    EXPECT_TRUE(lcm.payload_size == 2);
    EXPECT_TRUE(lcm.payload[0] == 44);
    EXPECT_TRUE(lcm.payload[1] == 55);

    unsigned char max_cfg[3 + MAX_LCM_PAYLOAD_LENGTH];
    for (size_t i = 0; i < sizeof(max_cfg); ++i) {
        max_cfg[i] = (unsigned char) i;
    }

    lcm_light_ctrl_request(&lcm, max_cfg, (int) sizeof(max_cfg));
    EXPECT_TRUE(lcm.payload_size == MAX_LCM_PAYLOAD_LENGTH);
    EXPECT_TRUE(lcm.payload[0] == 3);
    EXPECT_TRUE(lcm.payload[MAX_LCM_PAYLOAD_LENGTH - 1] == (unsigned char) (2 + MAX_LCM_PAYLOAD_LENGTH));

    unsigned char oversized_cfg[3 + MAX_LCM_PAYLOAD_LENGTH + 5];
    for (size_t i = 0; i < sizeof(oversized_cfg); ++i) {
        oversized_cfg[i] = (unsigned char) (200 + i);
    }

    lcm_light_ctrl_request(&lcm, oversized_cfg, (int) sizeof(oversized_cfg));
    EXPECT_TRUE(lcm.payload_size == MAX_LCM_PAYLOAD_LENGTH);
    EXPECT_TRUE(lcm.payload[0] == 203);
    EXPECT_TRUE(lcm.payload[MAX_LCM_PAYLOAD_LENGTH - 1] == (unsigned char) (202 + MAX_LCM_PAYLOAD_LENGTH));

    return true;
}

static bool test_lcm_disabled_responses_are_minimal(void) {
    vesc_if_fake_reset();

    LcmData lcm = {0};
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
    EXPECT_TRUE(strcmp(lcm.name, "External") == 0);

    unsigned char ctrl[] = {1, 2, 3, 4, 5, 6};
    lcm_light_ctrl_request(&lcm, ctrl, (int) sizeof(ctrl));
    EXPECT_EQ_U32(lcm.brightness, 12u);
    EXPECT_EQ_U32(lcm.payload_size, 2u);

    State state = {.state = STATE_RUNNING};
    MotorData motor = {0};
    lcm_poll_response(&lcm, &state, FS_BOTH, &motor, 45.0f);
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 2u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_POLL);
    EXPECT_EQ_U32(lcm.payload_size, 2u);

    lcm_light_info_response(&lcm);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 3u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_LIGHT_INFO);
    EXPECT_EQ_U32(payload[2], 0u);

    lcm_device_info_response(&lcm);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 2u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_DEVICE_INFO);

    lcm_get_battery_response(&lcm);
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

    CfgLeds cfg = {0};
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
    EXPECT_EQ_U32(lcm.brightness, 82u);
    EXPECT_EQ_U32(lcm.brightness_idle, 37u);
    EXPECT_EQ_U32(lcm.status_brightness, 41u);
    EXPECT_TRUE(!lcm.lights_off_when_lifted);

    lcm_fakes_set_runtime_status(true, false);
    lcm_configure(&lcm, &leds);
    EXPECT_EQ_U32(lcm.brightness, 37u);
    EXPECT_EQ_U32(lcm.brightness_idle, 37u);
    EXPECT_EQ_U32(lcm.status_brightness, 19u);

    lcm_fakes_set_runtime_status(false, false);
    lcm_configure(&lcm, &leds);
    EXPECT_EQ_U32(lcm.brightness, 0u);
    EXPECT_EQ_U32(lcm.brightness_idle, 0u);
    EXPECT_EQ_U32(lcm.status_brightness, 0u);

    lcm.enabled = false;
    lcm.brightness = 99;
    lcm_fakes_set_runtime_status(true, true);
    lcm_configure(&lcm, &leds);
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
    LcmData lcm = {0};
    lcm.enabled = true;
    Leds leds = {0};
    lcm_fakes_set_runtime_status(true, false);

    LcmConfigureGuard guard = {.lcm = &lcm, .leds = &leds};
    EXPECT_TRUE(test_expect_no_signal(SIGSEGV, run_lcm_configure, &guard));

    EXPECT_EQ_U32(lcm.brightness, 0u);
    EXPECT_EQ_U32(lcm.brightness_idle, 0u);
    EXPECT_EQ_U32(lcm.status_brightness, 0u);
    EXPECT_TRUE(lcm.lights_off_when_lifted);

    return true;
}

static bool test_lcm_poll_response_pitch_payload_and_name_edges(void) {
    vesc_if_fake_reset();
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 2.0f, 49.5f, 0, 0);

    LcmData lcm = {0};
    lcm.enabled = true;
    lcm.brightness = 11;
    lcm.brightness_idle = 22;
    lcm.status_brightness = 33;
    lcm.lights_off_when_lifted = true;

    uint8_t empty_name[] = {0};
    memcpy(lcm.name, "Previous", 9);
    lcm_poll_request(&lcm, empty_name, 0);
    EXPECT_TRUE(strcmp(lcm.name, "Previous") == 0);

    uint8_t long_name[MAX_LCM_NAME_LENGTH + 4];
    memset(long_name, 'A', sizeof(long_name));
    long_name[MAX_LCM_NAME_LENGTH - 1] = '\0';
    lcm_poll_request(&lcm, long_name, sizeof(long_name));
    EXPECT_TRUE(strlen(lcm.name) == MAX_LCM_NAME_LENGTH - 1);

    unsigned char ctrl[] = {44, 55, 66, 77, 88, 99};
    lcm_light_ctrl_request(&lcm, ctrl, (int) sizeof(ctrl));
    EXPECT_EQ_U32(lcm.brightness, 44u);
    EXPECT_EQ_U32(lcm.payload_size, 3u);

    State state = {.state = STATE_READY, .mode = MODE_NORMAL};
    MotorData motor = {0};
    motor.erpm = 123.0f;
    lcm_poll_response(&lcm, &state, FS_RIGHT, &motor, -27.4f);

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
    EXPECT_EQ_U32(lcm.payload_size, 0u);

    lcm.lights_off_when_lifted = false;
    lcm_poll_response(&lcm, &state, FS_NONE, &motor, 62.0f);
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[4], 0u);
    EXPECT_EQ_U32(len, 14u);

    return true;
}

static bool test_lcm_poll_request_name_length_bound(void) {
    LcmData lcm = {0};
    lcm.enabled = true;
    memset(lcm.name, 0, sizeof(lcm.name));

    uint8_t request[] = {'A', 'B', 'C', 'Z'};
    lcm_poll_request(&lcm, request, 3);

    EXPECT_TRUE(strcmp(lcm.name, "ABC") == 0);

    return true;
}

static bool test_lcm_poll_response_saturates_byte_fields(void) {
    vesc_if_fake_reset();

    LcmData lcm = {0};
    lcm.enabled = true;
    lcm.lights_off_when_lifted = true;

    State state = {.state = STATE_READY, .mode = MODE_NORMAL};
    MotorData motor = {0};

    lcm_poll_response(&lcm, &state, FS_NONE, &motor, 300.0f);

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 14u);
    EXPECT_EQ_U32(payload[1], COMMAND_LCM_POLL);
    EXPECT_EQ_U32(payload[4], UINT8_MAX);

    state.state = STATE_RUNNING;
    motor.duty_cycle.value = 3.0f;
    lcm_poll_response(&lcm, &state, FS_NONE, &motor, 0.0f);

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[4], 100u);

    return true;
}

static bool test_lcm_battery_response_nonfinite_values_are_stable(void) {
    const float values[] = {NAN, INFINITY, -INFINITY};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        vesc_if_fake_reset();
        vesc_if_fake_set_battery_level(values[i]);

        LcmData lcm = {0};
        lcm.enabled = true;

        lcm_get_battery_response(&lcm);

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
