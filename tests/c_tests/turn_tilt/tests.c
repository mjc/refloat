static bool test_turn_tilt_blends_turn_response(void) {
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
    EXPECT_FLOAT_NEAR(tt.yaw_change.value, 0.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);
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
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_TRUE(tt.setpoint.value < 2.0f);

    turn_tilt_reset(&tt);
    EXPECT_TRUE(isnan(tt.last_yaw_angle));
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    return true;
}

static bool test_turn_tilt_aggregates_inputs_and_boost(void) {
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
    EXPECT_FLOAT_NEAR(tt.target, 1.9f);
    EXPECT_TRUE(tt.target > low_speed_target);

    tt.last_yaw_angle = -170.0f;
    tt.yaw_change.value = 0.0f;
    tt.yaw_aggregate = -30.0f;
    imu.yaw = 170.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    EXPECT_TRUE(tt.yaw_change.value < 0.0f);
    EXPECT_TRUE(tt.yaw_aggregate < 0.0f);

    tt.yaw_change.alpha = 1.0f;
    float aggregate = tt.yaw_aggregate;
    imu.yaw = 120.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    EXPECT_TRUE(tt.yaw_aggregate != aggregate);

    return true;
}

static bool test_turn_tilt_activates_at_configured_threshold(void) {
    RefloatConfig cfg = {0};
    cfg.turntilt_strength = 10.0f;
    cfg.turntilt_angle_limit = 50.0f;
    cfg.turntilt_start_angle = 5;
    cfg.turntilt_start_erpm = 1000;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 20;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    static const struct {
        const char *label;
        float yaw_change;
        float yaw_aggregate;
        float abs_erpm;
        float expected_target;
    } cases[] = {
        {"below the yaw-change threshold", 29.99f, 5.0f, 1000.0f, 0.0f},
        {"below the yaw-aggregate threshold", 30.0f, 4.99f, 1000.0f, 0.0f},
        {"below the start speed", 30.0f, 5.0f, 999.0f, 0.0f},
        {"at all activation thresholds", 30.0f, 5.0f, 1000.0f, 0.65625f},
        {"at the speed-boost limit", 30.0f, 5.0f, 2000.0f, 0.9166667f},
        {"with saturated aggregate response", 30.0f, 100.0f, 2000.0f, 1.6666667f},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        TurnTilt tt;
        turn_tilt_init(&tt);
        turn_tilt_configure(&tt, &cfg, 100.0f);
        tt.yaw_change.value = cases[index].yaw_change;
        tt.yaw_aggregate = cases[index].yaw_aggregate;

        MotorData md = {.abs_erpm = cases[index].abs_erpm, .erpm_sign = 1, .forward = true};
        turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
        if (fabsf(tt.target - cases[index].expected_target) > TEST_FLOAT_EPS) {
            fprintf(stderr, "Turn-tilt threshold contract: %s\n", cases[index].label);
            test_report_float_failure(
                __FILE__,
                __LINE__,
                cases[index].label,
                tt.target,
                cases[index].expected_target,
                TEST_FLOAT_EPS
            );
            return false;
        }
    }

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

    imu.yaw = NAN;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    EXPECT_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    EXPECT_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);

    return true;
}

static bool test_turn_tilt_rejects_negative_angle_limit(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {
        .turntilt_strength = 5.0f,
        .turntilt_angle_limit = -1.0f,
        .turntilt_start_angle = 5,
        .turntilt_start_erpm = 1000,
        .turntilt_erpm_boost_end = 2000,
        .turntilt_yaw_aggregate = 30,
        .turn_tilt.filter.time_constant = 0.1f,
    };
    turn_tilt_configure(&tt, &cfg, 100.0f);
    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 2500.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);

    EXPECT_FLOAT_NEAR(tt.target, 0.0f);
    return true;
}

static bool test_turn_tilt_bounds_upper_config(void) {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {
        .turntilt_strength = 30.0f,
        .turntilt_angle_limit = 100.0f,
        .turntilt_start_angle = 5,
        .turntilt_start_erpm = 1000,
        .turntilt_erpm_boost = 10000,
        .turntilt_erpm_boost_end = 2000,
        .turntilt_yaw_aggregate = 30,
        .turn_tilt.filter.time_constant = 0.1f,
    };
    turn_tilt_configure(&tt, &cfg, 100.0f);
    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 2500.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);

    EXPECT_FLOAT_NEAR(tt.target, 30.0f);

    cfg.turntilt_erpm_boost = UINT16_MAX;
    turn_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(tt.boost_per_erpm, 0.05f);

    cfg.turn_tilt.filter.time_constant = 30.0f;
    turn_tilt_configure(&tt, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(tt.setpoint.alpha, 2.146f * ema_calculate_alpha_time_constant(0.5f, 100.0f));

    cfg.turntilt_start_erpm = 0;
    cfg.turntilt_erpm_boost = 0;
    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 50.0f;
    md.abs_erpm = 50.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 0.0f);

    cfg.turntilt_strength = 100.0f;
    cfg.turntilt_start_angle = 0.0f;
    cfg.turntilt_erpm_boost = 0;
    cfg.turntilt_yaw_aggregate = UINT8_MAX;
    cfg.turntilt_start_erpm = 1000;
    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 50.0f;
    md.abs_erpm = 2500.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 3.5882353f);

    cfg.turntilt_strength = 10.0f;
    cfg.turntilt_start_angle = 100.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 1.1960784f);

    cfg.turntilt_start_angle = 0.0f;
    cfg.turntilt_yaw_aggregate = 1;
    tt.yaw_aggregate = 25.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    EXPECT_FLOAT_NEAR(tt.target, 1.5f);
    return true;
}
