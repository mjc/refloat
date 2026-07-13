enum {
    MAIN_COMMAND_GET_ALLDATA = 10,
};

static bool test_main_all_data_saturates_oversized_float16_fields(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->balance_current.value = 4000.0f;
    d->footpad.adc_left = NAN;
    d->footpad.adc_right = 10.0f;
    d->setpoint = 100.0f;
    d->motor.duty_raw = 10.0f;
    d->motor.mosfet_temp = NAN;
    d->motor.motor_temp = 200.0f;
    d->motor.erpm = 100000.0f;
    vesc_if_fake_set_battery_level(2.0f);

    uint8_t request[] = {101, MAIN_COMMAND_GET_ALLDATA, 3};
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));

    size_t len = 0;
    const uint8_t *payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[0], 101u);
    EXPECT_EQ_U32(payload[1], MAIN_COMMAND_GET_ALLDATA);
    EXPECT_EQ_U32(payload[2], 3u);

    int32_t index = 3;
    EXPECT_EQ_U32(buffer_get_uint16(payload, &index), 0x7fffu);
    EXPECT_EQ_U32(payload[11], 0u);
    EXPECT_EQ_U32(payload[12], 255u);
    EXPECT_EQ_U32(payload[13], 255u);
    EXPECT_EQ_U32(payload[32], 255u);
    EXPECT_EQ_U32(payload[38], 0u);
    EXPECT_EQ_U32(payload[39], 255u);
    EXPECT_EQ_U32(payload[53], 250u);
    index = 24;
    EXPECT_TRUE(buffer_get_int16(payload, &index) == INT16_MAX);

    d->motor.erpm = -100000.0f;
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    payload = vesc_if_fake_last_app_data(&len);
    index = 24;
    EXPECT_TRUE(buffer_get_int16(payload, &index) == INT16_MIN);

    // VESC Tool permits zero battery capacity; BLDC then returns NaN from
    // mc_get_battery_level, which must not reach a byte conversion.
    vesc_if_fake_set_battery_level(NAN);
    vesc_if_fake_invoke_app_data_handler(request, sizeof(request));
    payload = vesc_if_fake_last_app_data(&len);
    EXPECT_TRUE(payload != NULL);
    EXPECT_EQ_U32(payload[53], 0u);

    main_protocol_fixture_stop(&fixture);
    return true;
}
