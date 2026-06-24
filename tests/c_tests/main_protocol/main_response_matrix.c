enum {
    MAIN_RESPONSE_INFO = 0,
    MAIN_RESPONSE_GET_RTDATA = 1,
    MAIN_RESPONSE_GET_ALLDATA = 10,
    MAIN_RESPONSE_REALTIME_INTERNAL = 31,
    MAIN_RESPONSE_REALTIME_INTERNAL_IDS = 32,
    MAIN_LEGACY_BEEP_ERROR = 10,
    MAIN_BEEP_CELL_BALANCE = 16,
    MAIN_BEEP_FW_FAULT = 19,
};

static bool test_main_all_data_mode_matrix(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    uint8_t request[] = {101, MAIN_RESPONSE_GET_ALLDATA, 0};
    size_t lengths[5] = {0};

    d->state.mode = MODE_HANDTEST;
    for (uint8_t mode = 0; mode < 5; ++mode) {
        request[2] = mode;
        vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
        const uint8_t *payload = vesc_if_fake_last_app_data(&lengths[mode]);
        EXPECT_TRUE(payload != NULL);
        EXPECT_EQ_U32(payload[2], mode);
    }
    EXPECT_EQ_U32(lengths[0], lengths[1]);
    EXPECT_TRUE(lengths[2] > lengths[1]);
    EXPECT_TRUE(lengths[3] > lengths[2]);
    EXPECT_TRUE(lengths[4] > lengths[3]);

    fake_vesc_if.foc_get_id = NULL;
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    EXPECT_TRUE(vesc_if_fake_last_app_data(NULL) != NULL);

    vesc_if_fake_set_fault(FAULT_CODE_OVER_VOLTAGE);
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(len, 4u);
    EXPECT_EQ_U32(payload[2], 69u);
    EXPECT_EQ_U32(payload[3], FAULT_CODE_OVER_VOLTAGE);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_info_version_and_capability_matrix(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->float_conf.hardware.leds.mode = LED_MODE_BOTH;
    d->data_record.enabled = true;

    uint8_t v1[] = {101, MAIN_RESPONSE_INFO};
    vesc_if_fake_invoke_app_data_handler(v1, sizeof(v1));
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(payload[4], 3u);

    uint8_t v0[] = {101, MAIN_RESPONSE_INFO, 0};
    vesc_if_fake_invoke_app_data_handler(v0, sizeof(v0));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(len, 3u);
    EXPECT_EQ_U32(payload[2], 0u);

    uint8_t v2[] = {101, MAIN_RESPONSE_INFO, 2, 0x5a};
    vesc_if_fake_invoke_app_data_handler(v2, sizeof(v2));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(payload[2], 2u);
    EXPECT_EQ_U32(payload[3], 0x5au);
    int32_t index = 55;
    uint32_t capabilities = buffer_get_uint32(payload, &index);
    EXPECT_TRUE(capabilities & (1u << 31));
    EXPECT_EQ_U32(capabilities & 0x3u, 0x3u);

    d->float_conf.hardware.leds.mode = LED_MODE_INTERNAL;
    vesc_if_fake_invoke_app_data_handler(v2, sizeof(v2));
    payload = vesc_if_fake_last_app_data(&len);
    index = 55;
    capabilities = buffer_get_uint32(payload, &index);
    EXPECT_EQ_U32(capabilities & 0x3u, 0x1u);

    uint8_t newest[] = {101, MAIN_RESPONSE_INFO, UINT8_MAX, 0xff};
    vesc_if_fake_invoke_app_data_handler(newest, sizeof(newest));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(payload[2], 2u);
    EXPECT_EQ_U32(payload[3], 0u);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_internal_realtime_state_matrix(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    uint8_t request[] = {101, MAIN_RESPONSE_REALTIME_INTERNAL};

    d->state.state = STATE_READY;
    d->state.charging = false;
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    size_t ready_len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&ready_len);
    EXPECT_EQ_U32(payload[2], 0x4u);

    d->state.state = STATE_RUNNING;
    d->state.charging = true;
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    size_t running_len = 0;
    payload = vesc_if_fake_last_app_data(&running_len);
    EXPECT_EQ_U32(payload[2], 0x7u);
    EXPECT_TRUE(running_len > ready_len);

    request[1] = MAIN_RESPONSE_REALTIME_INTERNAL_IDS;
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    size_t ids_len = 0;
    payload = vesc_if_fake_last_app_data(&ids_len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_TRUE(ids_len > 4u);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_legacy_realtime_maps_extended_beeps_to_error(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    uint8_t requests[][3] = {
        {101, MAIN_RESPONSE_GET_RTDATA, 0},
        {101, MAIN_RESPONSE_GET_ALLDATA, 0},
    };
    const size_t beep_offsets[] = {15u, 10u};
    for (int reason = MAIN_BEEP_CELL_BALANCE; reason <= MAIN_BEEP_FW_FAULT; ++reason) {
        d->beep_reason = reason;
        for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); ++i) {
            vesc_if_fake_invoke_app_data_handler(requests[i], sizeof(requests[i]));
            size_t len = 0;
            const uint8_t *payload = vesc_if_fake_last_app_data(&len);
            EXPECT_TRUE(len > beep_offsets[i]);
            EXPECT_EQ_U32(payload[beep_offsets[i]] >> 4, MAIN_LEGACY_BEEP_ERROR);
        }
    }

    d->state.mode = MODE_HANDTEST;
    d->state.charging = true;
    vesc_if_fake_invoke_app_data_handler(requests[0], sizeof(requests[0]));
    EXPECT_TRUE(vesc_if_fake_last_app_data(NULL) != NULL);

    main_protocol_fixture_stop(&fixture);
    return true;
}
