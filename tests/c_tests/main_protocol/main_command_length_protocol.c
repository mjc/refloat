enum {
    MAIN_COMMAND_TUNE_DEFAULTS = 3,
    MAIN_COMMAND_CFG_SAVE = 4,
    MAIN_COMMAND_CFG_RESTORE = 5,
    MAIN_COMMAND_BOOSTER = 8,
    MAIN_COMMAND_LOCK = 12,
    MAIN_COMMAND_HANDTEST = 13,
    MAIN_COMMAND_RT_TUNE = 2,
    MAIN_COMMAND_TUNE_OTHER = 6,
    MAIN_COMMAND_TUNE_TILT = 14,
    MAIN_COMMAND_REMOTE = 15,
};

typedef struct {
    const uint8_t *prefix;
    size_t prefix_len;
    unsigned int len;
} MainCommandGuardedPacket;

typedef struct {
    const char *label;
    uint8_t value;
    bool also_set_next_byte;
} MainSerializedConfigMutation;

static bool main_serialized_config_keeps_control_output_finite(
    const Data *data, int byte_offset, const MainSerializedConfigMutation *mutation
) {
    bool finite = isfinite(data->setpoint) && isfinite(data->balance_current.value);
    if (!finite) {
        fprintf(
            stderr,
            "serialized config byte=%d pattern=%s value=%u setpoint=%g balance_current=%g\n",
            byte_offset,
            mutation->label,
            mutation->value,
            (double) data->setpoint,
            (double) data->balance_current.value
        );
    }
    return finite;
}

static bool run_main_command_guarded_packet(void *ctx) {
    MainCommandGuardedPacket *guard = ctx;
    long page_size = sysconf(_SC_PAGESIZE);
    EXPECT_TRUE(page_size > 0);

    size_t mapping_size = (size_t) page_size * 2u;
    uint8_t *mapping =
        mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    EXPECT_TRUE(mapping != MAP_FAILED);

    uint8_t *guard_page = mapping + page_size;
    if (mprotect(guard_page, (size_t) page_size, PROT_NONE) != 0) {
        munmap(mapping, mapping_size);
        EXPECT_TRUE(false);
    }

    EXPECT_TRUE(guard->prefix_len <= (size_t) page_size);
    uint8_t *packet = guard_page - guard->prefix_len;
    for (size_t i = 0; i < guard->prefix_len; ++i) {
        packet[i] = guard->prefix[i];
    }

    vesc_if_fake_invoke_app_data_handler(packet, guard->len);
    munmap(mapping, mapping_size);
    return true;
}

static bool main_protocol_invoke_guarded_packet(
    const uint8_t *prefix, size_t prefix_len, unsigned int len
) {
    MainCommandGuardedPacket guard = {.prefix = prefix, .prefix_len = prefix_len, .len = len};
    return test_expect_no_signal(SIGSEGV, run_main_command_guarded_packet, &guard);
}

static bool test_main_rejects_truncated_command_headers(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));

    EXPECT_TRUE(main_protocol_invoke_guarded_packet(NULL, 0u, 0u));
    const uint8_t package_id[] = {101};
    EXPECT_TRUE(main_protocol_invoke_guarded_packet(package_id, sizeof(package_id), 1u));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_rejects_short_lock_and_handtest_payloads(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    uint8_t lock_without_payload[] = {101, MAIN_COMMAND_LOCK};
    bool lock_ok =
        main_protocol_invoke_guarded_packet(lock_without_payload, sizeof(lock_without_payload), 2u);

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    uint8_t handtest_without_payload[] = {101, MAIN_COMMAND_HANDTEST};
    bool handtest_ok = main_protocol_invoke_guarded_packet(
        handtest_without_payload, sizeof(handtest_without_payload), 2u
    );

    d->state.charging = true;
    uint8_t handtest_while_charging[] = {101, MAIN_COMMAND_HANDTEST, 1};
    vesc_if_fake_invoke_app_data_handler(handtest_while_charging, sizeof(handtest_while_charging));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    main_protocol_fixture_stop(&fixture);

    EXPECT_TRUE(lock_ok);
    EXPECT_TRUE(handtest_ok);
    return true;
}

