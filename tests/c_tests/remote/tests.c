static RefloatConfig default_remote_cfg(void) {
    return (RefloatConfig){
        .inputtilt_remote_type = INPUTTILT_NONE,
        .inputtilt_deadband = 0.2f,
        .inputtilt_angle_limit = 15.0f,
        .remote.max_move_speed = 0.0f,
        .remote.filter.time_constant = 0.1f,
        .remote_throttle_grace_period = 0.0f,
    };
}

typedef struct {
    Time time;
    Remote remote;
} RemoteFixture;

static void remote_fixture_start(RemoteFixture *fixture, uint32_t now_ticks) {
    vesc_if_fake_reset();
    *fixture = (RemoteFixture){0};
    fixture->time = (Time){.now = now_ticks};
    timer_expire(&fixture->time, &fixture->time.disengage_timer, 3.0f);
    remote_init(&fixture->remote, &fixture->time);
}

static bool test_remote_maps_command_input_to_motion_intent(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 10 * SYSTEM_TICK_RATE_HZ);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    RefloatConfig cfg = default_remote_cfg();

    remote_configure(&fixture.remote, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.setpoint.on_speed_up, 100.0f);

    fixture.remote.input = 0.75f;
    fixture.remote.move_speed = 3.0f;
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    fixture.time.now += 3 * SYSTEM_TICK_RATE_HZ;
    timer_expire(&fixture.time, &fixture.time.disengage_timer, 3.0f);
    remote_command_input(&fixture.remote, -0.5f, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, -0.5f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, -2.5f);

    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    vesc_if_fake_set_ppm(1.0f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, -0.5f);

    fixture.time.now += SYSTEM_TICK_RATE_HZ;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_TRUE(fixture.remote.input > 0.9f);

    fixture.remote.move_speed = 50.0f;
    float torque = remote_get_move_torque(&fixture.remote, -50.0f, 1.0f);
    EXPECT_FLOAT_NEAR(torque, 10.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_pid_i, 10.0f);

    fixture.remote.move_speed = -50.0f;
    torque = remote_get_move_torque(&fixture.remote, 50.0f, 1.0f);
    EXPECT_FLOAT_NEAR(torque, -10.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_pid_i, -10.0f);

    fixture.remote.move_speed = NAN;
    EXPECT_TRUE(isnan(remote_get_move_torque(&fixture.remote, 0.0f, 0.1f)));
    EXPECT_FLOAT_NEAR(fixture.remote.move_pid_i, 0.0f);

    remote_command_input(&fixture.remote, 0.5f, &fixture.time, &cfg);
    remote_reset(&fixture.remote, &fixture.time);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.setpoint.value, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    vesc_if_fake_set_ppm(-0.5f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_TRUE(fixture.remote.input < -0.3f);

    return true;
}

static bool test_remote_expires_uart_and_command_input(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 20 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_UART;
    cfg.inputtilt_deadband = 0.25f;
    cfg.inputtilt_angle_limit = 10.0f;
    cfg.remote.max_move_speed = 6.0f;

    vesc_if_fake_set_remote(0.625f, 0.1f);
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.5f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 3.0f);

    remote_command_input(&fixture.remote, -0.25f, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, -0.25f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, -1.5f);

    vesc_if_fake_set_remote(1.0f, 0.1f);
    fixture.time.now += (time_t) (0.5f * SYSTEM_TICK_RATE_HZ);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, -0.25f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, -1.5f);

    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 6.0f);

    vesc_if_fake_set_remote(0.5f, 0.5f);
    fixture.time.now += SYSTEM_TICK_RATE_HZ;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    remote_reset(&fixture.remote, &fixture.time);
    fixture.time.disengage_timer = fixture.time.now;
    cfg.remote_throttle_grace_period = 2.0f;
    vesc_if_fake_set_remote(1.0f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 1.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    timer_expire(&fixture.time, &fixture.time.disengage_timer, 2.0f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 1.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 6.0f);

    fixture.time.disengage_timer = fixture.time.now;
    remote_command_input(&fixture.remote, 0.5f, &fixture.time, &cfg);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    remote_reset(&fixture.remote, &fixture.time);
    fixture.time.disengage_timer = fixture.time.now;
    remote_command_input(&fixture.remote, 0.5f, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.5f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    cfg.inputtilt_invert_throttle = true;
    timer_expire(&fixture.time, &fixture.time.disengage_timer, 2.0f);
    fixture.time.now += 1u;
    remote_command_input(&fixture.remote, 0.5f, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, -0.5f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 3.0f);

    return true;
}

static bool test_remote_revoked_move_config_clears_stale_target(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 25 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_UART;
    cfg.remote.max_move_speed = 6.0f;
    vesc_if_fake_set_remote(1.0f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 6.0f);

    cfg.remote.max_move_speed = 0.0f;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    cfg.remote.max_move_speed = 6.0f;
    cfg.remote_throttle_grace_period = 10.0f;
    fixture.time.disengage_timer = fixture.time.now;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));
    return true;
}

