typedef struct {
    MotorControl mc;
    RefloatConfig cfg;
    Time time;
} MotorControlFixture;

typedef struct {
    MotorControl mc;
    MotorData md;
    PID pid;
    IMU imu;
    RefloatConfig cfg;
    State state;
    Time time;
    EMA balance_current;
    float softstart_pid_limit;
    bool traction_control;
} BalanceControlFixture;

static MotorControlFixture motor_control_fixture_start(void) {
    vesc_if_fake_reset();

    MotorControlFixture fixture = {0};
    motor_control_init(&fixture.mc);
    fixture.time.now = 1000000u;
    return fixture;
}

static void motor_control_fixture_configure(
    MotorControlFixture *fixture,
    float brake_current,
    float startup_click_current,
    ParkingBrakeMode parking_brake_mode,
    uint32_t main_freq
) {
    fixture->cfg = (RefloatConfig){
        .brake_current = brake_current,
        .startup_click_current = startup_click_current,
        .parking_brake_mode = parking_brake_mode,
    };
    motor_control_configure(&fixture->mc, &fixture->cfg, main_freq);
}

static BalanceControlFixture balance_control_fixture_start(void) {
    vesc_if_fake_reset();

    BalanceControlFixture fixture = {0};
    motor_control_init(&fixture.mc);
    motor_data_init(&fixture.md);
    pid_init(&fixture.pid);
    imu_init(&fixture.imu);
    ema_init(&fixture.balance_current);
    fixture.balance_current.alpha = 1.0f;
    fixture.state = (State){.state = STATE_RUNNING, .mode = MODE_NORMAL, .darkride = false};
    fixture.time.now = 1000000u;
    fixture.md.speed_constant = 1.0f / TORQUE_CONSTANT_COMPAT;
    fixture.md.current_min = 5.0f;
    fixture.md.current_max = 20.0f;
    fixture.md.braking = false;
    fixture.softstart_pid_limit = 0.0f;

    fixture.cfg = (RefloatConfig){
        .kp = 2.0f,
        .ki = 0.0f,
        .kp2 = 0.0f,
        .ki_limit = 0.0f,
        .kp_brake = 1.0f,
        .kp2_brake = 1.0f,
        .brake_current = 6.0f,
        .startup_click_current = 0.0f,
        .parking_brake_mode = PARKING_BRAKE_NEVER,
    };

    return fixture;
}

static float balance_control_request_current(
    BalanceControlFixture *fixture, float setpoint, float dt
) {
    pid_update(&fixture->pid, setpoint, &fixture->md, &fixture->imu, &fixture->cfg, dt);

    float pitch_based = motor_data_torque_to_current(&fixture->md, fixture->pid.rate_p);
    if (fixture->softstart_pid_limit < fixture->md.current_max) {
        pitch_based = fminf(fabsf(pitch_based), fixture->softstart_pid_limit) * sign(pitch_based);
        fixture->softstart_pid_limit += 100.0f * dt;
    }

    float new_current =
        motor_data_torque_to_current(&fixture->md, fixture->pid.p + fixture->pid.i) + pitch_based;
    float current_limit;

    if (fixture->state.mode == MODE_HANDTEST) {
        current_limit = 7.0f;
    } else if (fixture->state.mode == MODE_FLYWHEEL) {
        current_limit = 40.0f;
    } else {
        current_limit = fixture->md.braking ? fixture->md.current_min : fixture->md.current_max;
    }

    if (fabsf(new_current) > current_limit) {
        new_current = sign(new_current) * current_limit;
    }

    if (fixture->state.darkride) {
        new_current = -new_current;
    }

    if (fixture->traction_control) {
        ema_reset(&fixture->balance_current, 0.0f);
    } else {
        ema_update(&fixture->balance_current, new_current);
    }

    motor_control_request_current(&fixture->mc, fixture->balance_current.value);
    motor_control_apply(
        &fixture->mc, fixture->md.abs_erpm_smooth.value, fixture->state.state, &fixture->time
    );
    return new_current;
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

static bool test_motor_control_handles_parking_and_tone_timing(void) {
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
    size_t current_calls_before_hold = vesc_if_fake_mc_set_current_calls();
    size_t duty_calls_before_hold = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&fixture.mc, 0.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == current_calls_before_hold);
    EXPECT_TRUE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before_hold + 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

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

static bool test_motor_control_invalid_parking_mode_fails_open(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 6.0f, 0.0f, PARKING_BRAKE_ALWAYS, 1000u);
    motor_control_apply(&fixture.mc, 0.0f, STATE_READY, &fixture.time);
    EXPECT_TRUE(fixture.mc.parking_brake_active);

    fixture.cfg.parking_brake_mode = (ParkingBrakeMode) 255;
    motor_control_configure(&fixture.mc, &fixture.cfg, 1000u);
    motor_control_apply(&fixture.mc, 0.0f, STATE_READY, &fixture.time);

    EXPECT_TRUE(!fixture.mc.parking_brake_active);
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

static bool test_motor_control_saturates_long_tone_period(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 6.0f, 0.0f, PARKING_BRAKE_NEVER, 1000u);

    motor_control_play_tone(&fixture.mc, 1u, 1.0f);

    EXPECT_EQ_U32(fixture.mc.tone_ticks, UINT8_MAX);
    EXPECT_EQ_U32(fixture.mc.tone_counter, UINT8_MAX);
    return true;
}

