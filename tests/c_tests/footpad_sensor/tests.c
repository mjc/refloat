static bool test_footpad_sensor(void) {
    vesc_if_fake_reset();
    FootpadSensor fs = {
        .adc_left = 9.0f,
        .adc_right = 8.0f,
        .state = FS_BOTH,
    };
    footpad_sensor_init(&fs);

    EXPECT_FLOAT_NEAR(fs.adc_left, 0.0f);
    EXPECT_FLOAT_NEAR(fs.adc_right, 0.0f);
    EXPECT_TRUE(fs.state == FS_NONE);

    RefloatConfig cfg = {
        .fault_adc1 = 1.0f,
        .fault_adc2 = 2.0f,
    };

    struct {
        float adc1;
        float adc2;
        FootpadSensorState state;
        float expected_left;
        float expected_right;
    } cases[] = {
        {0.5f, 2.5f, FS_RIGHT, 0.5f, 2.5f},
        {1.0f, 2.0f, FS_NONE, 1.0f, 2.0f},
        {-1.0f, -1.0f, FS_NONE, fs.adc_left, fs.adc_right},
        {1.5f, 2.5f, FS_BOTH, 1.5f, 2.5f},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        vesc_if_fake_set_analog(cases[i].adc1, cases[i].adc2);
        footpad_sensor_update(&fs, &cfg);
        EXPECT_TRUE(fs.state == cases[i].state);
        if (cases[i].adc1 >= 0.0f && cases[i].adc2 >= 0.0f) {
            EXPECT_FLOAT_NEAR(fs.adc_left, cases[i].expected_left);
            EXPECT_FLOAT_NEAR(fs.adc_right, cases[i].expected_right);
        }
    }

    cfg.hardware.swap_footpad_adcs = true;
    struct {
        float adc1;
        float adc2;
        FootpadSensorState state;
        float expected_left;
        float expected_right;
    } swapped_cases[] = {
        {0.5f, 2.5f, FS_LEFT, 2.5f, 0.5f},
        {1.0f, 2.0f, FS_NONE, 2.0f, 1.0f},
        {1.1f, 2.1f, FS_BOTH, 2.1f, 1.1f},
    };

    for (size_t i = 0; i < sizeof(swapped_cases) / sizeof(swapped_cases[0]); ++i) {
        vesc_if_fake_set_analog(swapped_cases[i].adc1, swapped_cases[i].adc2);
        footpad_sensor_update(&fs, &cfg);
        EXPECT_TRUE(fs.state == swapped_cases[i].state);
        EXPECT_FLOAT_NEAR(fs.adc_left, swapped_cases[i].expected_left);
        EXPECT_FLOAT_NEAR(fs.adc_right, swapped_cases[i].expected_right);
    }

    cfg.fault_adc1 = 0.0f;
    cfg.fault_adc2 = 0.0f;
    footpad_sensor_update(&fs, &cfg);
    EXPECT_TRUE(fs.state == FS_BOTH);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_NONE) == 0);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_LEFT) == 1);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_RIGHT) == 1);
    EXPECT_TRUE(footpad_sensor_state_to_switch_compat(FS_BOTH) == 2);

    return true;
}

static bool test_footpad_sensor_rejects_negative_thresholds(void) {
    // ConfigParams/XML and COMM_SET_CUSTOM_CONFIG over BLE preserve signed float16 values.
    const RefloatConfig cases[] = {
        {.fault_adc1 = -2.0f, .fault_adc2 = 1.0f},
        {.fault_adc1 = 1.0f, .fault_adc2 = -2.0f},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        vesc_if_fake_reset();
        vesc_if_fake_set_analog(-1.0f, -1.0f);
        FootpadSensor fs;
        footpad_sensor_init(&fs);

        footpad_sensor_update(&fs, &cases[i]);
        EXPECT_TRUE(fs.state == FS_NONE);
    }

    return true;
}
