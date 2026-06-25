static RefloatConfig default_torque_tilt_cfg(void) {
    return (RefloatConfig) {
        .torquetilt_strength = 3.0f,
        .torquetilt_strength_regen = 2.0f,
        .torquetilt_start_current = 2.0f,
        .torquetilt_angle_limit = 5.0f,
        .torque_tilt.filter.time_constant = 0.2f,
        .torque_tilt.filter.on_speed_time_constant = 0.1f,
        .torque_tilt.filter.off_speed_time_constant = 0.1f,
        .torque_tilt.filter.on_speed_limit = 100.0f,
        .torque_tilt.filter.off_speed_limit = 100.0f,
    };
}

static bool test_torque_tilt_threshold_limit_regen_and_wheelslip(void) {
    TorqueTilt tt;
    torque_tilt_init(&tt);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    RefloatConfig cfg = default_torque_tilt_cfg();
    torque_tilt_configure(&tt, &cfg, 100.0f);

    MotorData md = {0};
    md.forward = true;
    md.braking = false;
    md.torque = 1.5f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    md.torque = 4.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, cfg.torquetilt_angle_limit);
    EXPECT_TRUE(tt.setpoint.value > 0.0f);

    md.braking = true;
    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, -2.0f);

    tt.setpoint.value = 4.0f;
    torque_tilt_update(&tt, &md, &cfg, true, 0.1f);
    EXPECT_TRUE(tt.setpoint.is_winddown);
    EXPECT_TRUE(tt.setpoint.value < 4.0f);

    torque_tilt_reset(&tt);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 0.0f);
    EXPECT_TRUE(!tt.setpoint.is_winddown);

    return true;
}

static bool test_torque_tilt_sign_strength_and_filter_edges(void) {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_strength = 1.5f;
    cfg.torquetilt_strength_regen = 0.0f;
    cfg.torquetilt_start_current = 1.0f;
    cfg.torquetilt_angle_limit = 10.0f;
    cfg.torque_tilt.filter.time_constant = 0.25f;
    cfg.torque_tilt.filter.on_speed_time_constant = 0.15f;
    cfg.torque_tilt.filter.off_speed_time_constant = 0.2f;
    cfg.torque_tilt.filter.on_speed_limit = 7.0f;
    cfg.torque_tilt.filter.off_speed_limit = 3.0f;
    torque_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.on_speed_up, 7.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.off_speed_up, 3.0f);

    MotorData md = {0};
    md.forward = false;
    md.braking = false;
    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, -3.0f);
    EXPECT_TRUE(tt.setpoint.value < 0.0f);

    md.forward = true;
    md.torque = cfg.torquetilt_start_current * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    md.torque = (cfg.torquetilt_start_current + 0.5f) * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.75f);

    md.braking = true;
    md.torque = -4.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    cfg.torquetilt_strength_regen = 3.0f;
    cfg.torquetilt_start_current = 5.0f;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    md.torque = 20.0f * TORQUE_CONSTANT_COMPAT;
    md.braking = false;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, cfg.torquetilt_angle_limit);

    tt.target = 4.0f;
    tt.setpoint.value = 3.0f;
    torque_tilt_reset(&tt);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    return true;
}

static bool test_torque_tilt_negative_limit_and_regen_edges(void) {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_strength = 2.0f;
    cfg.torquetilt_strength_regen = 1.5f;
    cfg.torquetilt_start_current = 1.0f;
    cfg.torquetilt_angle_limit = 4.0f;
    cfg.torque_tilt.filter.time_constant = 0.1f;
    cfg.torque_tilt.filter.on_speed_time_constant = 0.1f;
    cfg.torque_tilt.filter.off_speed_time_constant = 0.1f;
    cfg.torque_tilt.filter.on_speed_limit = 100.0f;
    cfg.torque_tilt.filter.off_speed_limit = 100.0f;
    torque_tilt_configure(&tt, &cfg, 100.0f);

    MotorData md = {
        .forward = false,
        .braking = false,
        .torque = -20.0f * TORQUE_CONSTANT_COMPAT,
    };
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, -cfg.torquetilt_angle_limit);
    EXPECT_TRUE(tt.setpoint.value < 0.0f);

    md.forward = true;
    md.braking = true;
    md.torque = 3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 3.0f);

    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, -3.0f);

    return true;
}
