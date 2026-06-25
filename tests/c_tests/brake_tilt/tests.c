static RefloatConfig default_brake_tilt_cfg(void) {
    return (RefloatConfig) {
        .braketilt_strength = 10.0f,
        .braketilt_lingering = 1.0f,
        .atr.filter.time_constant = 0.1f,
        .atr.filter.on_speed_time_constant = 0.1f,
        .atr.filter.off_speed_time_constant = 0.1f,
        .atr.filter.on_speed_limit = 100.0f,
        .atr.filter.off_speed_limit = 50.0f,
    };
}

static RefloatConfig default_brake_tilt_booster_cfg(void) {
    return (RefloatConfig) {
        .booster_current = 10.0f,
        .booster_angle = 2.0f,
        .booster_ramp = 2.0f,
        .brkbooster_current = 5.0f,
        .brkbooster_angle = 2.0f,
        .brkbooster_ramp = 2.0f,
    };
}

static bool test_booster_and_brake_tilt_branch_cases(void) {
    Booster booster;
    booster_init(&booster);
    booster_configure(&booster, 100.0f);

    RefloatConfig cfg = default_brake_tilt_booster_cfg();

    MotorData md = {.abs_erpm = 0.0f, .braking = false};
    booster_update(&booster, &md, &cfg, 2.5f);
    float ramped_accel = booster.torque.value;
    EXPECT_TRUE(ramped_accel > 0.0f);
    EXPECT_TRUE(ramped_accel < cfg.booster_current * TORQUE_CONSTANT_COMPAT);

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, 1.25f);
    EXPECT_TRUE(booster.torque.value > 0.0f);

    booster_reset(&booster);
    md.braking = true;
    md.abs_erpm = 0.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    float low_speed_brake = booster.torque.value;

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    EXPECT_TRUE(booster.torque.value < low_speed_brake);

    BrakeTilt bt;
    brake_tilt_init(&bt);
    cfg.braketilt_strength = 0.0f;
    cfg.braketilt_lingering = 1.0f;
    cfg.atr.filter.time_constant = 0.1f;
    cfg.atr.filter.on_speed_time_constant = 0.1f;
    cfg.atr.filter.off_speed_time_constant = 0.1f;
    cfg.atr.filter.on_speed_limit = 100.0f;
    cfg.atr.filter.off_speed_limit = 50.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(bt.factor, 0.0f);

    ATR atr = {0};
    md = (MotorData) {
        .braking = true,
        .abs_erpm = 3000.0f,
        .erpm = 3000.0f,
        .erpm_sign = 1,
        .forward = true,
    };
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);

    cfg.braketilt_strength = 10.0f;
    cfg.braketilt_lingering = 2.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);
    EXPECT_TRUE(bt.factor < 0.0f);
    EXPECT_FLOAT_NEAR(bt.setpoint.off_speed_up, cfg.atr.filter.off_speed_limit / 2.0f);

    atr.accel_diff = 0.0f;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    EXPECT_TRUE(bt.target > 0.0f);
    float flat_target = bt.target;

    atr.accel_diff = -2.0f;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    EXPECT_TRUE(bt.target > 0.0f);
    EXPECT_TRUE(bt.target < flat_target);

    atr.accel_diff = -3.0f;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);

    brake_tilt_update(&bt, &md, &atr, false, 6.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);

    bt.setpoint.value = 4.0f;
    brake_tilt_update(&bt, &md, &atr, true, -6.0f, 0.1f);
    EXPECT_TRUE(bt.setpoint.is_winddown);
    EXPECT_TRUE(bt.setpoint.value < 4.0f);

    brake_tilt_reset(&bt);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);
    EXPECT_FLOAT_NEAR(bt.setpoint.value, 0.0f);

    return true;
}

static bool test_brake_tilt_negative_erpm_downhill_boundaries(void) {
    BrakeTilt bt;
    brake_tilt_init(&bt);

    RefloatConfig cfg = default_brake_tilt_cfg();
    brake_tilt_configure(&bt, &cfg, 100.0f);

    MotorData md = {
        .braking = true,
        .abs_erpm = 2000.0f,
        .erpm = -2000.0f,
        .erpm_sign = -1,
        .forward = false,
    };
    ATR atr = {.accel_diff = 2.0f};

    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);

    md.abs_erpm = 2001.0f;
    md.erpm = -1000.0f;
    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, -2.0f);

    md.erpm = -1001.0f;
    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, -1.0f);

    atr.accel_diff = 3.0f;
    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);

    return true;
}
