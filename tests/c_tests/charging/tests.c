typedef struct {
    Charging charging;
    State state;
} ChargingFixture;

static void charging_fixture_start(ChargingFixture *fixture, bool charging_state) {
    vesc_if_fake_reset();
    *fixture = (ChargingFixture){0};
    charging_init(&fixture->charging);
    fixture->state = (State){.charging = charging_state};
}

static bool test_charging_timeout_boundaries(void) {
    ChargingFixture fixture;
    charging_fixture_start(&fixture, true);

    vesc_if_fake_set_seconds(10.0f);
    uint8_t charge_buf[] = {151, 1, 0, 120, 0, 30};
    charging_state_request(&fixture.charging, charge_buf, sizeof(charge_buf), &fixture.state);
    EXPECT_TRUE(fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.timer, 10.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.voltage, 12.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.current, 3.0f);

    vesc_if_fake_set_seconds(15.0f);
    charging_timeout(&fixture.charging, &fixture.state);
    EXPECT_TRUE(fixture.state.charging);

    vesc_if_fake_set_seconds(15.001f);
    charging_timeout(&fixture.charging, &fixture.state);
    EXPECT_TRUE(!fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.voltage, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.current, 0.0f);

    fixture.state.charging = true;
    vesc_if_fake_set_seconds(20.0f);
    uint8_t not_charging_buf[] = {151, 0, 0xff, 0xff, 0xff, 0xff};
    charging_state_request(
        &fixture.charging, not_charging_buf, sizeof(not_charging_buf), &fixture.state
    );
    EXPECT_TRUE(!fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.timer, 20.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.voltage, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.current, 0.0f);

    vesc_if_fake_set_seconds(24.0f);
    charging_timeout(&fixture.charging, &fixture.state);
    EXPECT_TRUE(!fixture.state.charging);

    return true;
}

static bool test_charging_decodes_signed_payloads_and_rejects_malformed_frames(void) {
    ChargingFixture fixture;
    charging_fixture_start(&fixture, false);

    fixture.charging.timer = 7.0f;
    fixture.charging.voltage = 1.5f;
    fixture.charging.current = 0.5f;

    vesc_if_fake_set_seconds(30.0f);
    uint8_t short_buf[] = {151, 2, 0xff, 0x9c, 0x00};
    charging_state_request(&fixture.charging, short_buf, sizeof(short_buf), &fixture.state);
    EXPECT_TRUE(!fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.timer, 7.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.voltage, 1.5f);
    EXPECT_FLOAT_NEAR(fixture.charging.current, 0.5f);

    uint8_t negative_current_buf[] = {151, 1, 0, 120, 0xff, 0xce};
    charging_state_request(
        &fixture.charging, negative_current_buf, sizeof(negative_current_buf), &fixture.state
    );
    EXPECT_TRUE(!fixture.state.charging);

    uint8_t signed_charge_buf[] = {151, 2, 0xff, 0x9c, 0xff, 0xce};
    charging_state_request(
        &fixture.charging, signed_charge_buf, sizeof(signed_charge_buf), &fixture.state
    );
    EXPECT_TRUE(!fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.timer, 7.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.voltage, 1.5f);
    EXPECT_FLOAT_NEAR(fixture.charging.current, 0.5f);

    vesc_if_fake_set_seconds(31.0f);
    uint8_t bad_magic_buf[] = {0, 0, 0x00, 0x64, 0x00, 0x32};
    charging_state_request(&fixture.charging, bad_magic_buf, sizeof(bad_magic_buf), &fixture.state);
    EXPECT_TRUE(!fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.timer, 7.0f);
    EXPECT_FLOAT_NEAR(fixture.charging.voltage, 1.5f);
    EXPECT_FLOAT_NEAR(fixture.charging.current, 0.5f);

    fixture.state.state = STATE_RUNNING;
    uint8_t running_charge_buf[] = {151, 1, 0, 120, 0, 30};
    charging_state_request(
        &fixture.charging, running_charge_buf, sizeof(running_charge_buf), &fixture.state
    );
    EXPECT_TRUE(!fixture.state.charging);
    EXPECT_FLOAT_NEAR(fixture.charging.timer, 7.0f);

    const Mode blocked_modes[] = {MODE_HANDTEST, MODE_FLYWHEEL};
    fixture.state.state = STATE_READY;
    for (size_t i = 0; i < sizeof(blocked_modes) / sizeof(blocked_modes[0]); ++i) {
        fixture.state.mode = blocked_modes[i];
        charging_state_request(
            &fixture.charging, running_charge_buf, sizeof(running_charge_buf), &fixture.state
        );
        EXPECT_TRUE(!fixture.state.charging);
    }

    return true;
}

static bool test_charging_timeout_survives_vesc_tick_wrap(void) {
    ChargingFixture fixture;
    charging_fixture_start(&fixture, true);
    fixture.charging.timer = 429496.7296f - 4.0f;
    vesc_if_fake_set_seconds(2.0f);

    charging_timeout(&fixture.charging, &fixture.state);

    EXPECT_TRUE(!fixture.state.charging);
    return true;
}