static bool test_motor_control_completes_click_lifecycle(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 5.0f, 2.0f, PARKING_BRAKE_NEVER, 1000u);

    motor_control_play_click(&fixture.mc);
    EXPECT_EQ_U32(fixture.mc.click_counter, 3u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 1u);
    EXPECT_EQ_U32(fixture.mc.tone_counter, 1u);
    fixture.mc.requested_current = 1.0f;

    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_EQ_U32(fixture.mc.click_counter, 2u);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 1u);
    EXPECT_TRUE(fixture.mc.tone_high);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 3.0f);

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

static bool test_motor_control_switches_parking_at_motion_threshold(void) {
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

static bool test_motor_control_bounds_configured_currents(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_fixture_configure(&fixture, 200.0f, 50.0f, PARKING_BRAKE_NEVER, 1000u);

    motor_control_apply(&fixture.mc, 3000.0f, STATE_READY, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_brake_current(), 100.0f);

    motor_control_play_click(&fixture.mc);
    motor_control_apply(&fixture.mc, 100.0f, STATE_RUNNING, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 20.0f);

    return true;
}

static bool test_motor_control_disable_discards_pending_current(void) {
    MotorControlFixture fixture = motor_control_fixture_start();
    motor_control_request_current(&fixture.mc, 12.0f);

    motor_control_apply(&fixture.mc, 0.0f, STATE_DISABLED, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);

    motor_control_apply(&fixture.mc, 0.0f, STATE_READY, &fixture.time);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == 1u);
    return true;
}

static bool test_balance_control_nominal_request_through_fake_vesc(void) {
    BalanceControlFixture fixture = balance_control_fixture_start();
    fixture.imu.balance_pitch = 6.0f;

    balance_control_request_current(&fixture, 10.0f, 0.01f);

    EXPECT_TRUE(vesc_if_fake_timeout_reset_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_mc_set_current_calls() == 1);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 8.0f);
    EXPECT_FLOAT_NEAR(fixture.balance_current.value, 8.0f);

    return true;
}

static bool test_balance_control_current_limits_and_modes(void) {
    BalanceControlFixture fixture = balance_control_fixture_start();
    fixture.imu.balance_pitch = 0.0f;
    fixture.cfg.kp = 10.0f;
    fixture.cfg.kp2 = 0.0f;
    fixture.md.current_max = 18.0f;
    fixture.md.current_min = 5.0f;

    balance_control_request_current(&fixture, 10.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 18.0f);

    fixture.state.mode = MODE_HANDTEST;
    balance_control_request_current(&fixture, 10.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 7.0f);

    fixture.state.mode = MODE_FLYWHEEL;
    balance_control_request_current(&fixture, -10.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -40.0f);

    return true;
}

static bool test_balance_control_darkride_and_traction_control(void) {
    BalanceControlFixture fixture = balance_control_fixture_start();
    fixture.imu.balance_pitch = 0.0f;
    fixture.cfg.kp = 10.0f;
    fixture.md.current_max = 12.0f;
    fixture.state.darkride = true;

    balance_control_request_current(&fixture, 10.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), -12.0f);

    fixture.state.darkride = false;
    fixture.traction_control = true;
    balance_control_request_current(&fixture, 10.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);

    return true;
}

static bool test_balance_control_softstart_limits_pitch_based_current(void) {
    BalanceControlFixture fixture = balance_control_fixture_start();
    fixture.imu.balance_pitch = 0.0f;
    fixture.imu.pitch_rate = -10.0f;
    fixture.cfg.kp = 0.0f;
    fixture.cfg.kp2 = 3.0f;
    fixture.md.current_max = 20.0f;
    fixture.softstart_pid_limit = 3.0f;

    balance_control_request_current(&fixture, 0.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 3.0f);
    EXPECT_FLOAT_NEAR(fixture.softstart_pid_limit, 4.0f);

    balance_control_request_current(&fixture, 0.0f, 0.01f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_current(), 4.0f);
    EXPECT_FLOAT_NEAR(fixture.softstart_pid_limit, 5.0f);

    return true;
}

static bool test_balance_control_invalid_balance_estimate_reaches_motor_command(void) {
    BalanceControlFixture fixture = balance_control_fixture_start();
    fixture.imu.balance_pitch = NAN;
    fixture.imu.pitch_rate = 0.0f;
    fixture.md.abs_erpm_smooth.value = 100.0f;

    balance_control_request_current(&fixture, 10.0f, 0.01f);

    EXPECT_TRUE(isfinite(fixture.balance_current.value));
    return true;
}
