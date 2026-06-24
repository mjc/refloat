static bool test_motor_data_refresh_update_and_alerts(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    EXPECT_FLOAT_NEAR(md.speed_constant, 0.0f);

    vesc_if_fake_set_cfg_int(CFG_PARAM_si_battery_cells, 15);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -30.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 45.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -12.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 18.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);

    motor_data_refresh_motor_config(&md, 3.2f, 4.2f);
    EXPECT_FLOAT_NEAR(md.lv_threshold, 48.0f);
    EXPECT_FLOAT_NEAR(md.hv_threshold, 63.0f);
    EXPECT_FLOAT_NEAR(md.current_min, 30.0f);
    EXPECT_FLOAT_NEAR(md.current_max, 45.0f);
    EXPECT_FLOAT_NEAR(md.battery_current_min, 12.0f);
    EXPECT_FLOAT_NEAR(md.battery_current_max, 18.0f);
    EXPECT_FLOAT_NEAR(md.mosfet_temp_max, 77.0f);
    EXPECT_FLOAT_NEAR(md.motor_temp_max, 87.0f);
    EXPECT_FLOAT_NEAR(md.duty_max_with_margin, 0.90f);
    EXPECT_TRUE(md.speed_constant > 0.0f);

    motor_data_configure(&md, 0.0f, 100.0f);
    vesc_if_fake_set_motor_telemetry(
        -1200.0f, 5.0f, 12.5f, -10.0f, -8.0f, -0.4f, -6.0f, 50.0f, 61.0f, 72.0f
    );
    motor_data_update(&md, 0.02f);

    EXPECT_FLOAT_NEAR(md.erpm, -1200.0f);
    EXPECT_FLOAT_NEAR(md.abs_erpm, 1200.0f);
    EXPECT_TRUE(md.erpm_sign == -1);
    EXPECT_FLOAT_NEAR(md.speed, 18.0f);
    EXPECT_FLOAT_NEAR(md.distance, 12.5f);
    EXPECT_TRUE(md.braking);
    EXPECT_TRUE(md.duty_raw >= 0.0f);
    EXPECT_TRUE(md.motor_current_saturation > 0.0f);
    EXPECT_TRUE(md.battery_current_saturation > 0.0f);
    EXPECT_FLOAT_NEAR(md.batt_voltage, 50.0f);
    EXPECT_FLOAT_NEAR(md.mosfet_temp, 61.0f);
    EXPECT_FLOAT_NEAR(md.motor_temp, 72.0f);
    EXPECT_FLOAT_NEAR(
        motor_data_get_current_saturation(&md),
        fmaxf(md.motor_current_saturation, md.battery_current_saturation)
    );

    float current = motor_data_torque_to_current(&md, 2.0f);
    EXPECT_FLOAT_NEAR(current, 2.0f * md.speed_constant);

    AlertTracker at;
    alert_tracker_init(&at);
    Time time = {.now = 1000u};
    vesc_if_fake_set_fault(FAULT_CODE_ABS_OVER_CURRENT);
    motor_data_evaluate_alerts(&md, &at, &time);
    EXPECT_TRUE(at.fatal_error);
    EXPECT_TRUE(at.fw_fault_code == FAULT_CODE_ABS_OVER_CURRENT);

    motor_data_destroy(&md);
    return true;
}

static bool test_motor_data_fallback_limits_and_direction_edges(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 0.5f, 100.0f);

    vesc_if_fake_set_cfg_int(CFG_PARAM_si_battery_cells, 0);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 50.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 55.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.8f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.0f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 0);

    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md.lv_threshold, 42.0f);
    EXPECT_FLOAT_NEAR(md.hv_threshold, 60.0f);
    EXPECT_FLOAT_NEAR(md.speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);
    EXPECT_FLOAT_NEAR(md.mosfet_temp_max, 47.0f);
    EXPECT_FLOAT_NEAR(md.motor_temp_max, 52.0f);
    EXPECT_FLOAT_NEAR(md.duty_max_with_margin, 0.75f);

    vesc_if_fake_set_motor_telemetry(
        0.0f, 0.0f, 1.0f, 12.0f, 20.0f, 0.25f, 8.0f, 54.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, 0.02f);
    EXPECT_FLOAT_NEAR(md.motor_current_saturation, 0.0f);
    EXPECT_FLOAT_NEAR(md.battery_current_saturation, 0.0f);
    EXPECT_TRUE(md.forward);

    md.filt_current.value = 25.0f;
    md.speed_constant = 1.0f;
    vesc_if_fake_set_motor_telemetry(
        -100.0f, 0.0f, 1.5f, 12.0f, 25.0f, 0.10f, -8.0f, 50.0f, 41.0f, 46.0f
    );
    for (size_t i = 0; i < 8; ++i) {
        motor_data_update(&md, 0.02f);
    }
    EXPECT_TRUE(md.torque > 18.0f);
    EXPECT_TRUE(md.forward);
    EXPECT_TRUE(!md.braking);
    EXPECT_FLOAT_NEAR(md.motor_current_saturation, 0.0f);
    EXPECT_FLOAT_NEAR(md.battery_current_saturation, 0.0f);

    motor_data_reset(&md);
    EXPECT_FLOAT_NEAR(md.duty_cycle.value, 0.0f);
    EXPECT_FLOAT_NEAR(md.acceleration.value, 0.0f);
    EXPECT_FLOAT_NEAR(md.filt_current.value, 0.0f);

    motor_data_destroy(&md);
    return true;
}

