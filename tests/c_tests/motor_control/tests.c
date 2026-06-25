typedef struct {
    MotorControl mc;
    RefloatConfig cfg;
    Time time;
} MotorControlFixture;

static MotorControlFixture motor_control_fixture_start(void) {
    vesc_if_fake_reset();

    MotorControlFixture fixture = {0};
    motor_control_init(&fixture.mc);
    fixture.time.now = 1000000u;
    return fixture;
}

static void motor_control_fixture_configure(
    MotorControlFixture *fixture, float brake_current, float startup_click_current,
    ParkingBrakeMode parking_brake_mode, uint32_t main_freq
) {
    fixture->cfg = (RefloatConfig) {
        .brake_current = brake_current,
        .startup_click_current = startup_click_current,
        .parking_brake_mode = parking_brake_mode,
    };
    motor_control_configure(&fixture->mc, &fixture->cfg, main_freq);
}

static bool test_motor_control_current_brake_and_tone(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    EXPECT_TRUE(isnan(fixture.mc.requested_current));
    EXPECT_TRUE(!fixture.mc.disabled);

    motor_control_fixture_configure(&fixture, 7.5f, 2.0f, PARKING_BRAKE_IDLE, 1000u);
    EXPECT_FLOAT_NEAR(fixture.mc.brake_current, 7.5f);
    EXPECT_FLOAT_NEAR(fixture.mc.click_current, 2.0f);
    EXPECT_TRUE(fixture.mc.main_freq == 500u);

    motor_control_apply(&fixture.mc, 0.0f, STATE_DISABLED, &fixture.time);
    EXPECT_TRUE(fixture.mc.disabled);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    EXPECT_TRUE(vesc_if_fake_timeout_reset_calls() == 0);

    motor_control_apply(&fixture.mc, 0.0f, STATE_DISABLED, &fixture.time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == 1);

    motor_control_request_current(&fixture.mc, 4.25f);
    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_TRUE(!fixture.mc.disabled);
    EXPECT_TRUE(isnan(fixture.mc.requested_current));
    EXPECT_TRUE(vesc_if_fake_timeout_reset_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_off_delay_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current_off_delay(), 0.05f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 4.25f);

    motor_control_apply(&fixture.mc, 3000.0f, STATE_RUNNING, &fixture.time);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), 7.5f);

    fixture.time.now += 2000000u;
    motor_control_apply(&fixture.mc, 0.0f, STATE_RUNNING, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);

    fixture.mc.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    timer_refresh(&fixture.time, &fixture.mc.brake_timer);
    motor_control_apply(&fixture.mc, 0.0f, STATE_RUNNING, &fixture.time);
    EXPECT_TRUE(fixture.mc.parking_brake_active);
    size_t duty_calls_before = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&fixture.mc, 0.0f, STATE_STARTUP, &fixture.time);
    EXPECT_TRUE(fixture.mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before + 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

    motor_control_play_tone(&fixture.mc, 250u, 1.5f);
    EXPECT_TRUE(fixture.mc.tone_ticks == 2u);
    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -1.5f);
    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 1.5f);

    motor_control_stop_tone(&fixture.mc);
    EXPECT_TRUE(fixture.mc.tone_ticks == 0u);
    EXPECT_TRUE(fixture.mc.tone_counter == 0u);

    motor_control_play_click(&fixture.mc);
    EXPECT_TRUE(fixture.mc.click_counter == 3u);
    EXPECT_TRUE(fixture.mc.tone_ticks > 0u);

    return true;
}

