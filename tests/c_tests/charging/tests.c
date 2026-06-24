static bool test_charging_timeout_boundaries(void) {
    vesc_if_fake_reset();

    Charging charging;
    State state = {.charging = true};
    charging_init(&charging);

    vesc_if_fake_set_seconds(10.0f);
    uint8_t charge_buf[] = {151, 1, 0, 120, 0, 30};
    charging_state_request(&charging, charge_buf, sizeof(charge_buf), &state);
    EXPECT_TRUE(state.charging);
    EXPECT_FLOAT_NEAR(charging.timer, 10.0f);
    EXPECT_FLOAT_NEAR(charging.voltage, 12.0f);
    EXPECT_FLOAT_NEAR(charging.current, 3.0f);

    vesc_if_fake_set_seconds(15.0f);
    charging_timeout(&charging, &state);
    EXPECT_TRUE(state.charging);

    vesc_if_fake_set_seconds(15.001f);
    charging_timeout(&charging, &state);
    EXPECT_TRUE(!state.charging);

    state.charging = true;
    vesc_if_fake_set_seconds(20.0f);
    uint8_t not_charging_buf[] = {151, 0, 0xff, 0xff, 0xff, 0xff};
    charging_state_request(&charging, not_charging_buf, sizeof(not_charging_buf), &state);
    EXPECT_TRUE(!state.charging);
    EXPECT_FLOAT_NEAR(charging.timer, 20.0f);
    EXPECT_FLOAT_NEAR(charging.voltage, 0.0f);
    EXPECT_FLOAT_NEAR(charging.current, 0.0f);

    vesc_if_fake_set_seconds(24.0f);
    charging_timeout(&charging, &state);
    EXPECT_TRUE(!state.charging);

    return true;
}

static bool test_charging_signed_payload_and_invalid_frame_edges(void) {
    vesc_if_fake_reset();

    Charging charging;
    State state = {.charging = false};
    charging_init(&charging);

    charging.timer = 7.0f;
    charging.voltage = 1.5f;
    charging.current = 0.5f;

    vesc_if_fake_set_seconds(30.0f);
    uint8_t short_buf[] = {151, 2, 0xff, 0x9c, 0x00};
    charging_state_request(&charging, short_buf, sizeof(short_buf), &state);
    EXPECT_TRUE(!state.charging);
    EXPECT_FLOAT_NEAR(charging.timer, 7.0f);
    EXPECT_FLOAT_NEAR(charging.voltage, 1.5f);
    EXPECT_FLOAT_NEAR(charging.current, 0.5f);

    uint8_t signed_charge_buf[] = {151, 2, 0xff, 0x9c, 0xff, 0xce};
    charging_state_request(&charging, signed_charge_buf, sizeof(signed_charge_buf), &state);
    EXPECT_TRUE(!state.charging);
    EXPECT_FLOAT_NEAR(charging.timer, 7.0f);
    EXPECT_FLOAT_NEAR(charging.voltage, 1.5f);
    EXPECT_FLOAT_NEAR(charging.current, 0.5f);

    vesc_if_fake_set_seconds(31.0f);
    uint8_t bad_magic_buf[] = {0, 0, 0x00, 0x64, 0x00, 0x32};
    charging_state_request(&charging, bad_magic_buf, sizeof(bad_magic_buf), &state);
    EXPECT_TRUE(!state.charging);
    EXPECT_FLOAT_NEAR(charging.timer, 7.0f);
    EXPECT_FLOAT_NEAR(charging.voltage, 1.5f);
    EXPECT_FLOAT_NEAR(charging.current, 0.5f);

    return true;
}
