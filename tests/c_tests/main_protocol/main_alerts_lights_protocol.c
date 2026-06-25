enum {
    MAIN_COMMAND_LIGHTS_CONTROL = 20,
    MAIN_COMMAND_ALERTS_LIST = 35,
    MAIN_COMMAND_ALERTS_CONTROL = 36,
};

static bool test_main_lights_control_protocol(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_EQ_U32(len, 0u);

    uint8_t set_lights[7] = {101, MAIN_COMMAND_LIGHTS_CONTROL};
    int32_t index = 2;
    buffer_append_uint32(set_lights, 0x00000003u, &index);
    set_lights[index++] = 0x01u;
    EXPECT_EQ_U32(index, sizeof(set_lights));
    vesc_if_fake_invoke_app_data_handler(set_lights, sizeof(set_lights));

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 3u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], MAIN_COMMAND_LIGHTS_CONTROL);
    EXPECT_EQ_U32(payload[2], 0x01u);

    index = 2;
    buffer_append_uint32(set_lights, 0x00000002u, &index);
    set_lights[index++] = 0x02u;
    vesc_if_fake_invoke_app_data_handler(set_lights, sizeof(set_lights));

    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 3u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], MAIN_COMMAND_LIGHTS_CONTROL);
    EXPECT_EQ_U32(payload[2], 0x03u);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_alerts_protocol(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->time.now = 1234u;
    alert_tracker_add(&d->alert_tracker, &d->time, ALERT_FW_FAULT, FAULT_CODE_NONE);
    alert_tracker_finalize(&d->alert_tracker, &d->time);
    EXPECT_TRUE(d->alert_tracker.fatal_error);

    uint8_t list_request[6] = {101, MAIN_COMMAND_ALERTS_LIST};
    int32_t index = 2;
    buffer_append_uint32(list_request, 0u, &index);
    EXPECT_EQ_U32(index, sizeof(list_request));
    vesc_if_fake_invoke_app_data_handler(list_request, sizeof(list_request));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 19u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], MAIN_COMMAND_ALERTS_LIST);

    index = 2;
    EXPECT_EQ_U32(buffer_get_uint32(payload, &index), 0x00000001u);
    EXPECT_EQ_U32(buffer_get_uint32(payload, &index), 0x00000000u);
    EXPECT_EQ_U32(payload[index++], FAULT_CODE_NONE);
    EXPECT_EQ_U32(payload[index++], 1u);
    EXPECT_EQ_U32(buffer_get_uint32(payload, &index), 1234u);
    EXPECT_EQ_U32(payload[index++], ALERT_FW_FAULT);
    EXPECT_EQ_U32(payload[index++], 1u);
    EXPECT_EQ_U32(payload[index++], FAULT_CODE_NONE);
    EXPECT_EQ_U32(index, len);

    uint8_t filtered_request[6] = {101, MAIN_COMMAND_ALERTS_LIST};
    index = 2;
    buffer_append_uint32(filtered_request, 1234u, &index);
    vesc_if_fake_invoke_app_data_handler(filtered_request, sizeof(filtered_request));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 12u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], MAIN_COMMAND_ALERTS_LIST);
    EXPECT_EQ_U32(payload[11], 0u);

    uint8_t clear_request[] = {101, MAIN_COMMAND_ALERTS_CONTROL, 1};
    vesc_if_fake_invoke_app_data_handler(clear_request, sizeof(clear_request));
    EXPECT_TRUE(!d->alert_tracker.fatal_error);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_invalid_command_protocol(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    (void) fixture.data;

    uint8_t info_request[] = {101, COMMAND_INFO, 2, 0};
    vesc_if_fake_invoke_app_data_handler(info_request, sizeof(info_request));
    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 60u);

    uint8_t bad_magic[] = {100, COMMAND_INFO, 2, 0};
    vesc_if_fake_invoke_app_data_handler(bad_magic, sizeof(bad_magic));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 60u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], COMMAND_INFO);

    uint8_t unknown_command[] = {101, 250};
    vesc_if_fake_invoke_app_data_handler(unknown_command, sizeof(unknown_command));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(len, 60u);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], COMMAND_INFO);

    main_protocol_fixture_stop(&fixture);
    return true;
}
