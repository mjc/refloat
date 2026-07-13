static bool test_leds_status_idle_blend_starts_after_timeout_and_resets_on_sensor(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 12.0f, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 12.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 12.1f);

    update_leds_at(&fixture, FS_NONE, 14.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_blend, 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_animation_start, 14.2f);

    vesc_if_fake_set_motor_telemetry(ERPM_MOVING_THRESHOLD + 1.0f, 0, 0, 0, 0, 0, 0, 50, 25, 25);
    update_leds_at(&fixture, FS_NONE, 14.25f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 14.25f);

    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 0, 50, 25, 25);
    update_leds_at(&fixture, FS_LEFT, 14.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 14.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_blend, 0.1f);
    update_leds_at(&fixture, FS_LEFT, 14.4f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 14.4f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_blend, 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_blend_front_idle_status(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 22.0f, STATE_READY);

    vesc_if_fake_set_imu(deg2rad(65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 22.1f);
    EXPECT_TRUE(fixture.leds.board_is_upright);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_blend, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 22.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.0f);

    update_leds_at(&fixture, FS_NONE, 25.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.1f);

    update_leds_at(&fixture, FS_RIGHT, 25.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 25.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.0f);

    fixture.cfg.lights_off_when_lifted = false;
    update_leds_at(&fixture, FS_NONE, 28.4f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_blend, 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_render_battery_boundaries(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 20.0f, STATE_READY);

    vesc_if_fake_set_battery_level(0.5f);
    update_leds_at(&fixture, FS_NONE, 20.1f);

    EXPECT_TRUE(fixture.leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[1] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[0] == fixture.leds.status_strip.data[1]);
    EXPECT_TRUE(fixture.leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[3] == 0u);

    uint32_t mid_battery_first_led = fixture.leds.status_strip.data[0];
    vesc_if_fake_set_battery_level(0.0f);
    update_leds_at(&fixture, FS_NONE, 20.2f);

    EXPECT_TRUE(fixture.leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[0] != mid_battery_first_led);
    EXPECT_TRUE(fixture.leds.status_strip.data[1] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[3] == 0u);

    vesc_if_fake_set_battery_level(NAN);
    update_leds_at(&fixture, FS_NONE, 20.3f);
    EXPECT_TRUE(fixture.leds.status_strip.data[0] != 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[1] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[2] == 0u);
    EXPECT_TRUE(fixture.leds.status_strip.data[3] == 0u);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_retain_utilization_hysteresis(void) {
    reset_fakes(18.0f);
    LedsFixture fixture = leds_fixture_prepare(18.0f);
    fixture.cfg.status.motor_utilization_threshold = 0.5f;
    leds_fixture_start(&fixture, STATE_RUNNING);

    fixture.motor.duty_cycle.value = 0.54f;
    update_leds_at(&fixture, FS_NONE, 18.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.motor.duty_cycle.value = 0.405f;
    update_leds_at(&fixture, FS_NONE, 18.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.motor.duty_cycle.value = 0.35f;
    update_leds_at(&fixture, FS_NONE, 18.3f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 0.0f);

    fixture.motor.duty_cycle.value = 0.0f;
    fixture.motor.motor_current_saturation = 0.9f;
    update_leds_at(&fixture, FS_NONE, 18.4f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.motor.motor_current_saturation = 0.0f;
    fixture.motor.battery_current_saturation = 0.95f;
    update_leds_at(&fixture, FS_NONE, 18.5f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 10.0f / LEDS_REFRESH_RATE);

    fixture.state.mode = MODE_FLYWHEEL;
    update_leds_at(&fixture, FS_NONE, 18.6f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 5.0f / LEDS_REFRESH_RATE);

    fixture.state.mode = MODE_NORMAL;
    fixture.motor.battery_current_saturation = 1.0f;
    fixture.leds.status_utilization_blend = 1.0f;
    update_leds_at(&fixture, FS_NONE, 18.7f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_utilization_blend, 1.0f);

    leds_fixture_destroy(&fixture);
    return true;
}
