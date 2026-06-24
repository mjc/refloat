static bool test_turn_tilt_branch_cases(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {0};
    cfg.turntilt_strength = 5.0f;
    cfg.turntilt_angle_limit = 8.0f;
    cfg.turntilt_start_angle = 5;
    cfg.turntilt_start_erpm = 1000;
    cfg.turntilt_erpm_boost = 50;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 30;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_TRUE(tt.boost_per_erpm > 0.0f);

    IMU imu = {.yaw = 179.0f};
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    imu.yaw = -179.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    EXPECT_TRUE(tt.yaw_change.value > 0.0f);

    imu.yaw = 120.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    EXPECT_TRUE(tt.yaw_change.value < 0.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);

    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 900.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    md.abs_erpm = 2500.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_TRUE(tt.target > 0.0f);
    EXPECT_TRUE(tt.target <= cfg.turntilt_angle_limit);

    md.erpm_sign = -1;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_TRUE(tt.target < 0.0f);
    EXPECT_TRUE(tt.target >= -cfg.turntilt_angle_limit);

    tt.setpoint.value = 4.0f;
    turn_tilt_update(&tt, &md, &cfg, true, 0.1f);
    EXPECT_TRUE(tt.setpoint.is_winddown);
    EXPECT_TRUE(tt.setpoint.value < 4.0f);

    cfg.turntilt_strength = 0.0f;
    tt.target = 3.0f;
    tt.setpoint.value = 2.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 3.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 2.0f);

    turn_tilt_reset(&tt);
    EXPECT_FLOAT_NEAR(tt.last_yaw_angle, 0.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    return true;
}

static bool test_turn_tilt_aggregate_and_boost_edges(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {0};
    cfg.turntilt_strength = 5.0f;
    cfg.turntilt_angle_limit = 20.0f;
    cfg.turntilt_start_angle = 2;
    cfg.turntilt_start_erpm = 500;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 10;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(tt.boost_per_erpm, 0.0005f);

    IMU imu = {.yaw = 0.0f};
    turn_tilt_aggregate(&tt, &imu, 1.0f);
    imu.yaw = 10.0f;
    turn_tilt_aggregate(&tt, &imu, 1.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);

    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 1500.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    float low_speed_target = tt.target;
    EXPECT_TRUE(low_speed_target > 0.0f);
    EXPECT_TRUE(low_speed_target < cfg.turntilt_angle_limit);

    turn_tilt_reset(&tt);
    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    md.abs_erpm = 3000.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 2.0f);
    EXPECT_TRUE(tt.target > low_speed_target);

    tt.last_yaw_angle = -170.0f;
    tt.yaw_change.value = 0.0f;
    tt.yaw_aggregate = -30.0f;
    imu.yaw = 170.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    EXPECT_TRUE(tt.yaw_change.value < 0.0f);
    EXPECT_TRUE(tt.yaw_aggregate < 0.0f);

    return true;
}

static bool test_turn_tilt_threshold_boundary_edges(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {0};
    cfg.turntilt_strength = 10.0f;
    cfg.turntilt_angle_limit = 50.0f;
    cfg.turntilt_start_angle = 5;
    cfg.turntilt_start_erpm = 1000;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 20;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);

    MotorData md = {.abs_erpm = 1000.0f, .erpm_sign = 1, .forward = true};
    tt.yaw_change.value = 29.99f;
    tt.yaw_aggregate = 5.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    tt.yaw_change.value = 30.0f;
    tt.yaw_aggregate = 4.99f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    tt.yaw_aggregate = 5.0f;
    md.abs_erpm = 999.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    md.abs_erpm = 1000.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.703125f);

    md.abs_erpm = 2000.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 1.0416667f);

    tt.yaw_aggregate = 100.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 1.6666667f);

    return true;
}

static bool test_turn_tilt_zero_denominator_config(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {0};
    cfg.turntilt_strength = 5.0f;
    cfg.turntilt_angle_limit = 20.0f;
    cfg.turntilt_start_angle = 2;
    cfg.turntilt_start_erpm = 500;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 0;
    cfg.turntilt_yaw_aggregate = 0;
    cfg.turn_tilt.filter.time_constant = 0.1f;

    turn_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_TRUE(isfinite(tt.boost_per_erpm));

    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 1500.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_TRUE(isfinite(tt.target));
    EXPECT_TRUE(isfinite(tt.setpoint.value));

    return true;
}

static bool test_turn_tilt_nonpositive_dt(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {0};
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);

    tt.last_yaw_angle = 5.0f;
    tt.yaw_change.value = 12.0f;
    tt.yaw_aggregate = 8.0f;

    IMU imu = {.yaw = 35.0f};
    turn_tilt_aggregate(&tt, &imu, 0.0f);
    EXPECT_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);

    imu.yaw = -20.0f;
    turn_tilt_aggregate(&tt, &imu, -0.02f);
    EXPECT_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);

    imu.yaw = 90.0f;
    turn_tilt_aggregate(&tt, &imu, NAN);
    EXPECT_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    EXPECT_TRUE(isfinite(tt.yaw_change.value));
    EXPECT_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);

    return true;
}