static bool test_remote_applies_deadband_inversion_and_idle_move(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 30 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    cfg.inputtilt_deadband = 0.25f;
    cfg.inputtilt_invert_throttle = true;
    cfg.remote.max_move_speed = 10.0f;

    vesc_if_fake_set_ppm(0.25f, 0.1f);
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    vesc_if_fake_set_ppm(0.4f, 0.1f);
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, -0.2f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 2.0f);

    vesc_if_fake_set_ppm(0.0f, 0.1f);
    fixture.time.now += SYSTEM_TICK_RATE_HZ;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 0.0f);

    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));

    return true;
}

static bool check_remote_boundary_case(
    const char *label,
    uint32_t start_ticks,
    FLOAT_INPUTTILT_REMOTE_TYPE remote_type,
    float input,
    float age_s,
    float deadband,
    float max_move_speed,
    float expected_input,
    float expected_move_speed
) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, start_ticks);
    fixture.time.now += 1u;

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = remote_type;
    cfg.inputtilt_deadband = deadband;
    cfg.remote.max_move_speed = max_move_speed;

    if (remote_type == INPUTTILT_PPM) {
        vesc_if_fake_set_ppm(input, age_s);
    } else {
        vesc_if_fake_set_remote(input, age_s);
    }

    remote_input(&fixture.remote, &fixture.time, &cfg);
    if (!isfinite(fixture.remote.input) ||
        fabsf(fixture.remote.input - expected_input) > TEST_FLOAT_EPS) {
        fprintf(stderr, "Remote input contract: %s\n", label);
        test_report_float_failure(
            __FILE__, __LINE__, label, fixture.remote.input, expected_input, TEST_FLOAT_EPS
        );
        return false;
    }

    if (isnan(expected_move_speed)) {
        if (!isnan(fixture.remote.move_speed)) {
            fprintf(stderr, "Remote input contract: %s\n", label);
            test_report_expr_failure(__FILE__, __LINE__, label);
            return false;
        }
    } else if (!isfinite(fixture.remote.move_speed) ||
               fabsf(fixture.remote.move_speed - expected_move_speed) > TEST_FLOAT_EPS) {
        fprintf(stderr, "Remote input contract: %s\n", label);
        test_report_float_failure(
            __FILE__,
            __LINE__,
            label,
            fixture.remote.move_speed,
            expected_move_speed,
            TEST_FLOAT_EPS
        );
        return false;
    }

    return true;
}

