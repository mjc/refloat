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

static BalanceFilterData level_balance_filter(void) {
    return (BalanceFilterData) {
        .q0 = 1.0f,
        .acc_mag = 1.0f,
        .kp_pitch = 1.0f,
        .kp_roll = 1.0f,
        .kp_yaw = 1.0f,
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

static bool test_balance_filter_nonfinite_dt(void) {
    BalanceFilterData bf = level_balance_filter();

    float gyro[3] = {0.5f, -0.25f, 0.125f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    balance_filter_update(&bf, gyro, accel, NAN);

    EXPECT_TRUE(isfinite(bf.q0));
    EXPECT_TRUE(isfinite(bf.q1));
    EXPECT_TRUE(isfinite(bf.q2));
    EXPECT_TRUE(isfinite(bf.q3));
    EXPECT_TRUE(isfinite(balance_filter_get_roll(&bf)));
    EXPECT_TRUE(isfinite(balance_filter_get_pitch(&bf)));
    EXPECT_TRUE(isfinite(balance_filter_get_yaw(&bf)));

    float norm = sqrtf(bf.q0 * bf.q0 + bf.q1 * bf.q1 + bf.q2 * bf.q2 + bf.q3 * bf.q3);
    EXPECT_TRUE(fabsf(norm - 1.0f) < 0.00001f);

    return true;
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
