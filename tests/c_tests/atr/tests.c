static RefloatConfig default_atr_edge_cfg(void) {
    return (RefloatConfig) {
        .atr.filter.time_constant = 0.05f,
        .atr.filter.on_speed_time_constant = 0.05f,
        .atr.filter.off_speed_time_constant = 0.05f,
        .atr.filter.on_speed_limit = 100.0f,
        .atr.filter.off_speed_limit = 100.0f,
        .atr.transition_boost = 1.8f,
        .atr_strength_up = 0.25f,
        .atr_strength_down = 0.25f,
        .atr_threshold_up = 5.0f,
        .atr_threshold_down = 5.0f,
        .atr_speed_boost = -0.5f,
        .atr_angle_limit = 2.0f,
        .atr_amps_accel_ratio = 1.0f,
        .atr_amps_decel_ratio = 1.0f,
    };
}

static bool test_atr_branch_cases(void) {
    ATR atr;
    atr_init(&atr);
    EXPECT_FLOAT_NEAR(atr.accel_diff, 0.0f);
    EXPECT_FLOAT_NEAR(atr.speed_boost, 0.0f);
    EXPECT_FLOAT_NEAR(atr.transition_boost, 1.0f);

    RefloatConfig cfg = {
        .atr.filter.time_constant = 0.1f,
        .atr.filter.on_speed_time_constant = 0.1f,
        .atr.filter.off_speed_time_constant = 0.1f,
        .atr.filter.on_speed_limit = 100.0f,
        .atr.filter.off_speed_limit = 100.0f,
        .atr.transition_boost = 2.0f,
        .atr_strength_up = 1.0f,
        .atr_strength_down = 0.5f,
        .atr_threshold_up = 0.1f,
        .atr_threshold_down = 0.2f,
        .atr_speed_boost = 0.6f,
        .atr_angle_limit = 4.0f,
        .atr_amps_accel_ratio = 1.0f,
        .atr_amps_decel_ratio = 1.5f,
    };
    atr_configure(&atr, &cfg, 100.0f);
    EXPECT_TRUE(atr.speed_boost_mult < 1.0f / 3000.0f);
    EXPECT_TRUE(atr.ad_alpha1 > 0.0f);
    EXPECT_TRUE(atr.ad_alpha2 > 0.0f);
    EXPECT_TRUE(atr.ad_alpha3 > 0.0f);

    MotorData md = {
        .torque = 20.0f,
        .erpm_sign = 1,
        .abs_erpm = 100.0f,
        .forward = true,
        .braking = false,
    };
    md.acceleration.value = 0.0f;
    atr.accel_diff = 3.0f;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, 0.0f);
    EXPECT_FLOAT_NEAR(atr.speed_boost, 0.0f);

    md.abs_erpm = 5000.0f;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    EXPECT_TRUE(atr.accel_diff > 0.0f);
    EXPECT_TRUE(atr.speed_boost > 0.0f);
    EXPECT_TRUE(atr.target > 0.0f);
    EXPECT_TRUE(atr.target <= cfg.atr_angle_limit);

    md.braking = true;
    md.torque = -20.0f;
    md.erpm_sign = -1;
    md.forward = false;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(atr.speed_boost, 0.0f);
    EXPECT_TRUE(fabsf(atr.target) <= cfg.atr_angle_limit);

    atr.setpoint.value = 3.0f;
    ema_reset(&atr.transition_target, -3.0f);
    md.braking = false;
    md.torque = -25.0f;
    md.erpm_sign = 1;
    md.forward = true;
    md.abs_erpm = 5000.0f;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    EXPECT_TRUE(atr.transition_boost >= 1.0f);
    EXPECT_TRUE(atr.transition_boost <= cfg.atr.transition_boost);

    atr.setpoint.value = 2.0f;
    atr_update(&atr, &md, &cfg, true, 0.1f);
    EXPECT_TRUE(atr.setpoint.is_winddown);
    EXPECT_TRUE(atr.setpoint.value < 2.0f);
    EXPECT_FLOAT_NEAR(atr.transition_target.value, atr.setpoint.value);

    atr_reset(&atr);
    EXPECT_FLOAT_NEAR(atr.accel_diff, 0.0f);
    EXPECT_FLOAT_NEAR(atr.speed_boost, 0.0f);
    EXPECT_FLOAT_NEAR(atr.target, 0.0f);
    EXPECT_FLOAT_NEAR(atr.transition_boost, 1.0f);
    EXPECT_FLOAT_NEAR(atr.setpoint.value, 0.0f);

    return true;
}

