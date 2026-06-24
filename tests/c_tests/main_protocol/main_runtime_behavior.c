static void main_test_thread_set_priority(int priority) {
    (void) priority;
}

static bool test_main_can_engage_conditions(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.charging = true;
    d->footpad.state = FS_BOTH;
    EXPECT_TRUE(!refloat_main_can_engage(d));

    d->state.charging = false;
    EXPECT_TRUE(refloat_main_can_engage(d));

    d->footpad.state = FS_LEFT;
    d->float_conf.fault_is_dual_switch = false;
    d->float_conf.startup_simplestart_enabled = false;
    d->state.mode = MODE_NORMAL;
    EXPECT_TRUE(!refloat_main_can_engage(d));

    d->float_conf.fault_is_dual_switch = true;
    EXPECT_TRUE(refloat_main_can_engage(d));

    d->float_conf.fault_is_dual_switch = false;
    d->float_conf.startup_simplestart_enabled = true;
    d->time.now = 5000;
    d->time.disengage_timer = 0;
    EXPECT_TRUE(refloat_main_can_engage(d));

    d->time.disengage_timer = d->time.now;
    d->time.engage_timer = d->time.now;
    EXPECT_TRUE(refloat_main_can_engage(d));

    d->float_conf.startup_simplestart_enabled = false;
    d->state.mode = MODE_FLYWHEEL;
    EXPECT_TRUE(refloat_main_can_engage(d));

    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_NONE;
    EXPECT_TRUE(!refloat_main_can_engage(d));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_fault_stop_conditions(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->time.now = 10000;
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_NONE;
    d->float_conf.fault_delay_switch_full = 0;
    timer_expire(&d->time, &d->fault_switch_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_SWITCH_FULL);

    d->state.stop_condition = STOP_NONE;
    d->footpad.state = FS_BOTH;
    d->imu.roll = 0;
    d->imu.pitch = 61;
    d->remote.setpoint.value = 0;
    d->float_conf.fault_delay_pitch = 0;
    timer_expire(&d->time, &d->fault_angle_pitch_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_PITCH);

    d->state.stop_condition = STOP_NONE;
    d->state.darkride = true;
    d->motor.erpm = 2501;
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_REVERSE_STOP);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_flywheel_and_quickstop_faults(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->time.now = 10000;
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_NONE;
    d->float_conf.enable_quickstop = true;
    d->motor.abs_erpm = 100;
    d->motor.erpm_sign = 1;
    d->imu.pitch = 20;
    d->remote.setpoint.value = 0;
    timer_refresh(&d->time, &d->fault_switch_timer);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_QUICKSTOP);

    d->state.stop_condition = STOP_NONE;
    d->state.mode = MODE_FLYWHEEL;
    d->flywheel_allow_abort = true;
    d->footpad.state = FS_LEFT;
    d->imu.pitch = 0;
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_TRUE(d->flywheel_abort);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_state_flag_encoding(void) {
    State state = {
        .state = STATE_RUNNING,
        .mode = MODE_FLYWHEEL,
        .sat = SAT_PB_DUTY,
        .stop_condition = STOP_QUICKSTOP,
        .charging = true,
        .wheelslip = true,
        .darkride = true,
    };
    FootpadSensor footpad = {.state = FS_RIGHT};
    AlertTracker alerts = {.fatal_error = true};
    DataRecord record = {.recording = true};

    EXPECT_EQ_U32(refloat_main_encode_extra_flags(&record), 1u);
    EXPECT_EQ_U32(refloat_main_encode_state_flags(&state, &footpad, &alerts, 0xa5u), 0x23b366a5u);
    return true;
}

static bool test_main_reset_and_engage_runtime_state(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->time.now = 1234;
    d->imu.balance_pitch = 3.5f;
    d->setpoint = -4.0f;
    d->setpoint_target = 8.0f;
    d->setpoint_target_interpolated = -2.0f;
    d->noseangling_interpolated = 1.0f;
    d->traction_control = true;
    d->softstart_pid_limit = 25.0f;
    d->float_conf.startup_pitch_tolerance = 7.0f;

    refloat_main_reset_runtime_vars(d);
    EXPECT_FLOAT_NEAR(d->setpoint, 3.5f);
    EXPECT_FLOAT_NEAR(d->setpoint_target_interpolated, 3.5f);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 0.0f);
    EXPECT_FLOAT_NEAR(d->noseangling_interpolated, 0.0f);
    EXPECT_TRUE(!d->traction_control);
    EXPECT_FLOAT_NEAR(d->softstart_pid_limit, 0.0f);
    EXPECT_FLOAT_NEAR(d->startup_pitch_tolerance, 7.0f);

    d->state.state = STATE_READY;
    refloat_main_engage(d);
    EXPECT_EQ_U32(d->state.state, STATE_RUNNING);
    EXPECT_EQ_U32(d->time.engage_timer, 1234u);

    d->state.darkride = true;
    d->enable_upside_down = true;
    d->is_upside_down_started = true;
    d->float_conf.fault_darkride_enabled = false;
    refloat_main_reconfigure(d);
    EXPECT_TRUE(!d->state.darkride);
    EXPECT_TRUE(!d->enable_upside_down);
    EXPECT_TRUE(!d->is_upside_down_started);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_pid_control_modes_and_safety_limits(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->motor.current_max = 8.0f;
    d->motor.current_min = -6.0f;
    d->softstart_pid_limit = 1.0f;
    d->setpoint = 1.0f;
    d->imu.pitch = 0.0f;

    d->state.mode = MODE_HANDTEST;
    refloat_main_pid_control(d, 0.01f);
    EXPECT_TRUE(isfinite(d->balance_current.value));

    d->state.mode = MODE_FLYWHEEL;
    d->state.darkride = true;
    refloat_main_pid_control(d, 0.01f);
    EXPECT_TRUE(isfinite(d->balance_current.value));

    d->state.mode = MODE_NORMAL;
    d->state.darkride = false;
    d->motor.braking = true;
    d->traction_control = true;
    refloat_main_pid_control(d, 0.01f);
    EXPECT_FLOAT_NEAR(d->balance_current.value, 0.0f);

    d->traction_control = false;
    d->motor.braking = false;
    d->motor.current_max = 1.0f;
    d->motor.current_min = 1.0f;
    d->softstart_pid_limit = 0.1f;
    d->setpoint = -30.0f;
    d->imu.balance_pitch = 0.0f;
    d->imu.pitch_rate = 20.0f;
    refloat_main_pid_control(d, 0.01f);
    EXPECT_TRUE(d->balance_current.value < 0.0f);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static void main_setpoint_target_prepare(Data *d) {
    d->state.state = STATE_RUNNING;
    d->state.mode = MODE_NORMAL;
    d->state.sat = SAT_NONE;
    d->state.wheelslip = false;
    d->state.darkride = false;
    d->state.stop_condition = STOP_NONE;
    d->motor.batt_voltage = 50.0f;
    d->motor.hv_threshold = 60.0f;
    d->motor.lv_threshold = 40.0f;
    d->motor.mosfet_temp = 20.0f;
    d->motor.mosfet_temp_max = 80.0f;
    d->motor.motor_temp = 20.0f;
    d->motor.motor_temp_max = 80.0f;
    d->motor.erpm = 1000.0f;
    d->motor.erpm_sign = 1;
    d->motor.abs_erpm = 1000.0f;
    d->motor.speed = 0.0f;
    d->motor.dir_current = 10.0f;
    d->motor.duty_cycle.value = 0.0f;
    d->motor.duty_raw = 0.0f;
    d->motor.duty_max_with_margin = 0.9f;
    d->motor.acceleration.value = 0.0f;
    d->float_conf.tiltback_duty = 0.8f;
    d->float_conf.tiltback_duty_angle = 5.0f;
    d->float_conf.tiltback_hv_angle = 6.0f;
    d->float_conf.tiltback_lv_angle = 4.0f;
    d->float_conf.tiltback_speed = 5.0f;
    d->float_conf.is_dutybeep_enabled = true;
    d->float_conf.fault_reversestop_enabled = false;
    d->bms.fault_mask = BMSF_NONE;
    d->beeper_enabled = false;
    d->duty_beeping = false;
    d->time.now = 10000;
}

static bool test_main_setpoint_target_enforces_safety_limits(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    main_setpoint_target_prepare(d);
    d->state.sat = SAT_CENTERING;
    d->setpoint_target = 0.0f;
    d->setpoint_target_interpolated = 0.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->float_conf.tiltback_duty = -1.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 0.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.9f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_DUTY);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 5.0f);
    EXPECT_TRUE(d->duty_beeping);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.9f;
    d->motor.erpm = -1000.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, -5.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 62.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_HIGH_VOLTAGE);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 6.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 62.0f;
    d->motor.erpm = -1000.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, -6.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 61.0f;
    d->bms.fault_mask = 1u << (BMSF_CELL_OVER_VOLTAGE - 1);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->beep_reason > 0);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 60.5f;
    d->tb_highvoltage_timer = d->time.now;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = d->motor.hv_threshold + 0.5f;
    timer_expire(&d->time, &d->tb_highvoltage_timer, 1.0f);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_HIGH_VOLTAGE);

    main_setpoint_target_prepare(d);
    d->bms.fault_mask = 1u << (BMSF_CONNECTION - 1);
    d->motor.erpm = -1000.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_ERROR);
    EXPECT_TRUE(d->beep_reason > 0);

    main_setpoint_target_prepare(d);
    d->bms.fault_mask = 1u << (BMSF_CONNECTION - 1);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 6.0f);

    main_setpoint_target_prepare(d);
    d->motor.mosfet_temp = 80.5f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->motor.mosfet_temp = 82.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_TEMPERATURE);
    EXPECT_TRUE(d->beep_reason > 0);

    main_setpoint_target_prepare(d);
    d->motor.motor_temp = 80.5f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->motor.motor_temp = 82.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_TEMPERATURE);
    EXPECT_TRUE(d->beep_reason > 0);

    const BMSFaultCode bms_temperature_faults[] = {
        BMSF_CELL_OVER_TEMP,
        BMSF_CELL_UNDER_TEMP,
        BMSF_OVER_TEMP,
    };
    for (size_t i = 0; i < sizeof(bms_temperature_faults) / sizeof(bms_temperature_faults[0]);
         ++i) {
        main_setpoint_target_prepare(d);
        d->bms.fault_mask = 1u << (bms_temperature_faults[i] - 1);
        d->motor.erpm = -1000.0f;
        refloat_main_calculate_setpoint_target(d);
        EXPECT_EQ_U32(d->state.sat, SAT_PB_TEMPERATURE);
        EXPECT_TRUE(d->beep_reason > 0);
    }

    main_setpoint_target_prepare(d);
    d->bms.fault_mask = 1u << (BMSF_CELL_OVER_TEMP - 1);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 4.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 37.0f;
    d->motor.erpm = -1000.0f;
    d->motor.erpm_sign = -1;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_LOW_VOLTAGE);
    EXPECT_FLOAT_NEAR(d->setpoint_target, -4.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 39.0f;
    d->motor.dir_current = 30.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 50.0f;
    d->bms.fault_mask = 1u << (BMSF_CELL_UNDER_VOLTAGE - 1);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_LOW_VOLTAGE);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 37.0f;
    d->motor.dir_current = 30.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 4.0f);

    main_setpoint_target_prepare(d);
    d->motor.motor_temp = 82.0f;
    d->motor.erpm = -1000.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, -4.0f);

    main_setpoint_target_prepare(d);
    d->motor.speed = -10.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_SPEED);
    EXPECT_FLOAT_NEAR(d->setpoint_target, -5.0f);

    main_setpoint_target_prepare(d);
    d->motor.speed = 10.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 5.0f);

    main_setpoint_target_prepare(d);
    d->motor.acceleration.value = 11001.0f;
    d->motor.duty_cycle.value = 0.5f;
    d->motor.abs_erpm = 3000.0f;
    d->state.darkride = true;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->state.wheelslip);
    EXPECT_TRUE(d->traction_control);
    EXPECT_TRUE(d->is_upside_down_started);

    d->motor.acceleration.value = 1000.0f;
    d->motor.duty_cycle.value = 0.1f;
    d->motor.duty_raw = 0.5f;
    d->time.now += 3000;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(!d->state.wheelslip);
    EXPECT_TRUE(!d->traction_control);

    main_setpoint_target_prepare(d);
    d->motor.acceleration.value = 11001.0f;
    d->motor.duty_cycle.value = 0.5f;
    d->motor.abs_erpm = 3000.0f;
    refloat_main_calculate_setpoint_target(d);
    d->motor.acceleration.value = 1000.0f;
    d->motor.duty_cycle.value = 0.95f;
    d->motor.duty_raw = 0.9f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 0.0f);

    main_setpoint_target_prepare(d);
    d->reverse_stop.target_setpoint = 17.0f;
    d->reverse_stop.start_setpoint = 0.0f;
    d->reverse_stop.progress.value = 0.5f;
    d->state.sat = SAT_REVERSESTOP;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_REVERSESTOP);
    EXPECT_TRUE(d->setpoint_target > 0.0f);

    main_setpoint_target_prepare(d);
    d->state.sat = SAT_REVERSESTOP;
    d->reverse_stop.progress.value = 1.0f;
    d->reverse_stop.target_setpoint = 0.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->float_conf.fault_reversestop_enabled = true;
    d->reverse_stop.target_setpoint = 17.0f;
    d->reverse_stop.start_setpoint = 0.0f;
    d->reverse_stop.progress.value = 0.5f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_REVERSESTOP);

    main_setpoint_target_prepare(d);
    d->duty_beeping = true;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(!d->duty_beeping);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_setpoint_target_handles_guard_transitions(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    main_setpoint_target_prepare(d);
    d->state.sat = SAT_CENTERING;
    d->setpoint_target = 1.0f;
    d->setpoint_target_interpolated = 0.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_CENTERING);

    main_setpoint_target_prepare(d);
    d->bms.fault_mask = 1u << (BMSF_CELL_OVER_VOLTAGE - 1);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->float_conf.fault_reversestop_enabled = true;
    d->reverse_stop.target_setpoint = 17.0f;
    d->reverse_stop.progress.value = 0.5f;
    d->state.darkride = true;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->state.sat != SAT_REVERSESTOP);

    main_setpoint_target_prepare(d);
    d->float_conf.fault_reversestop_enabled = true;
    d->reverse_stop.target_setpoint = 0.0f;
    d->reverse_stop.progress.value = 1.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    static const struct {
        Mode mode;
        float acceleration;
        int8_t erpm_sign;
        float duty;
        float abs_erpm;
    } wheelslip_edges[] = {
        {MODE_FLYWHEEL, 11001, 1, 0.5f, 3000},
        {MODE_NORMAL, 10000, 1, 0.5f, 3000},
        {MODE_NORMAL, 11001, -1, 0.5f, 3000},
        {MODE_NORMAL, 11001, 1, 0.29f, 3000},
        {MODE_NORMAL, 11001, 1, 0.5f, 2000},
    };
    for (size_t i = 0; i < sizeof(wheelslip_edges) / sizeof(wheelslip_edges[0]); ++i) {
        main_setpoint_target_prepare(d);
        d->state.mode = wheelslip_edges[i].mode;
        d->motor.acceleration.value = wheelslip_edges[i].acceleration;
        d->motor.erpm_sign = wheelslip_edges[i].erpm_sign;
        d->motor.duty_cycle.value = wheelslip_edges[i].duty;
        d->motor.abs_erpm = wheelslip_edges[i].abs_erpm;
        refloat_main_calculate_setpoint_target(d);
        EXPECT_TRUE(!d->state.wheelslip);
    }

    main_setpoint_target_prepare(d);
    d->state.wheelslip = true;
    d->motor.acceleration.value = 8000;
    d->motor.duty_cycle.value = 0.1f;
    d->wheelslip_timer = d->time.now;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->state.wheelslip);

    main_setpoint_target_prepare(d);
    d->state.wheelslip = true;
    d->motor.acceleration.value = 8000;
    d->motor.duty_cycle.value = 0.9f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->state.wheelslip);

    main_setpoint_target_prepare(d);
    d->state.wheelslip = true;
    d->motor.acceleration.value = 8000;
    d->motor.duty_cycle.value = 0.1f;
    d->motor.duty_raw = 0.9f;
    timer_expire(&d->time, &d->wheelslip_timer, 1.0f);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->state.wheelslip);

    main_setpoint_target_prepare(d);
    d->state.mode = MODE_FLYWHEEL;
    d->motor.duty_cycle.value = 0.9f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 5.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->bms.fault_mask = 1u << (BMSF_CELL_OVER_VOLTAGE - 1);
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_HIGH_VOLTAGE);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = d->motor.hv_threshold + 0.5f;
    d->tb_highvoltage_timer = d->time.now;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = d->motor.lv_threshold - 0.5f;
    d->motor.dir_current = 4.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_LOW_VOLTAGE);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = d->motor.lv_threshold - 1.0f;
    d->motor.dir_current = 10.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_LOW_VOLTAGE);

    main_setpoint_target_prepare(d);
    d->float_conf.is_dutybeep_enabled = false;
    d->float_conf.tiltback_duty_angle = 0.0f;
    d->motor.duty_cycle.value = 0.9f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(d->duty_beeping);

    main_setpoint_target_prepare(d);
    d->float_conf.is_dutybeep_enabled = false;
    d->motor.duty_cycle.value = 0.9f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_TRUE(!d->duty_beeping);

    main_setpoint_target_prepare(d);
    d->float_conf.tiltback_speed = 0;
    d->motor.speed = 10;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_NONE);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_eeprom_config_round_trip(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);
    d->float_conf.kp = 1.2f;
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_TRUE(vesc_if_fake_store_eeprom_var_calls() > 0);

    d->float_conf.kp = -9.0f;
    refloat_main_read_cfg_from_eeprom(d);
    EXPECT_FLOAT_NEAR(d->float_conf.kp, 1.2f);
    EXPECT_TRUE(vesc_if_fake_read_eeprom_var_calls() > 0);

    vesc_if_fake_set_eeprom_read_enabled(false);
    d->float_conf.kp = -9.0f;
    refloat_main_read_cfg_from_eeprom(d);
    RefloatConfig defaults = {0};
    confparser_set_defaults_refloatconfig(&defaults);
    EXPECT_FLOAT_NEAR(d->float_conf.kp, defaults.kp);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_rejects_zero_variable_tiltback_rate(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->float_conf.tiltback_variable = 0.0f;
    d->float_conf.tiltback_variable_max = 1.0f;
    refloat_main_reconfigure(d);

    EXPECT_TRUE(isfinite(d->tiltback_variable_max_erpm));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_startup_loop_reaches_ready(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    const float lv_threshold = d->motor.lv_threshold;

    d->beeper_enabled = true;
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 0, lv_threshold + 1.0f, 25, 25);
    vesc_if_fake_set_imu_startup_done(true);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_EQ_U32(d->state.state, STATE_READY);
    EXPECT_TRUE(d->beep_num_left > 3);

    d->state.state = STATE_STARTUP;
    d->beep_num_left = 0;
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 0, lv_threshold + 6.0f, 25, 25);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_EQ_U32(d->state.state, STATE_READY);
    EXPECT_EQ_U32(d->beep_num_left, 3u);

    d->state.state = STATE_DISABLED;
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);
    EXPECT_EQ_U32(d->state.state, STATE_DISABLED);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_running_loop_keeps_command_finite(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.state = STATE_RUNNING;
    d->state.mode = MODE_NORMAL;
    d->state.sat = SAT_NONE;
    d->state.stop_condition = STOP_NONE;
    d->footpad.state = FS_BOTH;
    d->imu.balance_pitch = 0.0f;
    d->imu.pitch = 0.0f;
    d->imu.roll = 0.0f;
    d->motor.abs_erpm = 0.0f;
    d->motor.erpm = 0.0f;
    d->motor.distance = 0.0f;

    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_TRUE(d->enable_upside_down);
    EXPECT_TRUE(isfinite(d->setpoint));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_special_modes_ignore_input_tilt(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    const Mode modes[] = {MODE_HANDTEST, MODE_FLYWHEEL};

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        d->state.state = STATE_RUNNING;
        d->state.mode = modes[i];
        d->state.sat = SAT_NONE;
        d->state.stop_condition = STOP_NONE;
        d->imu.balance_pitch = 0.0f;
        d->imu.pitch = 0.0f;
        d->imu.roll = 0.0f;
        d->motor.erpm = 0.0f;
        d->motor.abs_erpm = 0.0f;
        d->setpoint = 0.0f;
        d->setpoint_target = 0.0f;
        d->setpoint_target_interpolated = 0.0f;
        d->remote.input = 1.0f;
        smooth_setpoint_reset(&d->remote.setpoint);
        d->float_conf.inputtilt_angle_limit = 10.0f;
        d->remote.command_input_time = 10u * SYSTEM_TICK_RATE_HZ;
        vesc_if_fake_set_ticks(10u * SYSTEM_TICK_RATE_HZ);
        vesc_if_fake_set_analog(10.0f, 10.0f);
        vesc_if_fake_set_terminate_after_sleep(true);
        refloat_main_run_loop(d);
        EXPECT_FLOAT_NEAR(d->remote.setpoint.value, 0.0f);
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_ready_loop_engages_with_both_sensors(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_BOTH;
    d->imu.balance_pitch = 0.0f;
    d->imu.roll = 0.0f;
    d->startup_pitch_tolerance = 10.0f;
    d->float_conf.startup_roll_tolerance = 10.0f;
    d->float_conf.fault_adc1 = 0.0f;
    d->float_conf.fault_adc2 = 0.0f;

    vesc_if_fake_set_analog(10.0f, 10.0f);
    footpad_sensor_update(&d->footpad, &d->float_conf);
    EXPECT_EQ_U32(d->footpad.state, FS_BOTH);
    EXPECT_TRUE(refloat_main_can_engage(d));
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_EQ_U32(d->state.state, STATE_RUNNING);
    EXPECT_EQ_U32(d->state.stop_condition, STOP_NONE);

    const struct {
        bool darkride;
        bool disengaged_over_one_second;
        StopCondition stop_condition;
        float pitch;
        float roll;
        float erpm;
        bool pushstart;
        bool reverse_stop;
        RunState expected;
    } cases[] = {
        {true, false, STOP_NONE, 0, 130, 0, false, false, STATE_RUNNING},
        {true, false, STOP_REVERSE_STOP, 0, 130, 0, false, false, STATE_READY},
        {true, true, STOP_NONE, 0, 180, 0, false, false, STATE_RUNNING},
        {false, false, STOP_NONE, 20, 0, 1500, true, true, STATE_RUNNING},
        {false, false, STOP_NONE, 20, 0, -1500, true, true, STATE_READY},
    };
    const systime_t now = 100u * SYSTEM_TICK_RATE_HZ;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        d->state.state = STATE_READY;
        d->state.mode = MODE_NORMAL;
        d->state.darkride = cases[i].darkride;
        d->state.charging = false;
        d->state.stop_condition = cases[i].stop_condition;
        d->enable_upside_down = false;
        d->float_conf.fault_darkride_enabled = true;
        d->float_conf.startup_pushstart_enabled = cases[i].pushstart;
        d->float_conf.fault_reversestop_enabled = cases[i].reverse_stop;
        d->startup_pitch_tolerance = 10.0f;
        d->float_conf.startup_roll_tolerance = 10.0f;
        d->imu.balance_pitch = cases[i].pitch;
        d->imu.roll = cases[i].roll;
        d->time.disengage_timer =
            now - (cases[i].disengaged_over_one_second ? 2u * SYSTEM_TICK_RATE_HZ : 0u);

        vesc_if_fake_set_ticks(now);
        vesc_if_fake_set_motor_telemetry(cases[i].erpm, 0, 0, 0, 0, 0, 0, 50, 25, 25);
        vesc_if_fake_set_analog(10.0f, 10.0f);
        vesc_if_fake_set_terminate_after_sleep(true);
        refloat_main_run_loop(d);

        EXPECT_EQ_U32(d->state.state, cases[i].expected);
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_ready_loop_toggles_headlights(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    d->float_conf.hardware.leds.mode = LED_MODE_INTERNAL;
    d->float_conf.startup_simplestart_enabled = false;
    d->float_conf.startup_pushstart_enabled = false;
    d->float_conf.fault_adc1 = 1.0f;
    d->float_conf.fault_adc2 = 1.0f;
    leds_set_headlights_enabled(&d->leds, false);

    const FootpadSensorState sequence[] = {
        FS_LEFT,
        FS_NONE,
        FS_LEFT,
        FS_NONE,
        FS_RIGHT,
        FS_RIGHT,
        FS_NONE,
        FS_RIGHT,
        FS_NONE,
        FS_LEFT,
    };
    for (size_t i = 0; i < sizeof(sequence) / sizeof(sequence[0]); ++i) {
        vesc_if_fake_set_analog(
            sequence[i] == FS_LEFT ? 2.0f : 0.0f, sequence[i] == FS_RIGHT ? 2.0f : 0.0f
        );
        vesc_if_fake_set_ticks((i + 1) * SYSTEM_TICK_RATE_HZ / 5);
        vesc_if_fake_set_terminate_after_sleep(true);
        refloat_main_run_loop(d);

        if (i == 4) {
            EXPECT_TRUE(leds_get_runtime_status(&d->leds)->headlights_enabled);
        }
    }

    EXPECT_TRUE(!leds_get_runtime_status(&d->leds)->headlights_enabled);
    EXPECT_EQ_U32(d->state.state, STATE_READY);
    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_ready_loop_alert_timers(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    const systime_t alert_now = 100u * SYSTEM_TICK_RATE_HZ;

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    d->beeper_enabled = true;
    d->imu.balance_pitch = 90.0f;
    d->float_conf.bms.enabled = true;
    d->float_conf.bms.cell_lv_threshold = 3.0f;
    d->float_conf.bms.cell_hv_threshold = 4.3f;
    d->float_conf.bms.cell_balance_threshold = 0.1f;
    d->time.start_timer = 0;
    d->time.disengage_timer = 0;
    d->alert_timer = 0;
    d->bms.msg_age = 6.0f;
    vesc_if_fake_set_analog(0.0f, 0.0f);
    vesc_if_fake_set_ticks(alert_now);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_TRUE(bms_is_fault(&d->bms, BMSF_CONNECTION));
    EXPECT_EQ_U32(d->beep_num_left, 9u);
    uint8_t connection_reason = d->beep_reason;

    d->state.state = STATE_READY;
    d->beep_num_left = 0;
    d->beep_reason = 0;
    d->alert_timer = 0;
    d->bms.msg_age = 0.0f;
    d->bms.cell_lv = 3.5f;
    d->bms.cell_hv = 3.8f;
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_TRUE(bms_is_fault(&d->bms, BMSF_CELL_BALANCE));
    EXPECT_EQ_U32(d->beep_num_left, 9u);
    EXPECT_TRUE(d->beep_reason != connection_reason);

    d->float_conf.bms.enabled = false;
    d->state.state = STATE_READY;
    d->state.darkride = true;
    d->enable_upside_down = true;
    d->imu.roll = 180.0f;
    d->beep_num_left = 0;
    d->time.disengage_timer = 0;
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_TRUE(!d->state.darkride);
    EXPECT_TRUE(!d->enable_upside_down);
    EXPECT_EQ_U32(d->beep_num_left, 3u);

    d->state.state = STATE_READY;
    d->imu.roll = 0.0f;
    d->beep_num_left = 0;
    d->beep_reason = 0;
    d->time.idle_timer = 0;
    d->nag_timer = 0;
    d->idle_voltage = 40.0f;
    vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0, 0, 50, 25, 25);
    vesc_if_fake_set_ticks(2000u * SYSTEM_TICK_RATE_HZ);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_FLOAT_NEAR(d->idle_voltage, 50.0f);
    EXPECT_EQ_U32(d->beep_num_left, 0u);

    d->state.state = STATE_READY;
    vesc_if_fake_set_ticks(2061u * SYSTEM_TICK_RATE_HZ);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);

    EXPECT_EQ_U32(d->beep_num_left, 5u);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_running_loop_stops_on_open_switch(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.state = STATE_RUNNING;
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_NONE;
    d->float_conf.fault_delay_switch_full = 0.0f;
    vesc_if_fake_set_ticks(1000);
    vesc_if_fake_set_terminate_after_sleep(true);

    refloat_main_run_loop(d);

    EXPECT_EQ_U32(d->state.state, STATE_READY);
    EXPECT_EQ_U32(d->state.stop_condition, STOP_SWITCH_FULL);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_loop_processes_valid_state_inputs(void) {
    const struct {
        RunState state;
        Mode mode;
    } states[] = {
        {STATE_STARTUP, MODE_NORMAL},
        {STATE_READY, MODE_NORMAL},
        {STATE_READY, MODE_FLYWHEEL},
        {STATE_RUNNING, MODE_NORMAL},
        {STATE_RUNNING, MODE_HANDTEST},
        {STATE_RUNNING, MODE_FLYWHEEL},
        {STATE_DISABLED, MODE_NORMAL},
    };
    const FootpadSensorState footpads[] = {FS_NONE, FS_LEFT, FS_RIGHT, FS_BOTH};
    const float rolls[] = {0.0f, 110.0f, 160.0f, 180.0f};
    const float pitches[] = {0.0f, 20.0f, 80.0f};
    const float erpms[] = {-2500.0f, 0.0f, 2500.0f};

    for (size_t state_i = 0; state_i < sizeof(states) / sizeof(states[0]); ++state_i) {
        for (size_t footpad_i = 0; footpad_i < 4; ++footpad_i) {
            for (size_t roll_i = 0; roll_i < 4; ++roll_i) {
                for (size_t pitch_i = 0; pitch_i < 3; ++pitch_i) {
                    for (size_t erpm_i = 0; erpm_i < 3; ++erpm_i) {
                        for (uint8_t variant = 0; variant < 2; ++variant) {
                            MainProtocolFixture fixture = {0};
                            EXPECT_TRUE(main_protocol_fixture_start(&fixture));
                            Data *d = fixture.data;
                            d->state.state = states[state_i].state;
                            d->state.mode = states[state_i].mode;
                            d->state.darkride = variant;
                            d->state.stop_condition = STOP_NONE;
                            d->state.sat = SAT_NONE;
                            d->float_conf.fault_darkride_enabled = true;
                            d->float_conf.startup_pushstart_enabled = true;
                            d->float_conf.startup_simplestart_enabled = true;
                            d->float_conf.fault_adc1 = 1.0f;
                            d->float_conf.fault_adc2 = 1.0f;
                            d->startup_pitch_tolerance = 30.0f;
                            d->imu.balance_pitch = pitches[pitch_i];
                            d->imu.pitch = pitches[pitch_i];
                            d->imu.roll = rolls[roll_i];
                            d->enable_upside_down = true;

                            vesc_if_fake_set_ticks(20u * SYSTEM_TICK_RATE_HZ);
                            vesc_if_fake_set_analog(
                                footpads[footpad_i] & FS_LEFT ? 2.0f : 0.0f,
                                footpads[footpad_i] & FS_RIGHT ? 2.0f : 0.0f
                            );
                            vesc_if_fake_set_motor_telemetry(
                                erpms[erpm_i],
                                0.0f,
                                erpms[erpm_i] / 1000.0f,
                                10.0f,
                                10.0f,
                                variant ? 0.95f : 0.1f,
                                2.0f,
                                50.0f,
                                25.0f,
                                25.0f
                            );
                            vesc_if_fake_set_imu_startup_done(true);
                            vesc_if_fake_set_terminate_after_sleep(true);
                            refloat_main_run_loop(d);

                            EXPECT_TRUE(
                                d->state.state == STATE_DISABLED ||
                                d->state.state == STATE_STARTUP || d->state.state == STATE_READY ||
                                d->state.state == STATE_RUNNING
                            );
                            EXPECT_TRUE(isfinite(d->setpoint));
                            EXPECT_TRUE(isfinite(d->setpoint_target));
                            main_protocol_fixture_stop(&fixture);
                        }
                    }
                }
            }
        }
    }

    return true;
}

static bool test_main_loop_handles_targeted_state_transitions(void) {
    enum {
        LOOP_STARTUP_WAIT,
        LOOP_FW_FAULT,
        LOOP_CENTERING,
        LOOP_WHEELSLIP,
        LOOP_FLYWHEEL_ABORT,
        LOOP_PITCH_BOUNDARY,
        LOOP_DARKRIDE_PITCH,
        LOOP_DARKRIDE_ROLL,
        LOOP_TILT_OPPOSITE,
        LOOP_TILT_NEGATIVE,
        LOOP_FLYWHEEL_KONAMI,
        LOOP_BMS_BALANCE_GRACE,
        LOOP_BMS_BALANCE_OLD,
        LOOP_BMS_ALERT_FRESH,
        LOOP_CASE_COUNT,
    };

    for (unsigned int scenario = 0; scenario < LOOP_CASE_COUNT; ++scenario) {
        MainProtocolFixture fixture = {0};
        EXPECT_TRUE(main_protocol_fixture_start(&fixture));
        Data *d = fixture.data;
        d->state.state = STATE_READY;
        d->state.mode = MODE_NORMAL;
        vesc_if_fake_set_analog(0.0f, 0.0f);
        vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0.1f, 0, 50, 25, 25);

        switch (scenario) {
        case LOOP_STARTUP_WAIT:
            d->state.state = STATE_STARTUP;
            vesc_if_fake_set_imu_startup_done(false);
            break;
        case LOOP_FW_FAULT:
            d->state.state = STATE_DISABLED;
            vesc_if_fake_set_fault(FAULT_CODE_OVER_VOLTAGE);
            break;
        case LOOP_CENTERING:
            d->state.state = STATE_RUNNING;
            d->state.sat = SAT_CENTERING;
            vesc_if_fake_set_analog(2.0f, 2.0f);
            break;
        case LOOP_WHEELSLIP:
            d->state.state = STATE_RUNNING;
            d->state.wheelslip = true;
            vesc_if_fake_set_analog(2.0f, 2.0f);
            vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0.95f, 0, 50, 25, 25);
            break;
        case LOOP_FLYWHEEL_ABORT:
            d->state.mode = MODE_FLYWHEEL;
            d->flywheel_allow_abort = true;
            vesc_if_fake_set_analog(2.0f, 2.0f);
            break;
        case LOOP_PITCH_BOUNDARY:
            d->imu.pitch = 105.0f;
            break;
        case LOOP_DARKRIDE_PITCH:
            d->state.darkride = true;
            d->imu.roll = 140.0f;
            d->imu.balance_pitch = 100.0f;
            d->startup_pitch_tolerance = 30.0f;
            d->fault_angle_pitch_timer = 2u * SYSTEM_TICK_RATE_HZ;
            d->time.disengage_timer = 2u * SYSTEM_TICK_RATE_HZ;
            vesc_if_fake_set_ticks(2u * SYSTEM_TICK_RATE_HZ);
            break;
        case LOOP_DARKRIDE_ROLL:
            d->state.darkride = true;
            d->imu.roll = 120.0f;
            d->imu.balance_pitch = 0.0f;
            d->startup_pitch_tolerance = 30.0f;
            d->fault_angle_pitch_timer = 2u * SYSTEM_TICK_RATE_HZ;
            d->time.disengage_timer = 0u;
            vesc_if_fake_set_ticks(2u * SYSTEM_TICK_RATE_HZ);
            break;
        case LOOP_TILT_OPPOSITE:
            d->state.state = STATE_RUNNING;
            d->atr.setpoint.value = -2.0f;
            d->torque_tilt.setpoint.value = 2.0f;
            vesc_if_fake_set_analog(2.0f, 2.0f);
            break;
        case LOOP_TILT_NEGATIVE:
            d->state.state = STATE_RUNNING;
            d->atr.setpoint.value = -2.0f;
            d->torque_tilt.setpoint.value = -2.0f;
            vesc_if_fake_set_analog(2.0f, 2.0f);
            break;
        case LOOP_FLYWHEEL_KONAMI:
            d->imu.pitch = 80.0f;
            d->flywheel_konami.state = d->flywheel_konami.sequence_size - 1u;
            d->flywheel_konami.timer = 4u * SYSTEM_TICK_RATE_HZ / 5u;
            vesc_if_fake_set_ticks(SYSTEM_TICK_RATE_HZ);
            break;
        case LOOP_BMS_BALANCE_GRACE:
            d->float_conf.bms.enabled = true;
            d->float_conf.bms.cell_balance_threshold = 0.1f;
            d->bms.cell_lv = 3.5f;
            d->bms.cell_hv = 4.0f;
            d->bms.msg_age = 0.0f;
            d->time.disengage_timer = SYSTEM_TICK_RATE_HZ;
            vesc_if_fake_set_ticks(SYSTEM_TICK_RATE_HZ);
            break;
        case LOOP_BMS_BALANCE_OLD:
            d->float_conf.bms.enabled = true;
            d->float_conf.bms.cell_balance_threshold = 0.1f;
            d->bms.cell_lv = 3.5f;
            d->bms.cell_hv = 4.0f;
            d->bms.msg_age = 0.0f;
            d->time.disengage_timer = 0u;
            vesc_if_fake_set_ticks(6u * SYSTEM_TICK_RATE_HZ);
            break;
        case LOOP_BMS_ALERT_FRESH:
            d->float_conf.bms.enabled = true;
            d->bms.msg_age = 10.0f;
            d->alert_timer = 6u * SYSTEM_TICK_RATE_HZ;
            vesc_if_fake_set_ticks(6u * SYSTEM_TICK_RATE_HZ);
            break;
        }

        vesc_if_fake_set_terminate_after_sleep(true);
        refloat_main_run_loop(d);
        EXPECT_TRUE(isfinite(d->setpoint));
        if (scenario == LOOP_DARKRIDE_PITCH || scenario == LOOP_DARKRIDE_ROLL) {
            EXPECT_TRUE(d->state.darkride);
            EXPECT_EQ_U32(d->state.state, STATE_READY);
        }
        main_protocol_fixture_stop(&fixture);
    }

    return true;
}

static bool test_main_aux_loop_runs_one_iteration(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.state = STATE_READY;
    d->time.now = SYSTEM_TICK_RATE_HZ;
    d->odometer = 0;
    vesc_if_fake_set_odometer(300);
    fake_vesc_if.thread_set_priority = main_test_thread_set_priority;
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_aux_loop(d);

    EXPECT_TRUE(vesc_if_fake_sleep_us_calls() > 0);
    EXPECT_EQ_U32(d->state.state, STATE_READY);
    EXPECT_EQ_U32(d->odometer, 300u);

    main_protocol_fixture_stop(&fixture);

    fixture = (MainProtocolFixture){0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    d = fixture.data;
    d->state.state = STATE_RUNNING;
    fake_vesc_if.thread_set_priority = NULL;
    vesc_if_fake_set_odometer(300);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_aux_loop(d);
    EXPECT_EQ_U32(d->odometer, 0u);
    main_protocol_fixture_stop(&fixture);

    fixture = (MainProtocolFixture){0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    d = fixture.data;
    d->state.state = STATE_READY;
    fake_vesc_if.thread_set_priority = NULL;
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_aux_loop(d);
    EXPECT_EQ_U32(d->odometer, 0u);
    main_protocol_fixture_stop(&fixture);
    return true;
}

static float main_hostile_pushback_target(Data *d, size_t index) {
    main_setpoint_target_prepare(d);
    if (index == 0) {
        d->motor.duty_cycle.value = 0.9f;
        d->float_conf.tiltback_duty_angle = -5.0f;
    } else if (index == 1) {
        d->motor.duty_cycle.value = 0.1f;
        d->motor.batt_voltage = 62.0f;
        d->float_conf.tiltback_hv_angle = -6.0f;
    } else {
        d->motor.duty_cycle.value = 0.1f;
        d->motor.batt_voltage = 37.0f;
        d->motor.dir_current = 30.0f;
        d->float_conf.tiltback_lv_angle = -4.0f;
    }
    refloat_main_calculate_setpoint_target(d);
    return d->setpoint_target;
}

static bool main_hostile_fault_result(Data *d, size_t index) {
    main_setpoint_target_prepare(d);
    d->state.mode = MODE_NORMAL;
    d->state.darkride = false;
    d->state.stop_condition = STOP_NONE;
    d->footpad.state = FS_BOTH;
    d->imu.roll = 0.0f;
    d->imu.pitch = 0.0f;
    d->remote.setpoint.value = 0.0f;
    d->float_conf.fault_roll = 45.0f;
    d->float_conf.fault_pitch = 45.0f;
    d->float_conf.fault_delay_switch_full = 0;
    d->float_conf.fault_delay_switch_half = 0;
    d->float_conf.fault_delay_roll = 0;
    d->float_conf.fault_delay_pitch = 0;
    d->time.now = 20 * SYSTEM_TICK_RATE_HZ;

    switch (index) {
    case 0:
        d->float_conf.fault_pitch = 0;
        d->imu.pitch = 10.0f;
        timer_expire(&d->time, &d->fault_angle_pitch_timer, 1.0f);
        break;
    case 1:
        d->float_conf.fault_roll = 0;
        d->imu.roll = 10.0f;
        timer_expire(&d->time, &d->fault_angle_roll_timer, 1.0f);
        break;
    case 2:
        d->imu.pitch = 50.0f;
        d->float_conf.fault_delay_pitch = UINT16_MAX;
        timer_expire(&d->time, &d->fault_angle_pitch_timer, 11.0f);
        break;
    case 3:
        d->imu.roll = 50.0f;
        d->float_conf.fault_delay_roll = UINT16_MAX;
        timer_expire(&d->time, &d->fault_angle_roll_timer, 11.0f);
        break;
    case 4:
        d->footpad.state = FS_NONE;
        d->motor.abs_erpm = 600.0f;
        d->float_conf.fault_adc_half_erpm = 100.0f;
        d->float_conf.fault_delay_switch_full = UINT16_MAX;
        timer_expire(&d->time, &d->fault_switch_timer, 11.0f);
        break;
    default:
        d->footpad.state = FS_LEFT;
        d->motor.abs_erpm = 0.0f;
        d->float_conf.fault_is_dual_switch = false;
        d->float_conf.fault_adc_half_erpm = 100.0f;
        d->float_conf.fault_delay_switch_half = UINT16_MAX;
        timer_expire(&d->time, &d->fault_switch_half_timer, 11.0f);
        break;
    }
    return refloat_main_check_faults(d);
}

static bool test_main_rejects_hostile_safety_config(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_FLOAT_NEAR(main_hostile_pushback_target(d, i), 0.0f);
    }

    main_setpoint_target_prepare(d);
    d->float_conf.tiltback_variable = -1.0f;
    d->float_conf.tiltback_variable_max = 10.0f;
    refloat_main_reconfigure(d);
    EXPECT_FLOAT_NEAR(d->tiltback_variable, 0.0f);

    const bool expected[] = {false, false, true, true, true, true};
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        EXPECT_TRUE(main_hostile_fault_result(d, i) == expected[i]);
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_bounds_hostile_safety_limits(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.9f;
    d->float_conf.tiltback_duty_angle = 100.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_DUTY);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 30.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 62.0f;
    d->float_conf.tiltback_hv_angle = 100.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 30.0f);

    main_setpoint_target_prepare(d);
    d->motor.duty_cycle.value = 0.1f;
    d->motor.batt_voltage = 37.0f;
    d->motor.dir_current = 30.0f;
    d->float_conf.tiltback_lv_angle = 100.0f;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_FLOAT_NEAR(d->setpoint_target, 30.0f);

    main_setpoint_target_prepare(d);
    d->motor.speed = 101.0f;
    d->float_conf.tiltback_speed = UINT8_MAX;
    refloat_main_calculate_setpoint_target(d);
    EXPECT_EQ_U32(d->state.sat, SAT_PB_SPEED);

    d->float_conf.tiltback_constant = 20.0f;
    d->float_conf.tiltback_constant_erpm = 0;
    d->float_conf.tiltback_variable = 0.0f;
    d->float_conf.noseangling_speed = 100.0f;
    d->motor.abs_erpm = 100.0f;
    d->motor.erpm_sign = 1;
    refloat_main_apply_noseangling(d, 1.0f);
    EXPECT_FLOAT_NEAR(d->noseangling_interpolated, 0.0f);

    d->float_conf.startup_pitch_tolerance = 100.0f;
    refloat_main_reset_runtime_vars(d);
    EXPECT_FLOAT_NEAR(d->startup_pitch_tolerance, 80.0f);

    d->float_conf.tiltback_variable = 10.0f;
    d->float_conf.tiltback_variable_max = 20.0f;
    refloat_main_reconfigure(d);
    EXPECT_FLOAT_NEAR(d->tiltback_variable, 0.005f);
    EXPECT_FLOAT_NEAR(d->tiltback_variable_max_erpm, 2000.0f);

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_BOTH;
    d->imu.balance_pitch = 0.0f;
    d->imu.roll = 90.0f;
    d->startup_pitch_tolerance = 10.0f;
    d->float_conf.startup_roll_tolerance = 100.0f;
    d->float_conf.fault_adc1 = 0.0f;
    d->float_conf.fault_adc2 = 0.0f;
    vesc_if_fake_set_analog(10.0f, 10.0f);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);
    EXPECT_EQ_U32(d->state.state, STATE_READY);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool main_high_fault_result(Data *d, bool pitch) {
    main_setpoint_target_prepare(d);
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_BOTH;
    d->remote.setpoint.value = 0.0f;
    d->float_conf.fault_delay_pitch = 0;
    d->float_conf.fault_delay_roll = 0;
    d->time.now = 20 * SYSTEM_TICK_RATE_HZ;

    if (pitch) {
        d->float_conf.fault_pitch = 100.0f;
        d->imu.pitch = 91.0f;
        timer_expire(&d->time, &d->fault_angle_pitch_timer, 1.0f);
    } else {
        d->float_conf.fault_roll = 100.0f;
        d->imu.roll = 91.0f;
        timer_expire(&d->time, &d->fault_angle_roll_timer, 1.0f);
    }

    return refloat_main_check_faults(d);
}

static bool test_main_bounds_fault_and_setpoint_rate_config(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    EXPECT_TRUE(main_high_fault_result(d, true));
    EXPECT_TRUE(main_high_fault_result(d, false));

    const struct {
        SetpointAdjustmentType sat;
        float *setting;
        float maximum;
    } rates[] = {
        {SAT_NONE, &d->float_conf.tiltback_return_speed, 10.0f},
        {SAT_CENTERING, &d->float_conf.startup_speed, 100.0f},
        {SAT_REVERSESTOP, NULL, 100.0f},
        {SAT_PB_SPEED, &d->float_conf.tiltback_duty_speed, 30.0f},
        {SAT_PB_DUTY, &d->float_conf.tiltback_duty_speed, 30.0f},
        {SAT_PB_HIGH_VOLTAGE, &d->float_conf.tiltback_hv_speed, 30.0f},
        {SAT_PB_TEMPERATURE, &d->float_conf.tiltback_hv_speed, 30.0f},
        {SAT_PB_ERROR, &d->float_conf.tiltback_hv_speed, 30.0f},
        {SAT_PB_LOW_VOLTAGE, &d->float_conf.tiltback_lv_speed, 30.0f},
        {(SetpointAdjustmentType) UINT8_MAX, NULL, 0.0f},
    };
    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        if (rates[i].setting) {
            *rates[i].setting = 327.0f;
        }
        d->state.sat = rates[i].sat;
        EXPECT_FLOAT_NEAR(refloat_main_get_setpoint_adjustment_speed(d), rates[i].maximum);
    }

    d->float_conf.tiltback_variable_erpm = 0;
    d->tiltback_variable = 1.0f;
    d->tiltback_variable_max_erpm = 30.0f;
    d->float_conf.noseangling_speed = 327.0f;
    d->motor.abs_erpm = 30.0f;
    d->motor.erpm_sign = 1;
    d->noseangling_interpolated = 0.0f;
    refloat_main_apply_noseangling(d, 0.1f);
    EXPECT_FLOAT_NEAR(d->noseangling_interpolated, 10.0f);

    d->float_conf.tiltback_variable = 0.0f;
    d->float_conf.tiltback_constant = 10.0f;
    d->float_conf.tiltback_constant_erpm = 200;
    d->motor.abs_erpm = 201.0f;
    d->noseangling_interpolated = 0.0f;
    refloat_main_apply_noseangling(d, 0.1f);
    EXPECT_FLOAT_NEAR(d->noseangling_interpolated, 10.0f);

    main_protocol_fixture_stop(&fixture);
    return true;
}
