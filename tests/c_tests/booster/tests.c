static RefloatConfig default_booster_cfg(void) {
    return (RefloatConfig){
        .booster_current = 10.0f,
        .booster_angle = 2.0f,
        .booster_ramp = 2.0f,
        .brkbooster_current = 5.0f,
        .brkbooster_angle = 2.0f,
        .brkbooster_ramp = 2.0f,
    };
}

static bool test_booster_applies_speed_and_brake_torque(void) {
    RefloatConfig cfg = default_booster_cfg();
    cfg.booster_current = 8.0f;
    cfg.booster_angle = 2.0f;
    cfg.booster_ramp = 2.0f;
    cfg.brkbooster_current = 4.0f;
    cfg.brkbooster_angle = 3.0f;
    cfg.brkbooster_ramp = 2.0f;

    static const struct {
        const char *label;
        float abs_erpm;
        bool braking;
        float proportional;
        float expected_torque;
    } cases[] = {
        {"at the acceleration start angle", 3000.0f, false, 2.0f, 0.0f},
        {"past the acceleration ramp", 3000.0f, false, 4.0f, 8.0f * TORQUE_CONSTANT_COMPAT},
        {"at the high-speed acceleration start angle", 13000.0f, false, 1.0f, 0.0f},
        {"inside the high-speed acceleration ramp",
         13000.0f,
         false,
         -1.5f,
         -2.0f * TORQUE_CONSTANT_COMPAT},
        {"past the high-speed brake ramp", 13000.0f, true, -5.0f, -8.0f * TORQUE_CONSTANT_COMPAT},
        {"past the brake speed multiplier limit",
         23000.0f,
         true,
         5.0f,
         8.0f * TORQUE_CONSTANT_COMPAT},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        Booster booster;
        booster_init(&booster);
        booster.torque.alpha = 1.0f;

        MotorData md = {.abs_erpm = cases[index].abs_erpm, .braking = cases[index].braking};
        booster_update(&booster, &md, &cfg, cases[index].proportional);
        if (fabsf(booster.torque.value - cases[index].expected_torque) > TEST_FLOAT_EPS) {
            fprintf(stderr, "Booster torque contract: %s\n", cases[index].label);
            test_report_float_failure(
                __FILE__,
                __LINE__,
                cases[index].label,
                booster.torque.value,
                cases[index].expected_torque,
                TEST_FLOAT_EPS
            );
            return false;
        }
    }

    return true;
}

static bool test_booster_ramps_and_resets_with_speed(void) {
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

static bool test_booster_bounds_control_config(void) {
    // ConfigParams/XML and raw COMM_SET_CUSTOM_CONFIG packets can bypass the
    // VESC Tool widgets' XML minima and preserve these signed float16 values.
    Booster booster;
    booster_init(&booster);
    booster.torque.alpha = 1.0f;

    RefloatConfig cfg = default_booster_cfg();
    MotorData md = {.abs_erpm = 0.0f, .braking = false};

    cfg.booster_current = -10.0f;
    booster_update(&booster, &md, &cfg, 10.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    cfg = default_booster_cfg();
    cfg.booster_angle = -1.0f;
    booster_update(&booster, &md, &cfg, 0.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    cfg = default_booster_cfg();
    cfg.booster_angle = 1.0f;
    cfg.booster_ramp = -1.0f;
    booster_update(&booster, &md, &cfg, 1.5f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 5.0f * TORQUE_CONSTANT_COMPAT);

    cfg = default_booster_cfg();
    cfg.brkbooster_current = -10.0f;
    md.braking = true;
    booster_update(&booster, &md, &cfg, -10.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    cfg = default_booster_cfg();
    cfg.brkbooster_angle = -1.0f;
    booster_update(&booster, &md, &cfg, 0.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 0.0f);

    cfg = default_booster_cfg();
    cfg.brkbooster_angle = 1.0f;
    cfg.brkbooster_ramp = -1.0f;
    booster_update(&booster, &md, &cfg, -1.5f);
    EXPECT_FLOAT_NEAR(booster.torque.value, -2.5f * TORQUE_CONSTANT_COMPAT);

    md.braking = false;
    cfg = default_booster_cfg();
    cfg.booster_current = 200.0f;
    cfg.booster_angle = 0.0f;
    cfg.booster_ramp = 1.0f;
    booster_update(&booster, &md, &cfg, 2.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 100.0f * TORQUE_CONSTANT_COMPAT);

    cfg = default_booster_cfg();
    cfg.booster_angle = 0.0f;
    cfg.booster_ramp = 100.0f;
    booster_update(&booster, &md, &cfg, 17.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, 10.0f * TORQUE_CONSTANT_COMPAT);

    md.braking = true;
    cfg = default_booster_cfg();
    cfg.brkbooster_current = 200.0f;
    cfg.brkbooster_angle = 0.0f;
    cfg.brkbooster_ramp = 1.0f;
    booster_update(&booster, &md, &cfg, -2.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, -100.0f * TORQUE_CONSTANT_COMPAT);

    cfg = default_booster_cfg();
    cfg.brkbooster_angle = 0.0f;
    cfg.brkbooster_ramp = 100.0f;
    booster_update(&booster, &md, &cfg, -17.0f);
    EXPECT_FLOAT_NEAR(booster.torque.value, -5.0f * TORQUE_CONSTANT_COMPAT);

    return true;
}
