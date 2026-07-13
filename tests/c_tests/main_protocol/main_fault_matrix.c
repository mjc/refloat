static void main_fault_prepare(Data *d) {
    d->time.now = 10000;
    d->state.state = STATE_RUNNING;
    d->state.mode = MODE_NORMAL;
    d->state.sat = SAT_NONE;
    d->state.darkride = false;
    d->state.wheelslip = false;
    d->state.stop_condition = STOP_NONE;
    d->footpad.state = FS_BOTH;
    d->motor.erpm = 0;
    d->motor.abs_erpm = 0;
    d->motor.erpm_sign = 1;
    d->imu.pitch = 0;
    d->imu.roll = 0;
    d->remote.setpoint.value = 0;
    d->float_conf.enable_quickstop = false;
    d->float_conf.fault_moving_fault_disabled = false;
    d->float_conf.fault_is_dual_switch = true;
    d->float_conf.fault_darkride_enabled = false;
    timer_refresh(&d->time, &d->fault_switch_timer);
    timer_refresh(&d->time, &d->fault_switch_half_timer);
    timer_refresh(&d->time, &d->fault_angle_roll_timer);
    timer_refresh(&d->time, &d->fault_angle_pitch_timer);
    timer_refresh(&d->time, &d->upside_down_fault_timer);
}

static bool test_main_darkride_fault_matrix(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    main_fault_prepare(d);
    d->state.darkride = true;
    d->footpad.state = FS_NONE;
    d->motor.erpm = 1500;
    timer_expire(&d->time, &d->fault_switch_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_REVERSE_STOP);

    main_fault_prepare(d);
    d->state.darkride = true;
    d->state.wheelslip = true;
    d->footpad.state = FS_NONE;
    d->motor.erpm = 1500;
    timer_expire(&d->time, &d->upside_down_fault_timer, 2);
    timer_expire(&d->time, &d->fault_switch_timer, 0.04f);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_REVERSE_STOP);

    main_fault_prepare(d);
    d->state.darkride = true;
    d->footpad.state = FS_NONE;
    d->motor.erpm = 500;
    timer_expire(&d->time, &d->fault_angle_roll_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_REVERSE_STOP);

    main_fault_prepare(d);
    d->state.darkride = true;
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_SWITCH_HALF);

    main_fault_prepare(d);
    d->state.darkride = true;
    d->footpad.state = FS_NONE;
    d->motor.erpm = 500;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_switch_and_reverse_stop_fault_matrix(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    main_fault_prepare(d);
    d->footpad.state = FS_NONE;
    d->float_conf.fault_moving_fault_disabled = true;
    d->motor.erpm = d->float_conf.fault_adc_half_erpm * 3;
    d->motor.abs_erpm = fabsf(d->motor.erpm);
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->footpad.state = FS_NONE;
    d->float_conf.fault_delay_switch_full = 1000;
    d->float_conf.fault_delay_switch_half = 0;
    d->motor.abs_erpm = 0;
    timer_expire(&d->time, &d->fault_switch_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_SWITCH_FULL);

    main_fault_prepare(d);
    d->state.mode = MODE_FLYWHEEL;
    d->state.sat = SAT_REVERSESTOP;
    d->footpad.state = FS_NONE;
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_SWITCH_FULL);

    main_fault_prepare(d);
    d->state.sat = SAT_REVERSESTOP;
    timer_expire(&d->time, &d->reverse_stop.timer, 4);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_REVERSE_STOP);

    main_fault_prepare(d);
    d->float_conf.fault_is_dual_switch = false;
    d->float_conf.fault_delay_switch_half = 0;
    d->footpad.state = FS_LEFT;
    timer_expire(&d->time, &d->fault_switch_half_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_SWITCH_HALF);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_angle_fault_matrix(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    main_fault_prepare(d);
    d->float_conf.fault_delay_roll = 0;
    d->imu.roll = 95;
    timer_expire(&d->time, &d->fault_angle_roll_timer, 1);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_ROLL);

    main_fault_prepare(d);
    d->float_conf.fault_delay_pitch = 1000;
    d->imu.pitch = 60;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_delay_pitch = 0;
    d->imu.pitch = 60;
    d->remote.setpoint.value = 30;
    timer_expire(&d->time, &d->fault_angle_pitch_timer, 1);
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_darkride_enabled = true;
    d->float_conf.fault_delay_roll = 10000;
    d->imu.roll = 110;
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_ROLL);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_fault_guard_boundaries(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    for (uint8_t stage = 0; stage < 3; ++stage) {
        main_fault_prepare(d);
        d->state.darkride = true;
        d->state.wheelslip = stage > 0;
        d->footpad.state = FS_NONE;
        d->motor.erpm = 1500;
        if (stage == 2) {
            timer_expire(&d->time, &d->upside_down_fault_timer, 2);
        }
        EXPECT_TRUE(!refloat_main_check_faults(d));
    }

    main_fault_prepare(d);
    d->footpad.state = FS_NONE;
    d->float_conf.fault_moving_fault_disabled = true;
    d->float_conf.fault_delay_switch_full = 10000;
    d->float_conf.fault_delay_switch_half = 10000;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_moving_fault_disabled = true;
    d->motor.erpm = d->float_conf.fault_adc_half_erpm * 3;
    d->imu.roll = 40;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->footpad.state = FS_NONE;
    d->float_conf.fault_delay_switch_full = 10000;
    d->float_conf.fault_delay_switch_half = 10000;
    d->motor.abs_erpm = d->float_conf.fault_adc_half_erpm * 7;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    static const struct {
        float abs_erpm;
        float pitch;
        float remote;
        int8_t erpm_sign;
        bool should_fault;
    } quickstop_edges[] = {
        {200, 20, 0, 1, false},
        {100, 14, 0, 1, false},
        {100, 20, 30, 1, false},
        {100, 20, 0, -1, false},
        {100, -20, 0, -1, true},
    };
    for (size_t i = 0; i < sizeof(quickstop_edges) / sizeof(quickstop_edges[0]); ++i) {
        main_fault_prepare(d);
        d->footpad.state = FS_NONE;
        d->float_conf.enable_quickstop = true;
        d->float_conf.fault_delay_switch_full = 10000;
        d->float_conf.fault_delay_switch_half = 10000;
        d->motor.abs_erpm = quickstop_edges[i].abs_erpm;
        d->imu.pitch = quickstop_edges[i].pitch;
        d->remote.setpoint.value = quickstop_edges[i].remote;
        d->motor.erpm_sign = quickstop_edges[i].erpm_sign;
        EXPECT_TRUE(refloat_main_check_faults(d) == quickstop_edges[i].should_fault);
    }

    main_fault_prepare(d);
    d->state.sat = SAT_REVERSESTOP;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_is_dual_switch = false;
    d->footpad.state = FS_LEFT;
    d->motor.abs_erpm = d->float_conf.fault_adc_half_erpm;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_is_dual_switch = false;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_delay_roll = 10000;
    d->imu.roll = 95;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_darkride_enabled = true;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->float_conf.fault_darkride_enabled = true;
    d->float_conf.fault_delay_roll = 10000;
    d->imu.roll = 140;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->state.mode = MODE_FLYWHEEL;
    d->flywheel_allow_abort = true;
    d->footpad.state = FS_NONE;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_fault_prepare(d);
    d->footpad.state = FS_BOTH;
    d->imu.pitch = 80;
    d->remote.setpoint.value = 30;
    EXPECT_TRUE(!refloat_main_check_faults(d));

    main_protocol_fixture_stop(&fixture);
    return true;
}
