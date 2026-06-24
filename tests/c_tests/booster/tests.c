static RefloatConfig default_booster_cfg(void) {
    return (RefloatConfig) {
        .booster_current = 10.0f,
        .booster_angle = 2.0f,
        .booster_ramp = 2.0f,
        .brkbooster_current = 5.0f,
        .brkbooster_angle = 2.0f,
        .brkbooster_ramp = 2.0f,
    };
}

static bool test_booster_threshold_boundary_edges(void) {
    Booster booster;
    booster_init(&booster);
    booster.torque.alpha = 1.0f;

    RefloatConfig cfg = default_booster_cfg();
    cfg.booster_current = 8.0f;
    cfg.booster_angle = 2.0f;
    cfg.booster_ramp = 2.0f;
    cfg.brkbooster_current = 4.0f;
    cfg.brkbooster_angle = 3.0f;
    cfg.brkbooster_ramp = 2.0f;

    MotorData md = {.abs_erpm = 3000.0f, .braking = false};
    booster_update(&booster, &md, &cfg, 2.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, 4.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, cfg.booster_current * TORQUE_CONSTANT_COMPAT);

    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, 1.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, -1.5f);
    EXPECT_FLOAT_NEAR(booster.torque.value, -2.0f * TORQUE_CONSTANT_COMPAT);

    md.braking = true;
    booster_update(&booster, &md, &cfg, -5.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, -8.0f * TORQUE_CONSTANT_COMPAT);

    md.abs_erpm = 23000.0f;
    booster_update(&booster, &md, &cfg, 5.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 8.0f * TORQUE_CONSTANT_COMPAT);

    return true;
}

static bool test_booster_threshold_ramp_and_reset_edges(void) {
    Booster booster;
    booster_init(&booster);
    booster_configure(&booster, 1000.0f);

    RefloatConfig cfg = default_booster_cfg();
    cfg.booster_current = 8.0f;
    cfg.booster_angle = 3.0f;
    cfg.booster_ramp = 2.0f;
    cfg.brkbooster_current = 6.0f;
    cfg.brkbooster_angle = 4.0f;
    cfg.brkbooster_ramp = 2.0f;

    MotorData md = {.abs_erpm = 0.0f, .braking = false};
    booster_update(&booster, &md, &cfg, 3.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, -4.0f);
    EXPECT_TRUE(booster.torque.value < 0.0f);
    EXPECT_TRUE(booster.torque.value > -cfg.booster_current * TORQUE_CONSTANT_COMPAT);

    booster_reset(&booster);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, 6.0f);
    float low_speed_accel = booster.torque.value;
    EXPECT_TRUE(low_speed_accel > 0.0f);

    booster_reset(&booster);
    md.abs_erpm = 3000.0f;
    booster_update(&booster, &md, &cfg, 6.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, low_speed_accel);

    booster_reset(&booster);
    md.abs_erpm = 3000.0f;
    booster_update(&booster, &md, &cfg, 4.0f);
    float threshold_accel = booster.torque.value;

    booster_reset(&booster);
    md.abs_erpm = 3001.0f;
    booster_update(&booster, &md, &cfg, 4.0f);
    EXPECT_TRUE(booster.torque.value > threshold_accel);

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, 2.0f);
    EXPECT_TRUE(booster.torque.value > 0.0f);
    EXPECT_TRUE(booster.torque.value < low_speed_accel);

    booster_reset(&booster);
    md.braking = true;
    md.abs_erpm = 3000.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    float threshold_brake = booster.torque.value;

    booster_reset(&booster);
    md.abs_erpm = 3001.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    EXPECT_TRUE(booster.torque.value < threshold_brake);

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    EXPECT_TRUE(booster.torque.value < threshold_brake);

    return true;
}
