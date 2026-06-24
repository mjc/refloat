static bool test_motor_control_current_brake_and_tone(void) {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);
    EXPECT_TRUE(isnan(mc.requested_current));
    EXPECT_TRUE(!mc.disabled);

    RefloatConfig cfg = {
        .brake_current = 7.5f,
        .startup_click_current = 2.0f,
        .parking_brake_mode = PARKING_BRAKE_IDLE,
    };
    motor_control_configure(&mc, &cfg, 1000u);
    EXPECT_FLOAT_NEAR(mc.brake_current, 7.5f);
    EXPECT_FLOAT_NEAR(mc.click_current, 2.0f);
    EXPECT_TRUE(mc.main_freq == 500u);

    Time time = {.now = 1000000u};
    motor_control_apply(&mc, 0.0f, STATE_DISABLED, &time);
    EXPECT_TRUE(mc.disabled);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    EXPECT_TRUE(vesc_if_fake_timeout_reset_calls() == 0);

    motor_control_apply(&mc, 0.0f, STATE_DISABLED, &time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == 1);

    motor_control_request_current(&mc, 4.25f);
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_TRUE(!mc.disabled);
    EXPECT_TRUE(isnan(mc.requested_current));
    EXPECT_TRUE(vesc_if_fake_timeout_reset_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_off_delay_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current_off_delay(), 0.05f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 4.25f);

    motor_control_apply(&mc, 3000.0f, STATE_RUNNING, &time);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), 7.5f);

    time.now += 2000000u;
    motor_control_apply(&mc, 0.0f, STATE_RUNNING, &time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);

    mc.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    timer_refresh(&time, &mc.brake_timer);
    motor_control_apply(&mc, 0.0f, STATE_RUNNING, &time);
    EXPECT_TRUE(mc.parking_brake_active);
    size_t duty_calls_before = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&mc, 0.0f, STATE_STARTUP, &time);
    EXPECT_TRUE(mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before + 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

    motor_control_play_tone(&mc, 250u, 1.5f);
    EXPECT_TRUE(mc.tone_ticks == 2u);
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -1.5f);
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 1.5f);

    motor_control_stop_tone(&mc);
    EXPECT_TRUE(mc.tone_ticks == 0u);
    EXPECT_TRUE(mc.tone_counter == 0u);

    motor_control_play_click(&mc);
    EXPECT_TRUE(mc.click_counter == 3u);
    EXPECT_TRUE(mc.tone_ticks > 0u);

    return true;
}

static bool test_motor_control_parking_and_tone_edges(void) {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg = {
        .brake_current = 6.0f,
        .startup_click_current = 0.0f,
        .parking_brake_mode = PARKING_BRAKE_NEVER,
    };
    motor_control_configure(&mc, &cfg, 1000u);

    Time time = {.now = 1000000u};
    timer_refresh(&time, &mc.brake_timer);
    motor_control_apply(&mc, 0.0f, STATE_READY, &time);
    EXPECT_TRUE(!mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), cfg.brake_current);

    cfg.parking_brake_mode = PARKING_BRAKE_IDLE;
    motor_control_configure(&mc, &cfg, 1000u);
    motor_control_apply(&mc, 100.0f, STATE_READY, &time);
    EXPECT_TRUE(!mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 2);

    motor_control_apply(&mc, 25.0f, STATE_READY, &time);
    EXPECT_TRUE(mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == 1);

    motor_control_apply(&mc, 25.0f, STATE_RUNNING, &time);
    EXPECT_TRUE(!mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 3);

    mc.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    timer_expire(&time, &mc.brake_timer, 1.1f);
    size_t current_calls_before_release = vesc_if_fake_mc_set_current_calls();
    size_t duty_calls_before_release = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&mc, 0.0f, STATE_READY, &time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == current_calls_before_release + 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before_release);

    motor_control_play_click(&mc);
    EXPECT_EQ_U32(mc.click_counter, 0u);
    EXPECT_EQ_U32(mc.tone_ticks, 0u);

    motor_control_play_tone(&mc, 250u, 1.0f);
    EXPECT_EQ_U32(mc.tone_ticks, 2u);
    EXPECT_EQ_U32(mc.tone_counter, 2u);
    motor_control_apply(&mc, 10.0f, STATE_RUNNING, &time);
    EXPECT_EQ_U32(mc.tone_counter, 1u);
    motor_control_play_tone(&mc, 250u, 2.0f);
    EXPECT_EQ_U32(mc.tone_counter, 1u);
    EXPECT_FLOAT_NEAR(mc.tone_intensity, 2.0f);
    motor_control_play_tone(&mc, 500u, 3.0f);
    EXPECT_EQ_U32(mc.tone_ticks, 1u);
    EXPECT_EQ_U32(mc.tone_counter, 1u);
    EXPECT_FLOAT_NEAR(mc.tone_intensity, 3.0f);

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
    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg = {
        .brake_current = 6.0f,
        .startup_click_current = 0.0f,
        .parking_brake_mode = PARKING_BRAKE_NEVER,
    };
    motor_control_configure(&mc, &cfg, 1000u);

    MotorControlToneGuard guard = {.mc = &mc, .frequency = 0u, .intensity = 1.0f};
    EXPECT_TRUE(test_expect_no_signal(SIGFPE, run_motor_control_play_tone, &guard));

    EXPECT_EQ_U32(mc.tone_ticks, 0u);
    EXPECT_EQ_U32(mc.tone_counter, 0u);
    EXPECT_FLOAT_NEAR(mc.tone_intensity, 0.0f);

    return true;
}

