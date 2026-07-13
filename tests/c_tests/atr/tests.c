static RefloatConfig default_atr_cfg(void) {
    return (RefloatConfig){
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
        .atr_amps_accel_ratio = 5.0f,
        .atr_amps_decel_ratio = 4.0f,
    };
}

static bool test_atr_applies_tiltback_and_acceleration_response(void) {
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
        .atr_amps_accel_ratio = 5.0f,
        .atr_amps_decel_ratio = 4.0f,
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

static bool test_atr_applies_speed_boost_and_reset(void) {
    RefloatConfig cfg = default_atr_cfg();
    cfg.atr_strength_up = 1.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_angle_limit = 10.0f;

    static const struct {
        const char *label;
        float abs_erpm;
        bool responds;
        bool reduces_previous_response;
    } response_cases[] = {
        {"below the ATR activation speed", 250.0f, false, false},
        {"above the ATR activation speed", 251.0f, true, false},
        {"above the mid-speed response threshold", 1001.0f, true, true},
        {"above the high-speed response threshold", 2001.0f, true, true},
    };

    float previous_target = 0.0f;
    for (size_t index = 0; index < sizeof(response_cases) / sizeof(response_cases[0]); ++index) {
        ATR atr;
        atr_init(&atr);
        atr_configure(&atr, &cfg, 200.0f);

        MotorData md = {
            .torque = 9.0f * TORQUE_CONSTANT_COMPAT,
            .erpm_sign = 1,
            .abs_erpm = response_cases[index].abs_erpm,
            .forward = true,
        };
        atr_update(&atr, &md, &cfg, false, 0.01f);

        if (!response_cases[index].responds) {
            EXPECT_FLOAT_NEAR(atr.target, 0.0f);
            continue;
        }

        if (atr.target <= 0.0f ||
            (response_cases[index].reduces_previous_response && atr.target >= previous_target)) {
            fprintf(stderr, "ATR speed response: %s\n", response_cases[index].label);
            test_report_float_failure(
                __FILE__, __LINE__, response_cases[index].label, atr.target, previous_target, 0.0
            );
            return false;
        }
        previous_target = atr.target;
    }

    ATR atr;
    atr_init(&atr);
    atr_configure(&atr, &cfg, 200.0f);
    MotorData md = {
        .torque = 30.0f * TORQUE_CONSTANT_COMPAT,
        .erpm_sign = 1,
        .abs_erpm = 9000.0f,
        .forward = true,
    };

    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_TRUE(atr.speed_boost < 0.0f);
    EXPECT_TRUE(atr.target >= -cfg.atr_angle_limit);
    EXPECT_TRUE(atr.target <= cfg.atr_angle_limit);

    atr.setpoint.value = 0.25f;
    ema_reset(&atr.transition_target, -0.25f);
    atr.transition_target.alpha = 0.0f;
    md.abs_erpm = 250.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.transition_boost, 1.0f);

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

static bool test_atr_zero_accel_ratio_stays_finite(void) {
    ATR atr;
    atr_init(&atr);

    RefloatConfig cfg = default_atr_cfg();
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

typedef enum {
    ATR_NEGATIVE_STRENGTH_UP,
    ATR_NEGATIVE_STRENGTH_DOWN,
    ATR_NEGATIVE_THRESHOLD_UP,
    ATR_NEGATIVE_THRESHOLD_DOWN,
    ATR_NEGATIVE_ANGLE_LIMIT,
    ATR_NEGATIVE_TRANSITION_BOOST,
    ATR_NEGATIVE_SPEED_BOOST,
} AtrHostileConfigInput;

typedef struct {
    const char *label;
    AtrHostileConfigInput input;
    float expected_output;
} AtrHostileConfigCase;

static float atr_hostile_config_result(AtrHostileConfigInput input) {
    ATR atr;
    atr_init(&atr);
    RefloatConfig cfg = default_atr_cfg();
    cfg.atr_strength_up = 1.0f;
    cfg.atr_strength_down = 1.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_threshold_down = 0.0f;
    cfg.atr_speed_boost = 0.0f;
    cfg.atr_angle_limit = 10.0f;
    MotorData md = {.erpm_sign = 1, .abs_erpm = 2000.0f, .forward = true};

    switch (input) {
    case ATR_NEGATIVE_STRENGTH_UP:
        cfg.atr_strength_up = -1.0f;
        break;
    case ATR_NEGATIVE_STRENGTH_DOWN:
        cfg.atr_strength_down = -1.0f;
        md.forward = false;
        break;
    case ATR_NEGATIVE_THRESHOLD_UP:
        cfg.atr_threshold_up = -1.0f;
        break;
    case ATR_NEGATIVE_THRESHOLD_DOWN:
        cfg.atr_threshold_down = -1.0f;
        md.braking = true;
        break;
    case ATR_NEGATIVE_ANGLE_LIMIT:
        cfg.atr_angle_limit = -1.0f;
        break;
    case ATR_NEGATIVE_TRANSITION_BOOST:
        cfg.atr.transition_boost = -1.0f;
        break;
    case ATR_NEGATIVE_SPEED_BOOST:
        cfg.atr_speed_boost = -2.0f;
        md.abs_erpm = 14000.0f;
        break;
    }

    atr_configure(&atr, &cfg, 100.0f);
    atr.ad_alpha1 = atr.ad_alpha2 = atr.ad_alpha3 = 0.0f;
    atr.accel_diff = input == ATR_NEGATIVE_TRANSITION_BOOST ? -2.0f : 2.0f;
    atr.transition_target.alpha = 1.0f;
    atr.setpoint.value = input == ATR_NEGATIVE_TRANSITION_BOOST ? 2.0f : 0.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);

    return input == ATR_NEGATIVE_TRANSITION_BOOST ? atr.transition_boost : atr.target;
}

static bool test_atr_clamps_hostile_control_config(void) {
    // ConfigParams/XML and COMM_SET_CUSTOM_CONFIG over BLE preserve signed float16 values.
    static const AtrHostileConfigCase cases[] = {
        {"negative forward strength disables forward ATR", ATR_NEGATIVE_STRENGTH_UP, 0.0f},
        {"negative reverse strength disables reverse ATR", ATR_NEGATIVE_STRENGTH_DOWN, 0.0f},
        {"negative forward threshold clamps to zero", ATR_NEGATIVE_THRESHOLD_UP, 2.0f},
        {"negative braking threshold clamps to zero", ATR_NEGATIVE_THRESHOLD_DOWN, 2.0f},
        {"negative angle limit disables ATR", ATR_NEGATIVE_ANGLE_LIMIT, 0.0f},
        {"negative transition boost uses baseline", ATR_NEGATIVE_TRANSITION_BOOST, 1.0f},
        {"speed boost below the lower bound disables boost", ATR_NEGATIVE_SPEED_BOOST, 0.0f},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        float actual = atr_hostile_config_result(cases[index].input);
        if (fabsf(actual - cases[index].expected_output) > TEST_FLOAT_EPS) {
            fprintf(stderr, "ATR hostile configuration: %s\n", cases[index].label);
            test_report_float_failure(
                __FILE__,
                __LINE__,
                cases[index].label,
                actual,
                cases[index].expected_output,
                TEST_FLOAT_EPS
            );
            return false;
        }
    }

    return true;
}

static bool test_atr_clamps_upper_control_config(void) {
    ATR atr;
    atr_init(&atr);

    RefloatConfig cfg = default_atr_cfg();
    cfg.atr_strength_up = 100.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_angle_limit = 100.0f;
    atr_configure(&atr, &cfg, 100.0f);
    atr.ad_alpha1 = atr.ad_alpha2 = atr.ad_alpha3 = 0.0f;
    atr.accel_diff = 2.0f;
    MotorData md = {.erpm_sign = 1, .abs_erpm = 2000.0f, .forward = true};
    atr_update(&atr, &md, &cfg, false, 0.01f);

    EXPECT_FLOAT_NEAR(atr.target, 7.0f);

    atr_reset(&atr);
    cfg.atr_strength_down = 100.0f;
    md.forward = false;
    atr.accel_diff = 2.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.target, 7.0f);

    cfg.atr_strength_up = 3.5f;
    cfg.atr_threshold_up = 100.0f;
    md.forward = true;
    atr.accel_diff = 10.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.target, 27.5f);

    cfg.atr_strength_down = 3.5f;
    cfg.atr_threshold_down = 100.0f;
    md.forward = false;
    atr.accel_diff = 10.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.target, 27.5f);

    cfg.atr_strength_down = 1.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_threshold_down = 0.0f;
    cfg.atr.transition_boost = 100.0f;
    md.forward = true;
    atr.accel_diff = -3.0f;
    atr.transition_target.alpha = 1.0f;
    atr.setpoint.value = 3.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.transition_boost, 4.0f);

    cfg = default_atr_cfg();
    cfg.atr.filter.on_speed_limit = 327.0f;
    cfg.atr.filter.off_speed_limit = 327.0f;
    atr_configure(&atr, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(atr.setpoint.on_speed_up, 100.0f);
    EXPECT_FLOAT_NEAR(atr.setpoint.off_speed_up, 100.0f);

    cfg.atr.filter.time_constant = 30.0f;
    cfg.atr.filter.on_speed_time_constant = 30.0f;
    cfg.atr.filter.off_speed_time_constant = 30.0f;
    atr_configure(&atr, &cfg, 100.0f);
    EXPECT_FLOAT_NEAR(atr.setpoint.alpha, 2.146f * ema_calculate_alpha_time_constant(0.5f, 100.0f));
    EXPECT_FLOAT_NEAR(atr.setpoint.on_speed_alpha, ema_calculate_alpha_time_constant(0.5f, 100.0f));
    EXPECT_FLOAT_NEAR(
        atr.setpoint.off_speed_alpha, ema_calculate_alpha_time_constant(0.5f, 100.0f)
    );

    cfg.atr_strength_up = 1.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_angle_limit = 30.0f;
    cfg.atr_amps_accel_ratio = 1.0f;
    atr.ad_alpha1 = atr.ad_alpha2 = atr.ad_alpha3 = 1.0f;
    md.torque = 20.0f * TORQUE_CONSTANT_COMPAT;
    md.abs_erpm = 3000.0f;
    md.acceleration.value = 0.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, 2.4f);

    atr_reset(&atr);
    cfg.atr_amps_decel_ratio = 1.0f;
    md.braking = true;
    md.torque = -20.0f * TORQUE_CONSTANT_COMPAT;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    EXPECT_FLOAT_NEAR(atr.accel_diff, -7.0f);
    return true;
}
