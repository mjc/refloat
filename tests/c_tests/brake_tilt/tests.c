static RefloatConfig default_brake_tilt_cfg(void) {
    return (RefloatConfig){
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
    return (RefloatConfig){
        .booster_current = 10.0f,
        .booster_angle = 2.0f,
        .booster_ramp = 2.0f,
        .brkbooster_current = 5.0f,
        .brkbooster_angle = 2.0f,
        .brkbooster_ramp = 2.0f,
    };
}

static bool test_booster_and_brake_tilt_shape_motor_command(void) {
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
    md = (MotorData){
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

    brake_tilt_update(&bt, &md, &atr, false, 6.0f, 0.1f);
    EXPECT_FLOAT_NEAR(bt.target, 0.0f);

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

static bool test_brake_tilt_handles_downhill_negative_erpm(void) {
    RefloatConfig cfg = default_brake_tilt_cfg();

    static const struct {
        const char *label;
        bool braking;
        float abs_erpm;
        float erpm;
        float accel_diff;
        float expected_target;
    } cases[] = {
        {"at the brake-tilt speed threshold", true, 2000.0f, -2000.0f, 2.0f, 0.0f},
        {"without an active braking command", false, 2001.0f, -2000.0f, 2.0f, 0.0f},
        {"at the downhill acceleration threshold", true, 2001.0f, -1000.0f, 2.0f, -2.0f},
        {"downhill without acceleration disagreement", true, 2001.0f, -1001.0f, 0.0f, -2.0f},
        {"downhill with moderate acceleration disagreement", true, 2001.0f, -1001.0f, 2.0f, -1.0f},
        {"steep downhill disables brake tilt", true, 2001.0f, -1001.0f, 3.0f, 0.0f},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        BrakeTilt bt;
        brake_tilt_init(&bt);
        brake_tilt_configure(&bt, &cfg, 100.0f);

        MotorData md = {
            .braking = cases[index].braking,
            .abs_erpm = cases[index].abs_erpm,
            .erpm = cases[index].erpm,
            .erpm_sign = -1,
            .forward = false,
        };
        ATR atr = {.accel_diff = cases[index].accel_diff};
        brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);

        if (fabsf(bt.target - cases[index].expected_target) > TEST_FLOAT_EPS) {
            fprintf(stderr, "Brake-tilt downhill contract: %s\n", cases[index].label);
            test_report_float_failure(
                __FILE__,
                __LINE__,
                cases[index].label,
                bt.target,
                cases[index].expected_target,
                TEST_FLOAT_EPS
            );
            return false;
        }
    }

    return true;
}

static bool test_brake_tilt_rejects_negative_strength(void) {
    BrakeTilt bt;
    brake_tilt_init(&bt);

    RefloatConfig cfg = default_brake_tilt_cfg();
    cfg.braketilt_strength = -1.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);

    EXPECT_FLOAT_NEAR(bt.factor, 0.0f);
    return true;
}

static bool test_brake_tilt_bounds_high_strength(void) {
    BrakeTilt bt;
    brake_tilt_init(&bt);

    RefloatConfig cfg = default_brake_tilt_cfg();
    cfg.braketilt_strength = 25.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);

    EXPECT_FLOAT_NEAR(bt.factor, -0.5f);

    cfg.braketilt_lingering = 327.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(bt.setpoint.off_speed_up, 50.0f / 15.0f);

    cfg.braketilt_lingering = 1.0f;
    cfg.atr.filter.on_speed_limit = 327.0f;
    cfg.atr.filter.off_speed_limit = 327.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(bt.setpoint.on_speed_up, 100.0f);
    EXPECT_FLOAT_NEAR(bt.setpoint.off_speed_up, 100.0f);
    return true;
}