static bool test_remote_maps_input_age_and_deadband(void) {
    static const struct {
        const char *label;
        uint32_t start_ticks;
        FLOAT_INPUTTILT_REMOTE_TYPE remote_type;
        float input;
        float age_s;
        float deadband;
        float max_move_speed;
        float expected_input;
        float expected_move_speed;
    } cases[] = {
        {"PPM input just inside its fresh window",
         40 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_PPM,
         0.5f,
         0.499f,
         0.0f,
         4.0f,
         0.5f,
         2.0f},
        {"PPM input at the stale boundary",
         40 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_PPM,
         0.75f,
         0.5f,
         0.0f,
         4.0f,
         0.0f,
         NAN},
        {"UART input just inside its fresh window",
         40 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_UART,
         -0.5f,
         0.499f,
         0.0f,
         4.0f,
         -0.5f,
         -2.0f},
        {"UART input at the stale boundary",
         40 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_UART,
         -0.75f,
         0.5f,
         0.0f,
         4.0f,
         0.0f,
         NAN},
        {"PPM input at the deadband boundary",
         50 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_PPM,
         0.5f,
         0.1f,
         0.5f,
         8.0f,
         0.0f,
         NAN},
        {"PPM input beyond the deadband boundary",
         50 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_PPM,
         0.75f,
         0.1f,
         0.5f,
         8.0f,
         0.5f,
         4.0f},
        {"UART input at the deadband boundary leaves movement idle",
         50 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_UART,
         -0.5f,
         0.1f,
         0.5f,
         8.0f,
         0.0f,
         NAN},
        {"UART input beyond the deadband boundary",
         50 * SYSTEM_TICK_RATE_HZ,
         INPUTTILT_UART,
         -0.75f,
         0.1f,
         0.5f,
         8.0f,
         -0.5f,
         -4.0f},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        EXPECT_TRUE(check_remote_boundary_case(
            cases[index].label,
            cases[index].start_ticks,
            cases[index].remote_type,
            cases[index].input,
            cases[index].age_s,
            cases[index].deadband,
            cases[index].max_move_speed,
            cases[index].expected_input,
            cases[index].expected_move_speed
        ));
    }

    return true;
}

static bool test_remote_update_respects_darkride(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 65 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_angle_limit = 10.0f;
    remote_configure(&fixture.remote, &cfg, 100.0f);

    fixture.remote.input = 0.5f;
    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_NONE};
    remote_update(&fixture.remote, &state, &cfg, 0.01f);
    EXPECT_TRUE(fixture.remote.setpoint.value > 0.0f);

    remote_reset(&fixture.remote, &fixture.time);
    fixture.remote.input = 0.5f;
    state.darkride = true;
    remote_update(&fixture.remote, &state, &cfg, 0.01f);
    EXPECT_TRUE(fixture.remote.setpoint.value < 0.0f);

    return true;
}

static bool test_remote_rejects_invalid_deadband_config(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 60 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    cfg.inputtilt_deadband = 1.0f;
    cfg.remote.max_move_speed = 8.0f;

    vesc_if_fake_set_ppm(1.0f, 0.1f);
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);

    EXPECT_TRUE(isfinite(fixture.remote.move_speed) || isnan(fixture.remote.move_speed));
    EXPECT_TRUE(fabsf(fixture.remote.input) <= 1.0f);
    if (isfinite(fixture.remote.move_speed)) {
        EXPECT_TRUE(fabsf(fixture.remote.move_speed) <= cfg.remote.max_move_speed);
    }

    cfg.inputtilt_deadband = 1.5f;
    vesc_if_fake_set_ppm(1.0f, 0.1f);
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);

    EXPECT_TRUE(isfinite(fixture.remote.move_speed) || isnan(fixture.remote.move_speed));
    EXPECT_TRUE(fabsf(fixture.remote.input) <= 1.0f);
    if (isfinite(fixture.remote.move_speed)) {
        EXPECT_TRUE(fabsf(fixture.remote.move_speed) <= cfg.remote.max_move_speed);
    }

    return true;
}

