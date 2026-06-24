static RefloatConfig default_remote_cfg(void) {
    return (RefloatConfig) {
        .inputtilt_remote_type = INPUTTILT_NONE,
        .inputtilt_deadband = 0.2f,
        .inputtilt_angle_limit = 15.0f,
        .remote.max_move_speed = 0.0f,
        .remote.filter.time_constant = 0.1f,
        .remote_throttle_grace_period = 0.0f,
    };
}

static void init_remote_fixture(Time *time, Remote *remote, uint32_t now_ticks) {
    vesc_if_fake_reset();
    *time = (Time) {.now = now_ticks};
    timer_expire(time, &time->disengage_timer, 3.0f);
    remote_init(remote, time);
}

static bool test_remote_branch_cases(void) {
    Time time;
    Remote remote;
    init_remote_fixture(&time, &remote, 10 * SYSTEM_TICK_RATE_HZ);
    EXPECT_FLOAT_NEAR(remote.input, 0.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    RefloatConfig cfg = default_remote_cfg();

    remote_configure(&remote, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(remote.setpoint.on_speed_up, 100.0f);

    remote.input = 0.75f;
    remote.move_speed = 3.0f;
    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    time.now += 3 * SYSTEM_TICK_RATE_HZ;
    timer_expire(&time, &time.disengage_timer, 3.0f);
    remote_command_input(&remote, -0.5f, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, -0.5f);
    EXPECT_FLOAT_NEAR(remote.move_speed, -2.5f);

    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    vesc_if_fake_set_ppm(1.0f, 0.1f);
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, -0.5f);

    time.now += SYSTEM_TICK_RATE_HZ;
    remote_input(&remote, &time, &cfg);
    EXPECT_TRUE(remote.input > 0.9f);

    remote.move_speed = 50.0f;
    float torque = remote_get_move_torque(&remote, -50.0f, 1.0f);
    EXPECT_FLOAT_NEAR(torque, 10.0f);
    EXPECT_FLOAT_NEAR(remote.move_pid_i, 10.0f);

    remote.move_speed = -50.0f;
    torque = remote_get_move_torque(&remote, 50.0f, 1.0f);
    EXPECT_FLOAT_NEAR(torque, -10.0f);
    EXPECT_FLOAT_NEAR(remote.move_pid_i, -10.0f);

    remote.move_speed = NAN;
    EXPECT_TRUE(isnan(remote_get_move_torque(&remote, 0.0f, 0.1f)));
    EXPECT_FLOAT_NEAR(remote.move_pid_i, 0.0f);

    remote_reset(&remote, &time);
    EXPECT_FLOAT_NEAR(remote.setpoint.value, 0.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    return true;
}

static bool test_remote_uart_and_command_timeout_edges(void) {
    Time time;
    Remote remote;
    init_remote_fixture(&time, &remote, 20 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_UART;
    cfg.inputtilt_deadband = 0.25f;
    cfg.inputtilt_angle_limit = 10.0f;
    cfg.remote.max_move_speed = 6.0f;

    vesc_if_fake_set_remote(0.625f, 0.1f);
    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.5f);
    EXPECT_FLOAT_NEAR(remote.move_speed, 3.0f);

    remote_command_input(&remote, -0.25f, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, -0.25f);
    EXPECT_FLOAT_NEAR(remote.move_speed, -1.5f);

    vesc_if_fake_set_remote(1.0f, 0.1f);
    time.now += (time_t) (0.5f * SYSTEM_TICK_RATE_HZ);
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, -0.25f);
    EXPECT_FLOAT_NEAR(remote.move_speed, -1.5f);

    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 1.0f);
    EXPECT_FLOAT_NEAR(remote.move_speed, 6.0f);

    vesc_if_fake_set_remote(0.5f, 0.5f);
    time.now += SYSTEM_TICK_RATE_HZ;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    remote_reset(&remote, &time);
    time.disengage_timer = time.now;
    cfg.remote_throttle_grace_period = 2.0f;
    vesc_if_fake_set_remote(1.0f, 0.1f);
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 1.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    timer_expire(&time, &time.disengage_timer, 2.0f);
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 1.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 1.0f);
    EXPECT_FLOAT_NEAR(remote.move_speed, 6.0f);

    remote_reset(&remote, &time);
    time.disengage_timer = time.now;
    remote_command_input(&remote, 0.5f, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.5f);
    EXPECT_TRUE(isnan(remote.move_speed));

    return true;
}

static bool test_remote_deadband_invert_and_idle_move_edges(void) {
    Time time;
    Remote remote;
    init_remote_fixture(&time, &remote, 30 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    cfg.inputtilt_deadband = 0.25f;
    cfg.inputtilt_invert_throttle = true;
    cfg.remote.max_move_speed = 10.0f;

    vesc_if_fake_set_ppm(0.25f, 0.1f);
    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    vesc_if_fake_set_ppm(0.4f, 0.1f);
    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, -0.2f);
    EXPECT_FLOAT_NEAR(remote.move_speed, 2.0f);

    vesc_if_fake_set_ppm(0.0f, 0.1f);
    time.now += SYSTEM_TICK_RATE_HZ;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.0f);
    EXPECT_FLOAT_NEAR(remote.move_speed, 0.0f);

    time.now += 1u;
    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, 0.0f);
    EXPECT_TRUE(isnan(remote.move_speed));

    return true;
}