static bool test_motor_control_parking_and_tone_edges(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 6.0f, 0.0f, PARKING_BRAKE_NEVER, 1000u);

    timer_refresh(&fixture.time, &fixture.mc.brake_timer);
    motor_control_apply(&fixture.mc, 0.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(!fixture.mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), fixture.cfg.brake_current);

    fixture.cfg.parking_brake_mode = PARKING_BRAKE_IDLE;
    motor_control_configure(&fixture.mc, &fixture.cfg, 1000u);
    motor_control_apply(&fixture.mc, 100.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(!fixture.mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 2);

    motor_control_apply(&fixture.mc, 25.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(fixture.mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == 1);

    motor_control_apply(&fixture.mc, 25.0f, STATE_RUNNING, &fixture.time);
    EXPECT_TRUE(!fixture.mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 3);

    fixture.mc.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    timer_expire(&fixture.time, &fixture.mc.brake_timer, 1.1f);
    size_t current_calls_before_release = vesc_if_fake_mc_set_current_calls();
    size_t duty_calls_before_release = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&fixture.mc, 0.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == current_calls_before_release + 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before_release);

    motor_control_play_click(&fixture.mc);
    EXPECT_EQ_U32(fixture.mc.click_counter, 0u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 0u);

    motor_control_play_tone(&fixture.mc, 250u, 1.0f);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 2u);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 2u);
    motor_control_apply(&fixture.mc, 10.0f, STATE_RUNNING, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 1u);
    motor_control_play_tone(&fixture.mc, 250u, 2.0f);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 1u);
    EXPECT_FLOAT_NEAR(fixture.mc.tone_intensity, 2.0f);
    motor_control_play_tone(&fixture.mc, 500u, 3.0f);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 1u);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 1u);
    EXPECT_FLOAT_NEAR(fixture.mc.tone_intensity, 3.0f);

    return true;
}

typedef struct {
    MotorControl *mc;
    uint16_t frequency;
    float intensity;
} MotorControlToneGuard;

static bool run_motor_control_play_tone(void *ctx) {
    MotorControlToneGuard *guard = ctx;
    motor_control_play_tone(guard->mc, guard->frequency, guard->intensity);
    return true;
}

static bool test_motor_control_zero_tone_frequency(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 6.0f, 0.0f, PARKING_BRAKE_NEVER, 1000u);

    MotorControlToneGuard guard = {.mc = &fixture.mc, .frequency = 0u, .intensity = 1.0f};
    EXPECT_TRUE(test_expect_no_signal(SIGFPE, run_motor_control_play_tone, &guard));

    EXPECT_EQ_U32(fixture.mc.tone_ticks, 0u);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 0u);
    EXPECT_FLOAT_NEAR(fixture.mc.tone_intensity, 0.0f);

    return true;
}

static bool test_motor_control_click_lifecycle_edges(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 5.0f, 2.0f, PARKING_BRAKE_NEVER, 1000u);

    motor_control_play_click(&fixture.mc);
    EXPECT_EQ_U32(fixture.mc.click_counter, 3u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 1u);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 1u);

    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.click_counter, 2u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 1u);
    EXPECT_TRUE(fixture.mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 2.0f);

    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.click_counter, 1u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 1u);
    EXPECT_TRUE(!fixture.mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -2.0f);

    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.click_counter, 0u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 0u);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 0u);
    EXPECT_TRUE(!fixture.mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -2.0f);

    size_t current_calls_after_click = vesc_if_fake_mc_set_current_calls();
    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == current_calls_after_click);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() > 0);

    return true;
}

static bool test_motor_control_parking_and_moving_threshold_edges(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 4.0f, 0.0f, PARKING_BRAKE_ALWAYS, 1000u);

    timer_refresh(&fixture.time, &fixture.mc.brake_timer);
    motor_control_apply(&fixture.mc, 1999.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(fixture.mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

    motor_control_apply(&fixture.mc, 2000.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), fixture.cfg.brake_current);

    fixture.mc.parking_brake_mode = PARKING_BRAKE_NEVER;
    fixture.mc.brake_timer = fixture.time.now - 1u;
    motor_control_apply(&fixture.mc, ERPM_MOVING_THRESHOLD, STATE_READY, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.brake_timer, fixture.time.now - 1u);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 2);

    motor_control_apply(&fixture.mc, ERPM_MOVING_THRESHOLD + 0.1f, STATE_READY, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.brake_timer, fixture.time.now);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 3);

    return true;
}