static bool test_remote_move_torque_nonfinite_dt(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 90 * SYSTEM_TICK_RATE_HZ);
    fixture.remote.move_speed = 3.0f;
    fixture.remote.move_pid_i = 1.0f;

    float torque = remote_get_move_torque(&fixture.remote, 1.0f, 0.0f);
    EXPECT_TRUE(isfinite(torque));
    EXPECT_TRUE(isfinite(fixture.remote.move_pid_i));
    EXPECT_FLOAT_NEAR(fixture.remote.move_pid_i, 1.0f);

    torque = remote_get_move_torque(&fixture.remote, 1.0f, -0.02f);
    EXPECT_TRUE(isfinite(torque));
    EXPECT_TRUE(isfinite(fixture.remote.move_pid_i));
    EXPECT_FLOAT_NEAR(fixture.remote.move_pid_i, 1.0f);

    // NaN dt is an API-only hostile input; the VESC timer path supplies a
    // finite duration. Keep the guard without treating this as a VESC bug.
    torque = remote_get_move_torque(&fixture.remote, 1.0f, NAN);
    EXPECT_TRUE(isfinite(torque));
    EXPECT_TRUE(isfinite(fixture.remote.move_pid_i));
    EXPECT_FLOAT_NEAR(fixture.remote.move_pid_i, 1.0f);

    return true;
}

static bool test_remote_invalid_input_is_quarantined(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 60 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    vesc_if_fake_set_ppm(NAN, 0.1f);
    fixture.time.now += 1u;
    remote_input(&fixture.remote, &fixture.time, &cfg);

    EXPECT_TRUE(isfinite(fixture.remote.input));
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.0f);
    EXPECT_TRUE(isnan(fixture.remote.move_speed));
    return true;
}

static bool test_remote_rejects_negative_tilt_limit(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 60 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_angle_limit = -10.0f;
    remote_configure(&fixture.remote, &cfg, 100.0f);
    fixture.remote.input = 0.5f;
    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL};
    remote_update(&fixture.remote, &state, &cfg, 1.0f);

    EXPECT_TRUE(fixture.remote.setpoint.value >= 0.0f);
    return true;
}

static bool test_remote_bounds_input_limits(void) {
    RemoteFixture fixture;
    remote_fixture_start(&fixture, 60 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_angle_limit = 100.0f;
    remote_configure(&fixture.remote, &cfg, 100.0f);
    fixture.remote.input = 1.0f;
    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL};
    for (size_t i = 0; i < 100; ++i) {
        remote_update(&fixture.remote, &state, &cfg, 0.1f);
    }

    EXPECT_TRUE(fixture.remote.setpoint.value <= 90.0f);

    cfg.remote.filter.time_constant = 30.0f;
    remote_configure(&fixture.remote, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(
        fixture.remote.setpoint.alpha, 2.146f * ema_calculate_alpha_time_constant(0.5f, 100.0f)
    );

    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    cfg.remote.max_move_speed = UINT8_MAX;
    fixture.time.now += SYSTEM_TICK_RATE_HZ;
    vesc_if_fake_set_ppm(1.0f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 10.0f);

    fixture.time.now += SYSTEM_TICK_RATE_HZ;
    remote_command_input(&fixture.remote, -1.0f, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, -10.0f);

    fixture.time.now += SYSTEM_TICK_RATE_HZ;
    cfg.inputtilt_deadband = 0.9f;
    cfg.remote_throttle_grace_period = 0.0f;
    vesc_if_fake_set_ppm(0.75f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.input, 0.5f);

    remote_reset(&fixture.remote, &fixture.time);
    fixture.time.now += 61 * SYSTEM_TICK_RATE_HZ;
    timer_expire(&fixture.time, &fixture.time.disengage_timer, 61.0f);
    cfg.inputtilt_deadband = 0.0f;
    cfg.remote_throttle_grace_period = 100.0f;
    vesc_if_fake_set_ppm(1.0f, 0.1f);
    remote_input(&fixture.remote, &fixture.time, &cfg);
    EXPECT_FLOAT_NEAR(fixture.remote.move_speed, 10.0f);
    return true;
}