static bool test_main_commands_respect_packet_lengths(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));

    uint8_t packet[2u + 32u];
    packet[0] = 101u;
    for (unsigned int command = 0; command <= UINT8_MAX; ++command) {
        packet[1] = (uint8_t) command;
        for (size_t payload_len = 0; payload_len <= 32u; ++payload_len) {
            for (uint8_t pattern = 0; pattern < 3; ++pattern) {
                for (size_t i = 0; i < payload_len; ++i) {
                    packet[2u + i] = pattern == 0 ? 0 : pattern == 1 ? UINT8_MAX : (uint8_t) i;
                }
                EXPECT_TRUE(main_protocol_invoke_guarded_packet(
                    packet, 2u + payload_len, (unsigned int) (2u + payload_len)
                ));
            }
        }
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_bluetooth_tuning_ranges(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->brake_tilt.factor = 123.0f;
    uint8_t short_tune[] = {101, MAIN_COMMAND_RT_TUNE};
    vesc_if_fake_invoke_app_data_handler(short_tune, sizeof(short_tune));
    EXPECT_FLOAT_NEAR(d->brake_tilt.factor, 123.0f);

    uint8_t booster[] = {101, MAIN_COMMAND_BOOSTER, 0xff, 0x0f, 0xff, 0x0f};
    vesc_if_fake_invoke_app_data_handler(booster, sizeof(booster));
    EXPECT_FLOAT_NEAR(d->float_conf.booster_angle, 20.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.booster_ramp, 17.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.brkbooster_angle, 20.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.brkbooster_ramp, 17.0f);

    uint8_t other[17] = {101, MAIN_COMMAND_TUNE_OTHER};
    other[2 + 6] = 100;
    other[2 + 9] = 100;
    other[2 + 10] = UINT8_MAX;
    vesc_if_fake_invoke_app_data_handler(other, sizeof(other));
    EXPECT_FLOAT_NEAR(d->tiltback_variable, -0.001f);
    EXPECT_FLOAT_NEAR_EPS(d->tiltback_variable_max_erpm, 15500.0f, 0.01f);

    other[2 + 7] = 10;
    other[2 + 10] = 50;
    other[2 + 12] = 1 | (20 << 2);
    vesc_if_fake_invoke_app_data_handler(other, sizeof(other));
    EXPECT_FLOAT_NEAR(d->float_conf.noseangling_speed, 1.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_variable_max, 5.0f);
    EXPECT_EQ_U32(d->float_conf.inputtilt_remote_type, INPUTTILT_UART);

    uint8_t tune[18] = {101, MAIN_COMMAND_RT_TUNE};
    tune[2 + 10] = UINT8_MAX;
    tune[2 + 12] = UINT8_MAX;
    vesc_if_fake_invoke_app_data_handler(tune, sizeof(tune));
    EXPECT_FLOAT_NEAR(d->float_conf.braketilt_lingering, 15.0f);
    EXPECT_FLOAT_NEAR(d->float_conf.atr_threshold_up, 7.5f);
    EXPECT_FLOAT_NEAR(d->float_conf.atr_threshold_down, 7.5f);

    d->float_conf.haptic.duty_solid_offset = 0.1f;
    refloat_main_reconfigure(d);
    uint8_t tilt[] = {101, MAIN_COMMAND_TUNE_TILT, 0, 0, 50, 20, 10};
    vesc_if_fake_invoke_app_data_handler(tilt, sizeof(tilt));
    EXPECT_FLOAT_NEAR(d->haptic_feedback.duty_solid_threshold, 0.6f);

    d->state.state = STATE_READY;
    d->float_conf.braketilt_strength = 10.0f;
    refloat_main_reconfigure(d);
    EXPECT_TRUE(d->brake_tilt.factor < 0.0f);
    uint8_t handtest[] = {101, MAIN_COMMAND_HANDTEST, 1};
    vesc_if_fake_invoke_app_data_handler(handtest, sizeof(handtest));
    EXPECT_FLOAT_NEAR(d->brake_tilt.factor, 0.0f);

    handtest[2] = 0;
    vesc_if_fake_invoke_app_data_handler(handtest, sizeof(handtest));
    EXPECT_EQ_U32(d->state.mode, MODE_NORMAL);

    d->state.mode = MODE_FLYWHEEL;
    handtest[2] = 1;
    vesc_if_fake_invoke_app_data_handler(handtest, sizeof(handtest));
    EXPECT_EQ_U32(d->state.mode, MODE_FLYWHEEL);
    d->state.mode = MODE_NORMAL;

    uint8_t tune_edges[21] = {101, MAIN_COMMAND_RT_TUNE};
    tune_edges[2 + 18] = 0x01;
    vesc_if_fake_invoke_app_data_handler(tune_edges, sizeof(tune_edges));

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_restore_reconfigures_live_subsystems(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);

    d->float_conf.brake_current = 4.0f;
    d->float_conf.haptic.duty_solid_offset = 0.1f;
    d->float_conf.tiltback_duty = 0.7f;
    refloat_main_write_cfg_to_eeprom(d);

    d->float_conf.brake_current = 9.0f;
    d->float_conf.haptic.duty_solid_offset = 0.2f;
    d->float_conf.tiltback_duty = 0.4f;
    refloat_main_reconfigure(d);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 9.0f);
    EXPECT_FLOAT_NEAR(d->haptic_feedback.duty_solid_threshold, 0.6f);

    uint8_t restore[] = {101, MAIN_COMMAND_CFG_RESTORE};
    vesc_if_fake_invoke_app_data_handler(restore, sizeof(restore));

    EXPECT_FLOAT_NEAR(d->float_conf.brake_current, 4.0f);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 4.0f);
    EXPECT_FLOAT_NEAR(d->haptic_feedback.duty_solid_threshold, 0.8f);

    d->float_conf.brake_current = 9.0f;
    refloat_main_reconfigure(d);
    d->state.state = STATE_RUNNING;
    vesc_if_fake_invoke_app_data_handler(restore, sizeof(restore));
    EXPECT_FLOAT_NEAR(d->float_conf.brake_current, 9.0f);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 9.0f);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_lock_reconfigures_restored_values(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);

    d->float_conf.brake_current = 4.0f;
    refloat_main_write_cfg_to_eeprom(d);
    d->float_conf.brake_current = 9.0f;
    refloat_main_reconfigure(d);

    uint8_t lock[] = {101, MAIN_COMMAND_LOCK, 1};
    vesc_if_fake_invoke_app_data_handler(lock, sizeof(lock));

    EXPECT_TRUE(d->float_conf.disabled);
    EXPECT_FLOAT_NEAR(d->float_conf.brake_current, 4.0f);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 4.0f);

    d->float_conf.brake_current = 9.0f;
    refloat_main_reconfigure(d);
    size_t stores = vesc_if_fake_store_eeprom_var_calls();
    vesc_if_fake_set_eeprom_store_fail_call(stores + 1u);
    lock[2] = 0;
    vesc_if_fake_invoke_app_data_handler(lock, sizeof(lock));
    EXPECT_TRUE(d->float_conf.disabled);
    EXPECT_FLOAT_NEAR(d->float_conf.brake_current, 9.0f);
    EXPECT_FLOAT_NEAR(d->motor_control.brake_current, 9.0f);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_tune_defaults_resets_all_tune_families(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    d->float_conf.torquetilt_strength = 0.9f;
    d->float_conf.torquetilt_strength_regen = 0.8f;
    d->float_conf.torquetilt_start_current = 1.0f;
    d->float_conf.torquetilt_angle_limit = 2.0f;
    d->float_conf.torque_tilt.filter.on_speed_limit = 3.0f;
    d->float_conf.torque_tilt.filter.off_speed_limit = 4.0f;
    d->float_conf.tiltback_duty = 0.2f;
    d->float_conf.tiltback_duty_angle = 1.0f;
    d->float_conf.tiltback_duty_speed = 2.0f;
    d->float_conf.tiltback_speed = 3.0f;
    d->float_conf.tiltback_return_speed = 4.0f;
    d->float_conf.is_dutybeep_enabled = true;
    d->float_conf.tiltback_variable_erpm = 9000.0f;
    d->float_conf.is_beeper_enabled = false;
    d->beeper_enabled = false;

    uint8_t defaults[] = {101, MAIN_COMMAND_TUNE_DEFAULTS};
    vesc_if_fake_invoke_app_data_handler(defaults, sizeof(defaults));

    EXPECT_FLOAT_NEAR(d->float_conf.torquetilt_strength, CFG_DFLT_TORQUETILT_STRENGTH);
    EXPECT_FLOAT_NEAR(d->float_conf.torquetilt_strength_regen, CFG_DFLT_TORQUETILT_STRENGTH_REGEN);
    EXPECT_FLOAT_NEAR(d->float_conf.torquetilt_start_current, CFG_DFLT_TORQUETILT_START_CURRENT);
    EXPECT_FLOAT_NEAR(d->float_conf.torquetilt_angle_limit, CFG_DFLT_TORQUETILT_ANGLE_LIMIT);
    EXPECT_FLOAT_NEAR(
        d->float_conf.torque_tilt.filter.on_speed_limit, CFG_DFLT_TORQUE_TILT_FILTER_ON_SPEED_LIMIT
    );
    EXPECT_FLOAT_NEAR(
        d->float_conf.torque_tilt.filter.off_speed_limit,
        CFG_DFLT_TORQUE_TILT_FILTER_OFF_SPEED_LIMIT
    );
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty, CFG_DFLT_TILTBACK_DUTY);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_angle, CFG_DFLT_TILTBACK_DUTY_ANGLE);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty_speed, CFG_DFLT_TILTBACK_DUTY_SPEED);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_speed, CFG_DFLT_TILTBACK_SPEED);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_return_speed, CFG_DFLT_TILTBACK_RETURN_SPEED);
    EXPECT_TRUE(d->float_conf.is_dutybeep_enabled == CFG_DFLT_IS_DUTYBEEP_ENABLED);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_variable_erpm, CFG_DFLT_TILTBACK_VARIABLE_ERPM);
    EXPECT_TRUE(d->beeper_enabled == CFG_DFLT_IS_BEEPER_ENABLED);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_eeprom_write_commits_signature_last_and_verifies_data(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);
    d->beeper_enabled = true;

    size_t stores_before_oom = vesc_if_fake_store_eeprom_var_calls();
    vesc_if_fake_fail_next_malloc();
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores_before_oom);

    vesc_if_fake_set_eeprom_store_fail_call(2u);
    d->beep_num_left = 0;
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_EQ_U32(vesc_if_fake_eeprom_word(0), 0u);
    EXPECT_EQ_U32(d->beep_num_left, 0u);

    vesc_if_fake_set_eeprom_store_fail_call(0u);
    vesc_if_fake_set_eeprom_store_discard(true);
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_EQ_U32(d->beep_num_left, 0u);
    EXPECT_TRUE(vesc_if_fake_read_eeprom_var_calls() > 0u);

    vesc_if_fake_set_eeprom_store_discard(false);
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_TRUE(vesc_if_fake_eeprom_word(0) != 0u);
    EXPECT_TRUE(vesc_if_fake_last_eeprom_store_address() == 0);
    EXPECT_TRUE(d->beep_num_left > 0);

    vesc_if_fake_set_eeprom_store_discard(true);
    refloat_main_write_cfg_to_eeprom(d);
    vesc_if_fake_set_eeprom_store_discard(false);

    size_t stores = vesc_if_fake_store_eeprom_var_calls();
    d->state.state = STATE_RUNNING;
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores);
    d->state.state = STATE_READY;

    size_t reads = vesc_if_fake_read_eeprom_var_calls();
    vesc_if_fake_set_eeprom_read_fail_call(reads + 1u);
    refloat_main_write_cfg_to_eeprom(d);
    vesc_if_fake_set_eeprom_read_fail_call(0u);

    reads = vesc_if_fake_read_eeprom_var_calls();
    vesc_if_fake_set_eeprom_read_fail_call(reads + 2u);
    refloat_main_write_cfg_to_eeprom(d);
    vesc_if_fake_set_eeprom_read_fail_call(0u);

    const size_t words = (SERIALIZED_CONFIG_LENGTH + 3u) / 4u;
    stores = vesc_if_fake_store_eeprom_var_calls();
    vesc_if_fake_set_eeprom_store_fail_call(stores + words + 1u);
    refloat_main_write_cfg_to_eeprom(d);
    vesc_if_fake_set_eeprom_store_fail_call(0u);

    reads = vesc_if_fake_read_eeprom_var_calls();
    vesc_if_fake_set_eeprom_read_fail_call(reads + words + 1u);
    refloat_main_write_cfg_to_eeprom(d);
    vesc_if_fake_set_eeprom_read_fail_call(0u);

    stores = vesc_if_fake_store_eeprom_var_calls();
    vesc_if_fake_set_eeprom_store_discard_call(stores + words + 1u);
    refloat_main_write_cfg_to_eeprom(d);
    vesc_if_fake_set_eeprom_store_discard_call(0u);

    stores = vesc_if_fake_store_eeprom_var_calls();
    d->float_conf.kp += 1.0f;
    vesc_if_fake_set_eeprom_store_fail_call(stores + 2u);
    refloat_main_write_cfg_to_eeprom(d);
    EXPECT_EQ_U32(vesc_if_fake_eeprom_word(0), 0u);

    d->float_conf.kp = -9.0f;
    refloat_main_read_cfg_from_eeprom(d);
    RefloatConfig defaults = {0};
    confparser_set_defaults_refloatconfig(&defaults);
    EXPECT_FLOAT_NEAR(d->float_conf.kp, defaults.kp);

    vesc_if_fake_set_eeprom_store_fail_call(0u);
    d->float_conf.kp = 2.0f;
    refloat_main_write_cfg_to_eeprom(d);

    RefloatConfig candidate = d->float_conf;
    candidate.kp = 3.0f;
    uint8_t config[1024] = {0};
    confparser_serialize_refloatconfig(config, &candidate);
    stores = vesc_if_fake_store_eeprom_var_calls();
    vesc_if_fake_set_eeprom_store_fail_call(stores + 2u);
    EXPECT_TRUE(!vesc_if_fake_set_custom_config(config));
    EXPECT_FLOAT_NEAR(d->float_conf.kp, 2.0f);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_rejects_config_commands_in_special_modes(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);
    refloat_main_write_cfg_to_eeprom(d);

    const Mode modes[] = {MODE_HANDTEST, MODE_FLYWHEEL};
    uint8_t save[] = {101, MAIN_COMMAND_CFG_SAVE};
    uint8_t lock[] = {101, MAIN_COMMAND_LOCK, 1};
    uint8_t restore[] = {101, MAIN_COMMAND_CFG_RESTORE};
    uint8_t defaults[] = {101, MAIN_COMMAND_TUNE_DEFAULTS};
    uint8_t tune[14] = {101, MAIN_COMMAND_RT_TUNE};
    uint8_t other[14] = {101, MAIN_COMMAND_TUNE_OTHER};
    uint8_t tilt[] = {101, MAIN_COMMAND_TUNE_TILT, 0, 0, 50, 20, 10};
    uint8_t booster[] = {101, MAIN_COMMAND_BOOSTER, 0, 0, 0, 0};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        d->state.mode = modes[i];
        d->float_conf.disabled = false;
        d->float_conf.ki = 0.0f;
        d->float_conf.fault_darkride_enabled = false;
        d->float_conf.tiltback_duty = 0.1f;
        d->float_conf.booster_angle = 100.0f;
        size_t stores = vesc_if_fake_store_eeprom_var_calls();

        vesc_if_fake_invoke_app_data_handler(save, sizeof(save));
        EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores);

        vesc_if_fake_invoke_app_data_handler(lock, sizeof(lock));
        EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores);
        EXPECT_TRUE(!d->float_conf.disabled);

        vesc_if_fake_invoke_app_data_handler(restore, sizeof(restore));
        vesc_if_fake_invoke_app_data_handler(defaults, sizeof(defaults));
        vesc_if_fake_invoke_app_data_handler(tune, sizeof(tune));
        vesc_if_fake_invoke_app_data_handler(other, sizeof(other));
        vesc_if_fake_invoke_app_data_handler(tilt, sizeof(tilt));
        vesc_if_fake_invoke_app_data_handler(booster, sizeof(booster));
        EXPECT_FLOAT_NEAR(d->float_conf.ki, 0.0f);
        EXPECT_TRUE(!d->float_conf.fault_darkride_enabled);
        EXPECT_FLOAT_NEAR(d->float_conf.tiltback_duty, 0.1f);
        EXPECT_FLOAT_NEAR(d->float_conf.booster_angle, 100.0f);
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_remote_respects_ready_state_guards(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    float imu[3] = {0};
    uint8_t remote[] = {101, MAIN_COMMAND_REMOTE, 127};
    const systime_t now = 20u * SYSTEM_TICK_RATE_HZ;

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    d->float_conf.remote_throttle_grace_period = 10.0f;
    d->time.now = now;
    d->time.disengage_timer = now - 5u * SYSTEM_TICK_RATE_HZ;
    remote[2] = 128;
    vesc_if_fake_invoke_app_data_handler(remote, sizeof(remote));
    EXPECT_FLOAT_NEAR(d->remote.input, 0.0f);
    remote[2] = 127;
    vesc_if_fake_invoke_app_data_handler(remote, sizeof(remote));
    size_t current_requests = vesc_if_fake_mc_set_current_off_delay_calls();
    vesc_if_fake_invoke_imu_callback(imu, imu, imu, 0.002f);
    EXPECT_EQ_SIZE(vesc_if_fake_mc_set_current_off_delay_calls(), current_requests);

    d->time.disengage_timer = 0;
    const struct {
        Mode mode;
        bool charging;
    } unsafe[] = {
        {MODE_HANDTEST, false},
        {MODE_FLYWHEEL, false},
        {MODE_NORMAL, true},
    };
    for (size_t i = 0; i < sizeof(unsafe) / sizeof(unsafe[0]); ++i) {
        d->state.mode = unsafe[i].mode;
        d->state.charging = unsafe[i].charging;
        vesc_if_fake_invoke_app_data_handler(remote, sizeof(remote));
        current_requests = vesc_if_fake_mc_set_current_off_delay_calls();
        vesc_if_fake_invoke_imu_callback(imu, imu, imu, 0.002f);
        EXPECT_EQ_SIZE(vesc_if_fake_mc_set_current_off_delay_calls(), current_requests);
    }

    d->state.state = STATE_RUNNING;
    d->state.mode = MODE_NORMAL;
    d->state.charging = false;
    d->time.disengage_timer = 0;
    vesc_if_fake_invoke_app_data_handler(remote, sizeof(remote));
    d->state.state = STATE_READY;
    d->time.disengage_timer = d->time.now;
    current_requests = vesc_if_fake_mc_set_current_off_delay_calls();
    vesc_if_fake_invoke_imu_callback(imu, imu, imu, 0.002f);
    EXPECT_EQ_SIZE(vesc_if_fake_mc_set_current_off_delay_calls(), current_requests);

    const struct {
        RunState state;
        Mode mode;
        bool charging;
    } blocked[] = {
        {STATE_READY, MODE_HANDTEST, false},
        {STATE_READY, MODE_FLYWHEEL, false},
        {STATE_READY, MODE_NORMAL, true},
        {STATE_STARTUP, MODE_NORMAL, false},
        {STATE_DISABLED, MODE_NORMAL, false},
    };
    for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); ++i) {
        remote_reset(&d->remote, &d->time);
        d->state.state = blocked[i].state;
        d->state.mode = blocked[i].mode;
        d->state.charging = blocked[i].charging;
        d->time.disengage_timer = 0;
        vesc_if_fake_invoke_app_data_handler(remote, sizeof(remote));

        d->state.state = STATE_READY;
        d->state.mode = MODE_NORMAL;
        d->state.charging = false;
        current_requests = vesc_if_fake_mc_set_current_off_delay_calls();
        vesc_if_fake_invoke_imu_callback(imu, imu, imu, 0.002f);
        EXPECT_EQ_SIZE(vesc_if_fake_mc_set_current_off_delay_calls(), current_requests);
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_rejects_config_writes_while_running(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    fixture.data->state.state = STATE_RUNNING;
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);

    size_t stores = vesc_if_fake_store_eeprom_var_calls();
    uint8_t save[] = {101, MAIN_COMMAND_CFG_SAVE};
    vesc_if_fake_invoke_app_data_handler(save, sizeof(save));
    EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores);

    uint8_t lock[] = {101, MAIN_COMMAND_LOCK, 1};
    vesc_if_fake_invoke_app_data_handler(lock, sizeof(lock));
    EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores);

    uint8_t config[1024] = {0};
    EXPECT_TRUE(vesc_if_fake_get_custom_config(config, false) > 0);
    EXPECT_TRUE(!vesc_if_fake_set_custom_config(config));
    EXPECT_EQ_SIZE(vesc_if_fake_store_eeprom_var_calls(), stores);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_serialized_configuration_keeps_control_outputs_finite(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    uint8_t valid[1024] = {0};
    int len = vesc_if_fake_get_custom_config(valid, false);
    EXPECT_TRUE(len > 8);
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);

    static const MainSerializedConfigMutation mutations[] = {
        {.label = "zero", .value = 0u},
        {.label = "maximum byte", .value = UINT8_MAX},
        {.label = "maximum pair", .value = UINT8_MAX, .also_set_next_byte = true},
    };
    for (int i = 4; i < len; ++i) {
        for (size_t mutation_i = 0; mutation_i < sizeof(mutations) / sizeof(mutations[0]);
             ++mutation_i) {
            const MainSerializedConfigMutation *mutation = &mutations[mutation_i];
            uint8_t candidate[sizeof(valid)];
            memcpy(candidate, valid, (size_t) len);
            candidate[i] = mutation->value;
            if (mutation->also_set_next_byte && i + 1 < len) {
                candidate[i + 1] = UINT8_MAX;
            }
            d->state.state = STATE_READY;
            d->state.mode = MODE_NORMAL;
            if (!vesc_if_fake_set_custom_config(candidate)) {
                continue;
            }

            float acc[3] = {0.0f, 0.0f, 1.0f};
            float gyro[3] = {0.0f};
            float mag[3] = {0.0f};
            d->state.state = STATE_RUNNING;
            vesc_if_fake_invoke_imu_callback(acc, gyro, mag, 0.001f);
            EXPECT_TRUE(main_serialized_config_keeps_control_output_finite(d, i, mutation));
        }
    }

    main_protocol_fixture_stop(&fixture);
    return true;
}