static bool test_motor_data_init_alert_and_saturation_edges(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    EXPECT_FLOAT_NEAR(md.erpm, 0.0f);
    EXPECT_FLOAT_NEAR(md.abs_erpm, 0.0f);
    EXPECT_FLOAT_NEAR(md.last_erpm, 0.0f);
    EXPECT_TRUE(md.erpm_sign == 1);
    EXPECT_TRUE(md.forward);
    EXPECT_TRUE(!md.braking);
    EXPECT_FLOAT_NEAR(md.motor_current_saturation, 0.0f);
    EXPECT_FLOAT_NEAR(md.battery_current_saturation, 0.0f);

    AlertTracker at;
    alert_tracker_init(&at);
    Time time = {.now = 2000u};
    vesc_if_fake_set_fault(FAULT_CODE_NONE);
    motor_data_evaluate_alerts(&md, &at, &time);
    EXPECT_TRUE(!at.fatal_error);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));

    motor_data_configure(&md, 200.0f, 100.0f);
    md.speed_constant = 1.0f;
    md.current_min = 10.0f;
    md.current_max = 20.0f;
    md.battery_current_min = 5.0f;
    md.battery_current_max = 1000.0f;

    vesc_if_fake_set_motor_telemetry(
        -300.0f, 1.0f, 0.25f, 12.0f, 30.0f, 0.2f, 20.0f, 55.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    EXPECT_TRUE(!md.forward);
    EXPECT_TRUE(!md.braking);

    md.motor_current_saturation = 0.7f;
    md.battery_current_saturation = 0.2f;
    EXPECT_FLOAT_NEAR(motor_data_get_current_saturation(&md), md.motor_current_saturation);

    md.current_min = 1000.0f;
    md.battery_current_min = 1.0f;
    vesc_if_fake_set_motor_telemetry(
        300.0f, 1.0f, 0.50f, -12.0f, -30.0f, 0.2f, -20.0f, 55.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    EXPECT_TRUE(md.forward);
    EXPECT_TRUE(md.braking);

    md.motor_current_saturation = 0.1f;
    md.battery_current_saturation = 0.8f;
    EXPECT_FLOAT_NEAR(motor_data_get_current_saturation(&md), md.battery_current_saturation);

    motor_data_destroy(&md);
    return true;
}

static bool test_motor_data_forward_direction_threshold_edges(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 20.0f, 100.0f);
    md.speed_constant = 1.0f;
    md.current_min = 0.0f;
    md.current_max = 0.0f;
    md.battery_current_min = 0.0f;
    md.battery_current_max = 0.0f;
    md.filt_current.a0 = 1.0f;
    md.filt_current.a1 = 0.0f;
    md.filt_current.a2 = 0.0f;
    md.filt_current.b1 = 0.0f;
    md.filt_current.b2 = 0.0f;
    md.filt_current.z1 = 0.0f;
    md.filt_current.z2 = 0.0f;

    vesc_if_fake_set_motor_telemetry(
        -250.0f, 0.0f, 0.0f, 18.0f, 18.0f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    EXPECT_FLOAT_NEAR(md.torque, 18.0f);
    EXPECT_TRUE(md.forward);

    vesc_if_fake_set_motor_telemetry(
        -251.0f, 0.0f, 0.0f, 18.0f, 18.0f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    EXPECT_FLOAT_NEAR(md.torque, 18.0f);
    EXPECT_TRUE(!md.forward);

    vesc_if_fake_set_motor_telemetry(
        -250.0f, 0.0f, 0.0f, 17.99f, 17.99f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    EXPECT_TRUE(md.torque < 18.0f);
    EXPECT_TRUE(!md.forward);

    motor_data_destroy(&md);
    return true;
}

static bool test_motor_data_torque_constant_config_edges(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);

    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md.speed_constant, 1.0f / (1.5f * 0.5f * 14.0f * 0.006f));
    EXPECT_FLOAT_NEAR(motor_data_torque_to_current(&md, 3.0f), 3.0f * md.speed_constant);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.012f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 20);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md.speed_constant, 1.0f / (1.5f * 0.5f * 20.0f * 0.012f));

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.001f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 20);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md.speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.012f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 0);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md.speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);

    motor_data_destroy(&md);
    return true;
}

static bool test_motor_data_erpm_speed_conversion(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 20.0f, 100.0f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -30.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 45.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -12.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 18.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_gear_ratio, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_wheel_diameter, 0.28f);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);

    vesc_if_fake_set_motor_telemetry(
        1200.0f, 0.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, 0.02f);

    float mechanical_rpm = 1200.0f / (14.0f * 0.5f);
    float expected_kph = mechanical_rpm * (float) M_PI * 0.28f * 60.0f / 1000.0f;
    EXPECT_FLOAT_NEAR(md.speed, expected_kph);

    motor_data_destroy(&md);
    return true;
}

static bool test_motor_data_nonpositive_dt(void) {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 20.0f, 100.0f);
    md.speed_constant = 1.0f;
    md.last_erpm = 1000.0f;

    vesc_if_fake_set_motor_telemetry(
        1500.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );

    motor_data_update(&md, 0.0f);
    EXPECT_TRUE(isfinite(md.acceleration.value));

    vesc_if_fake_set_motor_telemetry(
        1250.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, -0.02f);
    EXPECT_TRUE(isfinite(md.acceleration.value));

    vesc_if_fake_set_motor_telemetry(
        1750.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, NAN);
    EXPECT_TRUE(isfinite(md.acceleration.value));

    motor_data_destroy(&md);
    return true;
}