static bool check_remote_boundary_case(
    uint32_t start_ticks,
    FLOAT_INPUTTILT_REMOTE_TYPE remote_type,
    float input,
    float age_s,
    float deadband,
    float max_move_speed,
    float expected_input,
    float expected_move_speed
) {
    Time time;
    Remote remote;
    init_remote_fixture(&time, &remote, start_ticks);
    time.now += 1u;

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = remote_type;
    cfg.inputtilt_deadband = deadband;
    cfg.remote.max_move_speed = max_move_speed;

    if (remote_type == INPUTTILT_PPM) {
        vesc_if_fake_set_ppm(input, age_s);
    } else {
        vesc_if_fake_set_remote(input, age_s);
    }

    remote_input(&remote, &time, &cfg);
    EXPECT_FLOAT_NEAR(remote.input, expected_input);
    if (isnan(expected_move_speed)) {
        EXPECT_TRUE(isnan(remote.move_speed));
    } else {
        EXPECT_FLOAT_NEAR(remote.move_speed, expected_move_speed);
    }

    return true;
}

static bool test_remote_deadband_and_age_boundaries(void) {
    EXPECT_TRUE(check_remote_boundary_case(40 * SYSTEM_TICK_RATE_HZ, INPUTTILT_PPM, 0.5f, 0.499f, 0.0f, 4.0f, 0.5f, 2.0f));
    EXPECT_TRUE(check_remote_boundary_case(40 * SYSTEM_TICK_RATE_HZ, INPUTTILT_PPM, 0.75f, 0.5f, 0.0f, 4.0f, 0.0f, NAN));
    EXPECT_TRUE(check_remote_boundary_case(40 * SYSTEM_TICK_RATE_HZ, INPUTTILT_UART, -0.5f, 0.499f, 0.0f, 4.0f, -0.5f, -2.0f));
    EXPECT_TRUE(check_remote_boundary_case(40 * SYSTEM_TICK_RATE_HZ, INPUTTILT_UART, -0.75f, 0.5f, 0.0f, 4.0f, 0.0f, NAN));
    EXPECT_TRUE(check_remote_boundary_case(50 * SYSTEM_TICK_RATE_HZ, INPUTTILT_PPM, 0.5f, 0.1f, 0.5f, 8.0f, 0.0f, NAN));
    EXPECT_TRUE(check_remote_boundary_case(50 * SYSTEM_TICK_RATE_HZ, INPUTTILT_PPM, 0.75f, 0.1f, 0.5f, 8.0f, 0.5f, 4.0f));
    EXPECT_TRUE(check_remote_boundary_case(50 * SYSTEM_TICK_RATE_HZ, INPUTTILT_UART, -0.5f, 0.1f, 0.5f, 8.0f, 0.0f, 0.0f));
    EXPECT_TRUE(check_remote_boundary_case(50 * SYSTEM_TICK_RATE_HZ, INPUTTILT_UART, -0.75f, 0.1f, 0.5f, 8.0f, -0.5f, -4.0f));
    return true;
}

static bool test_remote_rejects_invalid_deadband_config(void) {
    Time time;
    Remote remote;
    init_remote_fixture(&time, &remote, 60 * SYSTEM_TICK_RATE_HZ);

    RefloatConfig cfg = default_remote_cfg();
    cfg.inputtilt_remote_type = INPUTTILT_PPM;
    cfg.inputtilt_deadband = 1.0f;
    cfg.remote.max_move_speed = 8.0f;

    vesc_if_fake_set_ppm(1.0f, 0.1f);
    time.now += 1u;
    remote_input(&remote, &time, &cfg);

    EXPECT_TRUE(isfinite(remote.input));
    EXPECT_TRUE(isfinite(remote.move_speed) || isnan(remote.move_speed));
    EXPECT_TRUE(fabsf(remote.input) <= 1.0f);
    if (isfinite(remote.move_speed)) {
        EXPECT_TRUE(fabsf(remote.move_speed) <= cfg.remote.max_move_speed);
    }

    cfg.inputtilt_deadband = 1.5f;
    vesc_if_fake_set_ppm(1.0f, 0.1f);
    time.now += 1u;
    remote_input(&remote, &time, &cfg);

    EXPECT_TRUE(isfinite(remote.input));
    EXPECT_TRUE(isfinite(remote.move_speed) || isnan(remote.move_speed));
    EXPECT_TRUE(fabsf(remote.input) <= 1.0f);
    if (isfinite(remote.move_speed)) {
        EXPECT_TRUE(fabsf(remote.move_speed) <= cfg.remote.max_move_speed);
    }

    return true;
}

static bool test_remote_move_torque_nonfinite_dt(void) {
    Time time;
    Remote remote;
    init_remote_fixture(&time, &remote, 90 * SYSTEM_TICK_RATE_HZ);
    remote.move_speed = 3.0f;
    remote.move_pid_i = 1.0f;

    float torque = remote_get_move_torque(&remote, 1.0f, 0.0f);
    EXPECT_TRUE(isfinite(torque));
    EXPECT_TRUE(isfinite(remote.move_pid_i));
    EXPECT_FLOAT_NEAR(remote.move_pid_i, 1.0f);

    torque = remote_get_move_torque(&remote, 1.0f, -0.02f);
    EXPECT_TRUE(isfinite(torque));
    EXPECT_TRUE(isfinite(remote.move_pid_i));
    EXPECT_FLOAT_NEAR(remote.move_pid_i, 1.0f);

    torque = remote_get_move_torque(&remote, 1.0f, NAN);
    EXPECT_TRUE(isfinite(torque));
    EXPECT_TRUE(isfinite(remote.move_pid_i));
    EXPECT_FLOAT_NEAR(remote.move_pid_i, 1.0f);

    return true;
}
