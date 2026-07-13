static RefloatConfig default_torque_tilt_cfg(void) {
    return (RefloatConfig){
        .torquetilt_strength = 0.3f,
        .torquetilt_strength_regen = 0.2f,
        .torquetilt_start_current = 2.0f,
        .torquetilt_angle_limit = 5.0f,
        .torque_tilt.filter.time_constant = 0.2f,
        .torque_tilt.filter.on_speed_time_constant = 0.1f,
        .torque_tilt.filter.off_speed_time_constant = 0.1f,
        .torque_tilt.filter.on_speed_limit = 100.0f,
        .torque_tilt.filter.off_speed_limit = 100.0f,
    };
}

static bool test_torque_tilt_initializes_limits_and_winds_down(void) {
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
    EXPECT_FLOAT_NEAR(tt.target, 0.6f);
    EXPECT_TRUE(tt.setpoint.value > 0.0f);

    md.braking = true;
    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, -0.2f);

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

static bool test_torque_tilt_follows_torque_sign_and_strength(void) {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_strength = 1.0f;
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
    EXPECT_FLOAT_NEAR(tt.target, -2.0f);
    EXPECT_TRUE(tt.setpoint.value < 0.0f);

    md.forward = true;
    md.torque = cfg.torquetilt_start_current * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    md.torque = (cfg.torquetilt_start_current + 0.5f) * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.5f);

    md.braking = true;
    md.torque = -4.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    cfg.torquetilt_strength_regen = 1.0f;
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

static bool test_torque_tilt_applies_regen_and_lower_limits(void) {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_strength = 1.0f;
    cfg.torquetilt_strength_regen = 1.0f;
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
    EXPECT_FLOAT_NEAR(tt.target, 2.0f);

    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, -2.0f);

    return true;
}

static float torque_tilt_hostile_target(size_t field) {
    TorqueTilt tt;
    torque_tilt_init(&tt);
    RefloatConfig cfg = default_torque_tilt_cfg();
    MotorData md = {.forward = true, .torque = 4.0f * TORQUE_CONSTANT_COMPAT};

    switch (field) {
    case 0:
        cfg.torquetilt_strength = -1.0f;
        break;
    case 1:
        cfg.torquetilt_strength_regen = -1.0f;
        md.braking = true;
        md.torque = -4.0f * TORQUE_CONSTANT_COMPAT;
        break;
    case 2:
        cfg.torquetilt_start_current = -1.0f;
        md.torque = 0.5f * TORQUE_CONSTANT_COMPAT;
        break;
    default:
        cfg.torquetilt_angle_limit = -1.0f;
        break;
    }

    torque_tilt_update(&tt, &md, &cfg, false, 0.01f);
    return tt.target;
}

static bool test_torque_tilt_rejects_negative_control_config(void) {
    // ConfigParams/XML and COMM_SET_CUSTOM_CONFIG over BLE preserve signed float16 values.
    const float expected[] = {0.0f, 0.0f, 0.15f, 0.0f};
    for (size_t field = 0; field < 4; ++field) {
        EXPECT_FLOAT_NEAR(torque_tilt_hostile_target(field), expected[field]);
    }
    return true;
}

static bool test_torque_tilt_bounds_upper_config(void) {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_angle_limit = 100.0f;
    MotorData md = {.forward = true, .torque = 200.0f * TORQUE_CONSTANT_COMPAT};
    torque_tilt_update(&tt, &md, &cfg, false, 0.01f);

    EXPECT_FLOAT_NEAR(tt.target, 30.0f);

    cfg.torque_tilt.filter.on_speed_limit = 327.0f;
    cfg.torque_tilt.filter.off_speed_limit = 327.0f;
    torque_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.on_speed_up, 100.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.off_speed_up, 100.0f);

    cfg.torque_tilt.filter.time_constant = 30.0f;
    cfg.torque_tilt.filter.on_speed_time_constant = 30.0f;
    cfg.torque_tilt.filter.off_speed_time_constant = 30.0f;
    torque_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.alpha, 2.146f * ema_calculate_alpha_time_constant(0.5f, 100.0f));
    EXPECT_FLOAT_NEAR(tt.setpoint.on_speed_alpha, ema_calculate_alpha_time_constant(0.5f, 100.0f));
    EXPECT_FLOAT_NEAR(tt.setpoint.off_speed_alpha, ema_calculate_alpha_time_constant(0.5f, 100.0f));

    cfg.torquetilt_angle_limit = 30.0f;
    cfg.torquetilt_start_current = 0.0f;
    cfg.torquetilt_strength = 100.0f;
    md.torque = 2.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(tt.target, 2.0f);

    cfg.torquetilt_strength_regen = 100.0f;
    md.braking = true;
    md.torque = -2.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(tt.target, -2.0f);

    cfg.torquetilt_strength = 1.0f;
    cfg.torquetilt_start_current = 200.0f;
    md.braking = false;
    md.torque = 150.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(tt.target, 30.0f);
    return true;
}
