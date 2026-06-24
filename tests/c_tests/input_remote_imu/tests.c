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

static bool test_footpad_sensor(void) {
    vesc_if_fake_reset();
    FootpadSensor fs = {
        .adc_left = 9.0f,
        .adc_right = 8.0f,
        .state = FS_BOTH,
    };
    footpad_sensor_init(&fs);

    EXPECT_FLOAT_NEAR(fs.adc_left, 0.0f);
    EXPECT_FLOAT_NEAR(fs.adc_right, 0.0f);
    EXPECT_TRUE(fs.state == FS_NONE);

    RefloatConfig cfg = {
        .fault_adc1 = 1.0f,
        .fault_adc2 = 2.0f,
    };

    struct {
        float adc1;
        float adc2;
        FootpadSensorState state;
        float expected_left;
        float expected_right;
    } cases[] = {
        {0.5f, 2.5f, FS_RIGHT, 0.5f, 2.5f},
        {1.0f, 2.0f, FS_NONE, 1.0f, 2.0f},
        {-1.0f, -1.0f, FS_NONE, fs.adc_left, fs.adc_right},
        {1.5f, 2.5f, FS_BOTH, 1.5f, 2.5f},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        vesc_if_fake_set_analog(cases[i].adc1, cases[i].adc2);
        footpad_sensor_update(&fs, &cfg);
        EXPECT_TRUE(fs.state == cases[i].state);
        if (cases[i].adc1 >= 0.0f && cases[i].adc2 >= 0.0f) {
            EXPECT_FLOAT_NEAR(fs.adc_left, cases[i].expected_left);
            EXPECT_FLOAT_NEAR(fs.adc_right, cases[i].expected_right);
        }
    }

    cfg.hardware.swap_footpad_adcs = true;
    struct {
        float adc1;
        float adc2;
        FootpadSensorState state;
        float expected_left;
        float expected_right;
    } swapped_cases[] = {
        {0.5f, 2.5f, FS_LEFT, 2.5f, 0.5f},
        {1.0f, 2.0f, FS_NONE, 2.0f, 1.0f},
        {1.1f, 2.1f, FS_BOTH, 2.1f, 1.1f},
    };

    for (size_t i = 0; i < sizeof(swapped_cases) / sizeof(swapped_cases[0]); ++i) {
        vesc_if_fake_set_analog(swapped_cases[i].adc1, swapped_cases[i].adc2);
        footpad_sensor_update(&fs, &cfg);
        EXPECT_TRUE(fs.state == swapped_cases[i].state);
        EXPECT_FLOAT_NEAR(fs.adc_left, swapped_cases[i].expected_left);
        EXPECT_FLOAT_NEAR(fs.adc_right, swapped_cases[i].expected_right);
    }

    cfg.fault_adc1 = 0.0f;
    cfg.fault_adc2 = 0.0f;
    footpad_sensor_update(&fs, &cfg);
    EXPECT_TRUE(fs.state == FS_BOTH);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_NONE) == 0);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_LEFT) == 1);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_RIGHT) == 1);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_BOTH) == 2);

    return true;
}

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

static bool test_imu_update_edges(void) {
    vesc_if_fake_reset();

    IMU imu = {
        .pitch = 9.0f,
        .balance_pitch = 8.0f,
        .roll = 7.0f,
        .yaw = 6.0f,
        .pitch_rate = 5.0f,
        .flywheel_pitch_offset = 4.0f,
        .flywheel_roll_offset = 3.0f,
    };
    imu_init(&imu);
    EXPECT_FLOAT_NEAR(imu.pitch, 0.0f);
    EXPECT_FLOAT_NEAR(imu.balance_pitch, 0.0f);
    EXPECT_FLOAT_NEAR(imu.roll, 0.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, 0.0f);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, 0.0f);
    EXPECT_FLOAT_NEAR(imu.flywheel_pitch_offset, 0.0f);
    EXPECT_FLOAT_NEAR(imu.flywheel_roll_offset, 0.0f);

    BalanceFilterData bf = {0};
    State state = {.mode = MODE_NORMAL, .darkride = false};
    vesc_if_fake_set_imu(0.0f, deg2rad(60.0f), 0.0f, 0.0f, 2.0f, 4.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, 2.232051f);
    EXPECT_FLOAT_NEAR(imu.pitch, 0.0f);
    EXPECT_FLOAT_NEAR(imu.roll, 60.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, 0.0f);

    bf.q0 = cosf(deg2rad(10.0f) * 0.5f);
    bf.q2 = sinf(deg2rad(10.0f) * 0.5f);
    vesc_if_fake_set_imu(deg2rad(12.0f), deg2rad(30.0f), deg2rad(-15.0f), 0.0f, 3.0f, -2.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch, 12.0f);
    EXPECT_FLOAT_NEAR(imu.balance_pitch, 10.0f);
    EXPECT_FLOAT_NEAR(imu.roll, 30.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, -15.0f);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, 1.383975f);

    state.darkride = true;
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, -1.383975f);

    imu.pitch = 12.0f;
    imu.roll = -34.0f;
    imu_set_flywheel_offsets(&imu);
    EXPECT_FLOAT_NEAR(imu.flywheel_pitch_offset, 12.0f);
    EXPECT_FLOAT_NEAR(imu.flywheel_roll_offset, -34.0f);

    state.darkride = false;
    state.mode = MODE_FLYWHEEL;
    imu.flywheel_pitch_offset = 5.0f;
    imu.flywheel_roll_offset = 0.0f;

    vesc_if_fake_set_imu(deg2rad(2.0f), deg2rad(250.0f), deg2rad(15.0f), 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch, 3.0f);
    EXPECT_FLOAT_NEAR(imu.balance_pitch, imu.pitch);
    EXPECT_FLOAT_NEAR(imu.roll, -110.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, 15.0f);

    vesc_if_fake_set_imu(deg2rad(2.0f), deg2rad(-250.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, 110.0f);

    return true;
}

static bool test_imu_flywheel_roll_wrap_boundaries(void) {
    vesc_if_fake_reset();

    IMU imu;
    imu_init(&imu);
    BalanceFilterData bf = {0};
    State state = {.mode = MODE_FLYWHEEL, .darkride = false};
    imu.flywheel_pitch_offset = 0.0f;
    imu.flywheel_roll_offset = 0.0f;

    vesc_if_fake_set_imu(0.0f, deg2rad(-200.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, -200.0f);

    vesc_if_fake_set_imu(0.0f, deg2rad(-200.1f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, 159.9f);

    vesc_if_fake_set_imu(0.0f, deg2rad(200.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, 200.0f);

    vesc_if_fake_set_imu(0.0f, deg2rad(200.1f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, -159.9f);

    imu.flywheel_roll_offset = 40.0f;
    vesc_if_fake_set_imu(0.0f, deg2rad(240.1f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, -159.9f);

    return true;
}