static bool test_motor_control_click_lifecycle_edges(void) {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg = {
        .brake_current = 5.0f,
        .startup_click_current = 2.0f,
        .parking_brake_mode = PARKING_BRAKE_NEVER,
    };
    motor_control_configure(&mc, &cfg, 1000u);

    Time time = {.now = 1000000u};
    motor_control_play_click(&mc);
    EXPECT_EQ_U32(mc.click_counter, 3u);
    EXPECT_EQ_U32(mc.tone_ticks, 1u);
    EXPECT_EQ_U32(mc.tone_counter, 1u);

    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_EQ_U32(mc.click_counter, 2u);
    EXPECT_EQ_U32(mc.tone_ticks, 1u);
    EXPECT_TRUE(mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 2.0f);

    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_EQ_U32(mc.click_counter, 1u);
    EXPECT_EQ_U32(mc.tone_ticks, 1u);
    EXPECT_TRUE(!mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -2.0f);

    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_EQ_U32(mc.click_counter, 0u);
    EXPECT_EQ_U32(mc.tone_ticks, 0u);
    EXPECT_EQ_U32(mc.tone_counter, 0u);
    EXPECT_TRUE(!mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -2.0f);

    size_t current_calls_after_click = vesc_if_fake_mc_set_current_calls();
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == current_calls_after_click);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() > 0);

    return true;
}

static bool test_motor_control_parking_and_moving_threshold_edges(void) {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg = {
        .brake_current = 4.0f,
        .startup_click_current = 0.0f,
        .parking_brake_mode = PARKING_BRAKE_ALWAYS,
    };
    motor_control_configure(&mc, &cfg, 1000u);

    Time time = {.now = 1000000u};
    timer_refresh(&time, &mc.brake_timer);
    motor_control_apply(&mc, 1999.0f, STATE_READY, &time);
    EXPECT_TRUE(mc.parking_brake_active);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

    motor_control_apply(&mc, 2000.0f, STATE_READY, &time);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), cfg.brake_current);

    mc.parking_brake_mode = PARKING_BRAKE_NEVER;
    mc.brake_timer = time.now - 1u;
    motor_control_apply(&mc, ERPM_MOVING_THRESHOLD, STATE_READY, &time);
    EXPECT_EQ_U32(mc.brake_timer, time.now - 1u);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 2);

    motor_control_apply(&mc, ERPM_MOVING_THRESHOLD + 0.1f, STATE_READY, &time);
    EXPECT_EQ_U32(mc.brake_timer, time.now);
    EXPECT_TRUE(vesc_if_fake_mc_set_brake_current_calls() == 3);

    return true;
}
