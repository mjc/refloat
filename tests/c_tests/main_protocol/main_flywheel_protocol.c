enum {
    MAIN_COMMAND_FLYWHEEL = 22
};

static void main_send_flywheel(const uint8_t *payload, size_t len) {
    uint8_t packet[9] = {101, MAIN_COMMAND_FLYWHEEL};
    memcpy(packet + 2, payload, len);
    vesc_if_fake_invoke_app_data_handler(packet, len + 2);
}

static bool test_main_flywheel_command_guards_and_stop(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    uint8_t start[] = {1, 0, 0, 0, 0, 0};

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    main_send_flywheel(start, sizeof(start));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    start[0] |= 0x80;
    d->state.mode = MODE_HANDTEST;
    main_send_flywheel(start, sizeof(start));
    EXPECT_EQ_U32(d->state.mode, MODE_HANDTEST);

    d->state.mode = MODE_NORMAL;
    d->state.state = STATE_RUNNING;
    main_send_flywheel(start, sizeof(start));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    d->state.state = STATE_READY;
    d->state.charging = true;
    d->imu.pitch = 80.0f;
    main_send_flywheel(start, sizeof(start));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);
    d->state.charging = false;

    d->state.state = STATE_RUNNING;
    d->state.mode = MODE_FLYWHEEL;
    uint8_t stop[] = {0x80, 0, 0, 0, 0, 0};
    main_send_flywheel(stop, sizeof(stop));
    EXPECT_EQ_U32(d->state.mode, MODE_FLYWHEEL);
    EXPECT_EQ_U32(d->state.state, STATE_RUNNING);

    d->state.state = STATE_READY;
    main_send_flywheel(stop, sizeof(stop));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);
    EXPECT_EQ_U32(d->state.state, STATE_READY);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_flywheel_command_configuration(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;

    uint8_t command[] = {0x81, 0, 0, 0, 0, 0, 0};
    d->imu.pitch = 69.0f;
    main_send_flywheel(command, sizeof(command));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    d->imu.pitch = 80.0f;
    d->imu.roll = 4.0f;
    command[0] = 0x82;
    command[1] = 90;
    command[2] = 50;
    command[3] = 30;
    command[4] = 20;
    command[6] = 20;
    main_send_flywheel(command, sizeof(command));
    EXPECT_EQ_U32(d->state.mode, MODE_FLYWHEEL);
    EXPECT_FLOAT_NEAR(d->imu.flywheel_pitch_offset, 80.0f);
    EXPECT_FLOAT_NEAR(d->imu.flywheel_roll_offset, 4.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.kp, 9.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.kp2, 0.5f);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_angle, 3.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty, 0.2f);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_speed, 10.0f);

    command[0] = 0x85;
    memset(command + 1, 0, sizeof(command) - 1);
    main_send_flywheel(command, sizeof(command) - 1);
    EXPECT_FLOAT_NEAR(d->float_conf.fault_roll, 90.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.kp, 8.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.kp2, 0.3f);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_speed, 5.0f);

    command[6] = 1;
    main_send_flywheel(command, sizeof(command));
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_speed, 5.0f);
    command[6] = 100;
    main_send_flywheel(command, sizeof(command));
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_speed, 5.0f);

    d->imu.pitch = 0.0f;
    command[0] = 0x82;
    main_send_flywheel(command, sizeof(command));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);
    EXPECT_FLOAT_NEAR(d->float_conf.kp, CFG_DFLT_KP);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_flywheel_footpad_abort_option(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->state.state = STATE_READY;
    d->imu.pitch = 80.0f;

    uint8_t command[] = {0x82, 0, 0, 0, 0, 0};
    main_send_flywheel(command, sizeof(command));
    d->state.state = STATE_RUNNING;
    d->footpad.state = FS_LEFT;
    EXPECT_TRUE(!refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.mode, MODE_FLYWHEEL);

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    d->footpad.state = FS_NONE;
    command[5] = 1;
    main_send_flywheel(command, sizeof(command));
    d->state.state = STATE_RUNNING;
    d->footpad.state = FS_LEFT;
    EXPECT_TRUE(refloat_main_check_faults(d));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_ready_flywheel_abort_paths(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->state.state = STATE_READY;
    d->state.mode = MODE_FLYWHEEL;
    d->flywheel_abort = true;
    vesc_if_fake_set_analog(0.0f, 0.0f);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    d->state.state = STATE_READY;
    d->state.mode = MODE_FLYWHEEL;
    d->flywheel_abort = false;
    d->flywheel_allow_abort = true;
    vesc_if_fake_set_analog(10.0f, 10.0f);
    vesc_if_fake_set_terminate_after_sleep(true);
    refloat_main_run_loop(d);
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_flywheel_uses_tight_fault_angles(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->state.state = STATE_READY;
    d->imu.pitch = 80.0f;

    uint8_t command[] = {0x82, 0, 0, 0, 0, 0};
    main_send_flywheel(command, sizeof(command));
    d->state.state = STATE_RUNNING;
    d->footpad.state = FS_NONE;
    d->time.now = 100000u;
    d->float_conf.fault_delay_pitch = 0;
    d->imu.pitch = 7.0f;
    timer_expire(&d->time, &d->fault_angle_pitch_timer, 1.0f);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_PITCH);

    d->state.state = STATE_RUNNING;
    d->state.stop_condition = STOP_NONE;
    d->imu.pitch = 0.0f;
    d->imu.roll = 36.0f;
    d->float_conf.fault_delay_roll = 0;
    timer_expire(&d->time, &d->fault_angle_roll_timer, 1.0f);
    EXPECT_TRUE(refloat_main_check_faults(d));
    EXPECT_EQ_U32(d->state.stop_condition, STOP_ROLL);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_flywheel_reconfigures_cached_controls(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->state.state = STATE_READY;
    d->imu.pitch = 80.0f;
    d->float_conf.brake_current = 9.0f;
    d->float_conf.braketilt_strength = 10.0f;
    refloat_main_reconfigure(d);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 9.0f);
    EXPECT_TRUE(d->brake_tilt.factor < 0.0f);

    uint8_t command[] = {0x82, 0, 0, 0, 0, 0};
    main_send_flywheel(command, sizeof(command));
    EXPECT_FLOAT_NEAR(d->float_conf.brake_current, 0.0f);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 0.0f);
    EXPECT_FLOAT_NEAR(d->brake_tilt.factor, 0.0f);

    main_protocol_fixture_stop(&fixture);
    return true;
}
