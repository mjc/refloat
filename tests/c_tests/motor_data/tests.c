typedef struct {
    MotorData md;
    AlertTracker at;
    Time time;
} MotorDataFixture;

static void motor_data_fixture_start(MotorDataFixture *fixture) {
    vesc_if_fake_reset();

    *fixture = (MotorDataFixture){0};
    motor_data_init(&fixture->md);
    alert_tracker_init(&fixture->at);
    fixture->time.now = 1000u;
}

static void motor_data_fixture_configure(
    MotorDataFixture *fixture, float current_min, float current_max
) {
    motor_data_configure(&fixture->md, current_min, current_max);
}

static void motor_data_fixture_destroy(MotorDataFixture *fixture) {
    motor_data_destroy(&fixture->md);
}

static bool test_motor_data_refresh_update_and_alerts(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    EXPECT_FLOAT_NEAR(fixture.md.speed_constant, 0.0f);

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

    motor_data_refresh_motor_config(&fixture.md, 3.2f, 4.2f);
    EXPECT_FLOAT_NEAR(fixture.md.lv_threshold, 48.0f);
    EXPECT_FLOAT_NEAR(fixture.md.hv_threshold, 63.0f);
    EXPECT_FLOAT_NEAR(fixture.md.current_min, 30.0f);
    EXPECT_FLOAT_NEAR(fixture.md.current_max, 45.0f);
    EXPECT_FLOAT_NEAR(fixture.md.battery_current_min, 12.0f);
    EXPECT_FLOAT_NEAR(fixture.md.battery_current_max, 18.0f);
    EXPECT_FLOAT_NEAR(fixture.md.mosfet_temp_max, 77.0f);
    EXPECT_FLOAT_NEAR(fixture.md.motor_temp_max, 87.0f);
    EXPECT_FLOAT_NEAR(fixture.md.duty_max_with_margin, 0.90f);
    EXPECT_TRUE(fixture.md.speed_constant > 0.0f);

    motor_data_fixture_configure(&fixture, 0.0f, 100.0f);
    vesc_if_fake_set_motor_telemetry(
        -1200.0f, 5.0f, 12.5f, -10.0f, -8.0f, -0.4f, -6.0f, 50.0f, 61.0f, 72.0f
    );
    motor_data_update(&fixture.md, 0.02f);

    EXPECT_FLOAT_NEAR(fixture.md.erpm, -1200.0f);
    EXPECT_FLOAT_NEAR(fixture.md.abs_erpm, 1200.0f);
    EXPECT_TRUE(fixture.md.erpm_sign == -1);
    EXPECT_FLOAT_NEAR(fixture.md.speed, 18.0f);
    EXPECT_FLOAT_NEAR(fixture.md.distance, 12.5f);
    EXPECT_TRUE(fixture.md.braking);
    EXPECT_TRUE(fixture.md.duty_raw >= 0.0f);
    EXPECT_TRUE(fixture.md.motor_current_saturation > 0.0f);
    EXPECT_TRUE(fixture.md.battery_current_saturation > 0.0f);
    EXPECT_FLOAT_NEAR(fixture.md.batt_voltage, 50.0f);
    EXPECT_FLOAT_NEAR(fixture.md.mosfet_temp, 61.0f);
    EXPECT_FLOAT_NEAR(fixture.md.motor_temp, 72.0f);
    EXPECT_FLOAT_NEAR(
        motor_data_get_current_saturation(&fixture.md),
        fmaxf(fixture.md.motor_current_saturation, fixture.md.battery_current_saturation)
    );

    float current = motor_data_torque_to_current(&fixture.md, 2.0f);
    EXPECT_FLOAT_NEAR(current, 2.0f * fixture.md.speed_constant);

    vesc_if_fake_set_fault(FAULT_CODE_ABS_OVER_CURRENT);
    motor_data_evaluate_alerts(&fixture.md, &fixture.at, &fixture.time);
    EXPECT_TRUE(fixture.at.fatal_error);
    EXPECT_TRUE(fixture.at.fw_fault_code == FAULT_CODE_ABS_OVER_CURRENT);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_falls_back_to_limits_and_direction(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;
    motor_data_fixture_configure(&fixture, 0.5f, 100.0f);

    // VESC Tool constrains this field to 1..255. COMM_SET_MCCONF still
    // serializes it as a byte, so zero is a malformed but protocol-reachable
    // raw value; negative values are not a useful VESC Tool bug input.
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

    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->lv_threshold, 42.0f);
    EXPECT_FLOAT_NEAR(md->hv_threshold, 60.0f);
    EXPECT_FLOAT_NEAR(md->speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);
    EXPECT_FLOAT_NEAR(md->mosfet_temp_max, 47.0f);
    EXPECT_FLOAT_NEAR(md->motor_temp_max, 52.0f);
    EXPECT_FLOAT_NEAR(md->duty_max_with_margin, 0.75f);

    vesc_if_fake_set_cfg_int(CFG_PARAM_si_battery_cells, 15);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->lv_threshold, 42.0f);
    EXPECT_FLOAT_NEAR(md->hv_threshold, 60.0f);

    vesc_if_fake_set_motor_telemetry(
        0.0f, NAN, NAN, 12.0f, 20.0f, 0.25f, 8.0f, 54.0f, 40.0f, 45.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_FLOAT_NEAR(md->speed, 0.0f);
    EXPECT_FLOAT_NEAR(md->distance, 0.0f);
    EXPECT_FLOAT_NEAR(md->motor_current_saturation, 0.0f);
    EXPECT_FLOAT_NEAR(md->battery_current_saturation, 0.0f);
    EXPECT_TRUE(md->forward);

    md->filt_current.value = 25.0f;
    md->speed_constant = 1.0f;
    vesc_if_fake_set_motor_telemetry(
        -100.0f, 0.0f, 1.5f, 12.0f, 25.0f, 0.10f, -8.0f, 50.0f, 41.0f, 46.0f
    );
    for (size_t i = 0; i < 8; ++i) {
        motor_data_update(md, 0.02f);
    }
    EXPECT_TRUE(md->torque > 18.0f);
    EXPECT_TRUE(md->forward);
    EXPECT_TRUE(!md->braking);
    EXPECT_FLOAT_NEAR(md->motor_current_saturation, 0.0f);
    EXPECT_FLOAT_NEAR(md->battery_current_saturation, 0.0f);

    motor_data_reset(md);
    EXPECT_FLOAT_NEAR(md->duty_cycle.value, 0.0f);
    EXPECT_FLOAT_NEAR(md->acceleration.value, 0.0f);
    EXPECT_FLOAT_NEAR(md->filt_current.value, 0.0f);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_initializes_alerts_and_saturates_fields(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;
    EXPECT_FLOAT_NEAR(md->erpm, 0.0f);
    EXPECT_FLOAT_NEAR(md->abs_erpm, 0.0f);
    EXPECT_FLOAT_NEAR(md->last_erpm, 0.0f);
    EXPECT_TRUE(md->erpm_sign == 1);
    EXPECT_TRUE(md->forward);
    EXPECT_TRUE(!md->braking);
    EXPECT_FLOAT_NEAR(md->motor_current_saturation, 0.0f);
    EXPECT_FLOAT_NEAR(md->battery_current_saturation, 0.0f);

    vesc_if_fake_set_fault(FAULT_CODE_NONE);
    fixture.time.now = 2000u;
    motor_data_evaluate_alerts(md, &fixture.at, &fixture.time);
    EXPECT_TRUE(!fixture.at.fatal_error);
    EXPECT_TRUE(!alert_tracker_is_alert_active(&fixture.at, ALERT_FW_FAULT));

    motor_data_fixture_configure(&fixture, 200.0f, 100.0f);
    md->speed_constant = 1.0f;
    md->current_min = 10.0f;
    md->current_max = 20.0f;
    md->battery_current_min = 5.0f;
    md->battery_current_max = 1000.0f;

    vesc_if_fake_set_motor_telemetry(
        -300.0f, 1.0f, 0.25f, 12.0f, 30.0f, 0.2f, 20.0f, 55.0f, 40.0f, 41.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_TRUE(!md->forward);
    EXPECT_TRUE(!md->braking);

    md->motor_current_saturation = 0.7f;
    md->battery_current_saturation = 0.2f;
    EXPECT_FLOAT_NEAR(motor_data_get_current_saturation(md), md->motor_current_saturation);

    md->current_min = 1000.0f;
    md->battery_current_min = 1.0f;
    vesc_if_fake_set_motor_telemetry(
        300.0f, 1.0f, 0.50f, -12.0f, -30.0f, 0.2f, -20.0f, 55.0f, 40.0f, 41.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_TRUE(md->forward);
    EXPECT_TRUE(md->braking);

    md->motor_current_saturation = 0.1f;
    md->battery_current_saturation = 0.8f;
    EXPECT_FLOAT_NEAR(motor_data_get_current_saturation(md), md->battery_current_saturation);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_derives_forward_direction_from_speed(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;
    motor_data_fixture_configure(&fixture, 20.0f, 100.0f);
    md->speed_constant = 1.0f;
    md->current_min = 0.0f;
    md->current_max = 0.0f;
    md->battery_current_min = 0.0f;
    md->battery_current_max = 0.0f;
    md->filt_current.a0 = 1.0f;
    md->filt_current.a1 = 0.0f;
    md->filt_current.a2 = 0.0f;
    md->filt_current.b1 = 0.0f;
    md->filt_current.b2 = 0.0f;
    md->filt_current.z1 = 0.0f;
    md->filt_current.z2 = 0.0f;

    vesc_if_fake_set_motor_telemetry(
        -250.0f, 0.0f, 0.0f, 18.0f, 18.0f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_FLOAT_NEAR(md->torque, 18.0f);
    EXPECT_TRUE(md->forward);

    vesc_if_fake_set_motor_telemetry(
        -251.0f, 0.0f, 0.0f, 18.0f, 18.0f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_FLOAT_NEAR(md->torque, 18.0f);
    EXPECT_TRUE(!md->forward);

    vesc_if_fake_set_motor_telemetry(
        -250.0f, 0.0f, 0.0f, 17.99f, 17.99f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_TRUE(md->torque < 18.0f);
    EXPECT_TRUE(!md->forward);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_applies_torque_constant_configuration(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;

    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->speed_constant, 1.0f / (1.5f * 0.5f * 14.0f * 0.006f));
    EXPECT_FLOAT_NEAR(motor_data_torque_to_current(md, 3.0f), 3.0f * md->speed_constant);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.012f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 20);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->speed_constant, 1.0f / (1.5f * 0.5f * 20.0f * 0.012f));

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.001f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 20);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.012f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 0);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, INFINITY);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_gear_ratio, INFINITY);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_wheel_diameter, 0.28f);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);
    EXPECT_FLOAT_NEAR(md->erpm_to_speed, 0.0f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_si_gear_ratio, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_wheel_diameter, NAN);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->erpm_to_speed, 0.0f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_si_wheel_diameter, 0.0f);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    EXPECT_FLOAT_NEAR(md->erpm_to_speed, 0.0f);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_rejects_nonfinite_vesc_limits(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;

    // BLDC's Lisp C API writes these values directly; its hardware-limit
    // helper does not change NaN because neither comparison succeeds.
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, NAN);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, NAN);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, NAN);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, NAN);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, NAN);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, NAN);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, NAN);

    motor_data_refresh_motor_config(md, 42.0f, 60.0f);

    EXPECT_FLOAT_NEAR(md->current_min, 0.0f);
    EXPECT_FLOAT_NEAR(md->current_max, 0.0f);
    EXPECT_FLOAT_NEAR(md->battery_current_min, 0.0f);
    EXPECT_FLOAT_NEAR(md->battery_current_max, 0.0f);
    EXPECT_FLOAT_NEAR(md->mosfet_temp_max, 0.0f);
    EXPECT_FLOAT_NEAR(md->motor_temp_max, 0.0f);
    EXPECT_FLOAT_NEAR(md->duty_max_with_margin, 0.0f);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_erpm_speed_conversion(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;
    motor_data_fixture_configure(&fixture, 20.0f, 100.0f);

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
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);

    vesc_if_fake_set_motor_telemetry(
        1200.0f, 0.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(md, 0.02f);

    float mechanical_rpm = 1200.0f / (14.0f * 0.5f);
    float expected_kph = mechanical_rpm * (float) M_PI * 0.28f * 60.0f / 1000.0f;
    EXPECT_FLOAT_NEAR(md->speed, expected_kph);

    // VESC Tool permits a zero gear ratio. BLDC then exposes nonfinite speed
    // and distance, so retain the last valid Refloat values.
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_gear_ratio, 0.0f);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    vesc_if_fake_set_motor_telemetry(
        1200.0f, NAN, NAN, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_FLOAT_NEAR(md->speed, expected_kph);
    EXPECT_FLOAT_NEAR(md->distance, 1.0f);

    // Refloat's vTx 3 ERPM settings wrap 100000 to 34464. On a 30-pole
    // Superflux HS with a nominal 12-inch direct-drive tire, that becomes
    // relevant around 82 mph, not the reported 60 mph operating point.
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 30);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_gear_ratio, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_wheel_diameter, 0.3048f);
    motor_data_refresh_motor_config(md, 42.0f, 60.0f);
    vesc_if_fake_set_motor_telemetry(
        34464.0f, 0.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_FLOAT_NEAR_EPS(md->speed / 1.609344f, 82.03f, 0.01f);

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_nonpositive_dt(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;
    motor_data_fixture_configure(&fixture, 20.0f, 100.0f);
    md->speed_constant = 1.0f;
    md->last_erpm = 1000.0f;

    vesc_if_fake_set_motor_telemetry(
        1500.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );

    motor_data_update(md, 0.0f);
    EXPECT_TRUE(isfinite(md->acceleration.value));

    vesc_if_fake_set_motor_telemetry(
        1250.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(md, -0.02f);
    EXPECT_TRUE(isfinite(md->acceleration.value));

    vesc_if_fake_set_motor_telemetry(
        1750.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(md, NAN);
    EXPECT_TRUE(isfinite(md->acceleration.value));

    motor_data_fixture_destroy(&fixture);
    return true;
}

static bool test_motor_data_bounds_current_filter_cutoff(void) {
    MotorData bounded;
    MotorData expected;
    motor_data_init(&bounded);
    motor_data_init(&expected);

    motor_data_configure(&bounded, 327.0f, 100.0f);
    motor_data_configure(&expected, 20.0f, 100.0f);

    EXPECT_FLOAT_NEAR(bounded.filt_current.a0, expected.filt_current.a0);
    EXPECT_FLOAT_NEAR(bounded.filt_current.a1, expected.filt_current.a1);
    EXPECT_FLOAT_NEAR(bounded.filt_current.a2, expected.filt_current.a2);
    EXPECT_FLOAT_NEAR(bounded.filt_current.b1, expected.filt_current.b1);
    EXPECT_FLOAT_NEAR(bounded.filt_current.b2, expected.filt_current.b2);
    motor_data_destroy(&bounded);
    motor_data_destroy(&expected);
    return true;
}

static bool test_motor_data_invalid_telemetry_is_quarantined(void) {
    MotorDataFixture fixture;
    motor_data_fixture_start(&fixture);
    MotorData *md = &fixture.md;
    motor_data_fixture_configure(&fixture, 20.0f, 100.0f);
    md->speed_constant = 1.0f;

    vesc_if_fake_set_motor_telemetry(
        1000.0f, 2.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(md, 0.02f);
    const size_t invalid_fields[] = {0, 3, 4, 5, 6, 7, 8, 9};
    for (size_t i = 0; i < sizeof(invalid_fields) / sizeof(invalid_fields[0]); ++i) {
        float values[] = {1100.0f, 2.5f, 1.5f, 5.0f, 5.0f, 0.2f, 3.0f, 51.0f, 41.0f, 46.0f};
        values[invalid_fields[i]] = NAN;
        vesc_if_fake_set_motor_telemetry(
            values[0],
            values[1],
            values[2],
            values[3],
            values[4],
            values[5],
            values[6],
            values[7],
            values[8],
            values[9]
        );
        motor_data_update(md, 0.02f);

        EXPECT_TRUE(isfinite(md->erpm));
        EXPECT_TRUE(isfinite(md->current));
        EXPECT_TRUE(isfinite(md->dir_current));
        EXPECT_TRUE(isfinite(md->duty_raw));
        EXPECT_TRUE(isfinite(md->batt_current.value));
        EXPECT_TRUE(isfinite(md->batt_voltage));
        EXPECT_TRUE(isfinite(md->mosfet_temp));
        EXPECT_TRUE(isfinite(md->motor_temp));
    }

    float torque = md->torque;
    md->speed_constant = 0.0f;
    vesc_if_fake_set_motor_telemetry(
        1100.0f, 2.5f, 1.5f, 5.0f, 5.0f, 0.2f, 3.0f, 51.0f, 41.0f, 46.0f
    );
    motor_data_update(md, 0.02f);
    EXPECT_FLOAT_NEAR(md->torque, torque);
    motor_data_fixture_destroy(&fixture);
    return true;
}