static bool test_atr_threshold_speedboost_and_reset_edges(void) {
    ATR atr;
    atr_init(&atr);

    RefloatConfig cfg = default_atr_edge_cfg();
    atr_configure(&atr, &cfg, 200.0f);
    EXPECT_FLOAT_NEAR(atr.speed_boost_mult, 1.0f / 3000.0f);

    MotorData md = {
        .torque = 9.0f * TORQUE_CONSTANT_COMPAT,
        .erpm_sign = 1,
        .abs_erpm = 2000.0f,
        .forward = true,
        .braking = false,
    };
    md.acceleration.value = 0.0f;

    md.abs_erpm = 250.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, 0.0f);

    atr_reset(&atr);
    md.abs_erpm = 251.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, atr.ad_alpha3);

    atr_reset(&atr);
    md.abs_erpm = 1001.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, atr.ad_alpha2);

    atr_reset(&atr);
    md.abs_erpm = 2001.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, atr.ad_alpha1);

    atr_reset(&atr);
    md.abs_erpm = 2000.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.speed_boost, 0.0f);
    EXPECT_FLOAT_NEAR(atr.target, 0.0f);

    cfg.atr_threshold_up = 0.0f;
    md.abs_erpm = 9000.0f;
    md.torque = 30.0f * TORQUE_CONSTANT_COMPAT;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_TRUE(atr.speed_boost < 0.0f);
    EXPECT_TRUE(atr.target >= -cfg.atr_angle_limit);
    EXPECT_TRUE(atr.target <= cfg.atr_angle_limit);

    atr.accel_diff = 1.2f;
    atr.speed_boost = -0.2f;
    atr.target = 1.5f;
    atr.transition_boost = cfg.atr.transition_boost;
    atr.setpoint.value = 1.0f;
    ema_reset(&atr.transition_target, 1.0f);
    atr_reset(&atr);
    EXPECT_FLOAT_NEAR(atr.accel_diff, 0.0f);
    EXPECT_FLOAT_NEAR(atr.speed_boost, 0.0f);
    EXPECT_FLOAT_NEAR(atr.target, 0.0f);
    EXPECT_FLOAT_NEAR(atr.transition_boost, 1.0f);
    EXPECT_FLOAT_NEAR(atr.transition_target.value, 0.0f);
    EXPECT_FLOAT_NEAR(atr.setpoint.value, 0.0f);

    return true;
}

static bool test_atr_zero_accel_ratio_config(void) {
    ATR atr;
    atr_init(&atr);

    RefloatConfig cfg = default_atr_edge_cfg();
    cfg.atr.transition_boost = 1.5f;
    cfg.atr_strength_up = 1.0f;
    cfg.atr_strength_down = 1.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_threshold_down = 0.0f;
    cfg.atr_speed_boost = 0.0f;
    cfg.atr_angle_limit = 10.0f;
    cfg.atr_amps_accel_ratio = 0.0f;
    cfg.atr_amps_decel_ratio = 0.0f;
    atr_configure(&atr, &cfg, 100.0f);

    MotorData md = {
        .torque = 20.0f,
        .erpm_sign = 1,
        .abs_erpm = 4000.0f,
        .forward = true,
        .braking = false,
    };
    md.acceleration.value = 0.0f;

    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_TRUE(isfinite(atr.accel_diff));
    EXPECT_TRUE(isfinite(atr.target));
    EXPECT_TRUE(isfinite(atr.setpoint.value));

    md.braking = true;
    md.torque = -20.0f;
    md.erpm_sign = -1;
    md.forward = false;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_TRUE(isfinite(atr.accel_diff));
    EXPECT_TRUE(isfinite(atr.target));
    EXPECT_TRUE(isfinite(atr.setpoint.value));

    return true;
}
