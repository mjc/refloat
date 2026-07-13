typedef struct {
    lib_info info;
    Data *data;
} MainLifecycleFixture;

static bool main_lifecycle_fixture_start(MainLifecycleFixture *fixture) {
    vesc_if_fake_reset();
    fixture->info = (lib_info){0};
    EXPECT_TRUE(init(&fixture->info));
    EXPECT_TRUE(fixture->info.arg != NULL);
    EXPECT_TRUE(fixture->info.stop_fun != NULL);
    fixture->data = (Data *) fixture->info.arg;
    vesc_if_fake_set_arg(fixture->info.arg);
    return true;
}

static void main_lifecycle_fixture_stop(MainLifecycleFixture *fixture) {
    fixture->info.stop_fun(fixture->info.arg);
}

static bool test_main_init_and_stop_lifecycle(void) {
    MainLifecycleFixture fixture;
    EXPECT_TRUE(main_lifecycle_fixture_start(&fixture));

    size_t malloc_calls = vesc_if_fake_malloc_calls();
    EXPECT_TRUE(malloc_calls > 0);
    EXPECT_EQ_U32(vesc_if_fake_spawn_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_add_config_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_lbm_add_extension_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 0u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 0u);
    size_t free_calls_before_stop = vesc_if_fake_free_calls();

    main_lifecycle_fixture_stop(&fixture);

    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 2u);
    EXPECT_TRUE(vesc_if_fake_free_calls() > free_calls_before_stop);
    EXPECT_TRUE(vesc_if_fake_free_calls() <= malloc_calls);

    MainLifecycleFixture legacy_fixture = {0};
    vesc_if_fake_reset();
    vesc_if_fake_set_cfg_float(CFG_PARAM_IMU_mahony_kp, 2.0f);
    EXPECT_TRUE(init(&legacy_fixture.info));
    legacy_fixture.data = legacy_fixture.info.arg;
    EXPECT_FLOAT_NEAR(VESC_IF->get_cfg_float(CFG_PARAM_IMU_mahony_kp), 0.2f);
    EXPECT_FLOAT_NEAR(VESC_IF->get_cfg_float(CFG_PARAM_IMU_mahony_ki), 0.0f);
    EXPECT_FLOAT_NEAR(VESC_IF->get_cfg_float(CFG_PARAM_IMU_accel_confidence_decay), 0.1f);
    main_lifecycle_fixture_stop(&legacy_fixture);

    return true;
}

static bool test_main_init_oom_and_stop_without_threads(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fail_next_malloc();
    lib_info failed = {0};
    EXPECT_TRUE(!init(&failed));
    EXPECT_TRUE(failed.arg == NULL);
    EXPECT_TRUE(failed.stop_fun == NULL);

    MainLifecycleFixture fixture;
    EXPECT_TRUE(main_lifecycle_fixture_start(&fixture));
    fixture.data->main_thread = NULL;
    fixture.data->aux_thread = NULL;
    main_lifecycle_fixture_stop(&fixture);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 0u);
    return true;
}

static bool test_main_registered_package_callbacks(void) {
    MainLifecycleFixture fixture;
    EXPECT_TRUE(main_lifecycle_fixture_start(&fixture));

    uint8_t config[1024] = {0};
    int config_size = vesc_if_fake_get_custom_config(config, false);
    EXPECT_TRUE(config_size > 0);
    EXPECT_TRUE(vesc_if_fake_get_custom_config(config, true) == config_size);
    vesc_if_fake_fail_next_malloc();
    EXPECT_TRUE(vesc_if_fake_get_custom_config(config, true) == 0);

    fixture.data->state.mode = MODE_FLYWHEEL;
    EXPECT_TRUE(!vesc_if_fake_set_custom_config(config));

    fixture.data->state.mode = MODE_NORMAL;
    fixture.data->state.state = STATE_RUNNING;
    EXPECT_TRUE(vesc_if_fake_get_custom_config(config, false) == config_size);
    vesc_if_fake_set_eeprom_store_enabled(true);
    size_t stores = vesc_if_fake_store_eeprom_var_calls();
    EXPECT_TRUE(!vesc_if_fake_set_custom_config(config));
    EXPECT_TRUE(vesc_if_fake_store_eeprom_var_calls() == stores);

    fixture.data->state.state = STATE_READY;
    vesc_if_fake_set_eeprom_read_enabled(true);
    vesc_if_fake_fail_next_malloc();
    EXPECT_TRUE(!vesc_if_fake_set_custom_config(config));
    EXPECT_TRUE(vesc_if_fake_set_custom_config(config));
    EXPECT_TRUE(vesc_if_fake_store_eeprom_var_calls() > stores);

    config[0] ^= 1;
    stores = vesc_if_fake_store_eeprom_var_calls();
    EXPECT_TRUE(!vesc_if_fake_set_custom_config(config));
    EXPECT_TRUE(vesc_if_fake_store_eeprom_var_calls() == stores);

    uint8_t *xml = NULL;
    EXPECT_TRUE(vesc_if_fake_get_custom_config_xml(&xml) > 0);
    EXPECT_TRUE(xml != NULL);

    lbm_value fw_version[] = {6, 5, 1};
    EXPECT_EQ_U32(vesc_if_fake_invoke_extension("ext-set-fw-version", fw_version, 0), 1u);
    EXPECT_EQ_U32(vesc_if_fake_invoke_extension("ext-set-fw-version", fw_version, 3), 1u);
    EXPECT_TRUE(fixture.data->fw_version_major == 6);
    EXPECT_TRUE(fixture.data->fw_version_minor == 5);
    EXPECT_TRUE(fixture.data->fw_version_beta == 1);

    lbm_value bms_values[] = {3, 4, 5, 6, 7, 8};
    fixture.data->float_conf.bms.enabled = false;
    EXPECT_EQ_U32(vesc_if_fake_invoke_extension("ext-bms", bms_values, 6), 0u);
    fixture.data->float_conf.bms.enabled = true;
    EXPECT_EQ_U32(vesc_if_fake_invoke_extension("ext-bms", bms_values, 0), 1u);
    EXPECT_EQ_U32(vesc_if_fake_invoke_extension("ext-bms", bms_values, 6), 1u);
    EXPECT_FLOAT_NEAR(fixture.data->bms.cell_lv, 3.0f);
    EXPECT_FLOAT_NEAR(fixture.data->bms.cell_hv, 4.0f);
    EXPECT_TRUE(fixture.data->bms.cell_lt == 5);
    EXPECT_TRUE(fixture.data->bms.cell_ht == 6);
    EXPECT_TRUE(fixture.data->bms.bms_ht == 7);
    EXPECT_FLOAT_NEAR(fixture.data->bms.msg_age, 8.0f);

    float acc[] = {0.0f, 0.0f, 1.0f};
    float gyro[] = {0.0f, 0.0f, 0.0f};
    float mag[] = {0.0f, 0.0f, 0.0f};
    fixture.data->state.state = STATE_STARTUP;
    vesc_if_fake_invoke_imu_callback(acc, gyro, mag, 0.001f);
    fixture.data->state.state = STATE_READY;
    vesc_if_fake_invoke_imu_callback(acc, gyro, mag, 0.001f);
    fixture.data->state.state = STATE_RUNNING;
    vesc_if_fake_invoke_imu_callback(acc, gyro, mag, 0.001f);

    main_lifecycle_fixture_stop(&fixture);
    return true;
}

static bool test_main_fatal_error_terminate_runs_stop_path(void) {
    MainLifecycleFixture fixture;
    EXPECT_TRUE(main_lifecycle_fixture_start(&fixture));

    size_t malloc_calls = vesc_if_fake_malloc_calls();
    size_t free_calls_before_stop = vesc_if_fake_free_calls();

    refloat_main_fatal_error_terminate();

    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 2u);
    EXPECT_TRUE(vesc_if_fake_free_calls() > free_calls_before_stop);
    EXPECT_TRUE(vesc_if_fake_free_calls() <= malloc_calls);
    EXPECT_TRUE(fixture.data != NULL);

    return true;
}

static bool test_main_init_main_thread_spawn_failure_cleans_up(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fail_spawn_call(1);

    lib_info info = {0};
    EXPECT_TRUE(!init(&info));

    EXPECT_EQ_U32(vesc_if_fake_spawn_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 0u);
    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 0u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 0u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 0u);
    EXPECT_TRUE(vesc_if_fake_free_calls() == vesc_if_fake_malloc_calls());
    EXPECT_TRUE(info.arg == NULL);
    EXPECT_TRUE(info.stop_fun == NULL);

    return true;
}

static bool test_main_init_aux_thread_spawn_failure_cleans_up(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fail_spawn_call(2);

    lib_info info = {0};
    EXPECT_TRUE(!init(&info));

    EXPECT_EQ_U32(vesc_if_fake_spawn_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 0u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 0u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 0u);
    EXPECT_TRUE(vesc_if_fake_free_calls() == vesc_if_fake_malloc_calls());
    EXPECT_TRUE(info.arg == NULL);
    EXPECT_TRUE(info.stop_fun == NULL);

    return true;
}

static bool test_main_init_eeprom_allocation_failure_uses_defaults(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fail_malloc_call(2);

    lib_info info = {0};
    EXPECT_TRUE(init(&info));
    EXPECT_TRUE(info.arg != NULL);
    EXPECT_TRUE(info.stop_fun != NULL);
    vesc_if_fake_set_arg(info.arg);

    Data *d = (Data *) info.arg;
    RefloatConfig expected = {0};
    confparser_set_defaults_refloatconfig(&expected);
    EXPECT_TRUE(d->float_conf.inputtilt_remote_type == expected.inputtilt_remote_type);
    EXPECT_FLOAT_NEAR(d->float_conf.tiltback_constant, expected.tiltback_constant);
    EXPECT_FLOAT_NEAR(d->float_conf.leds.front.brightness, expected.leds.front.brightness);

    info.stop_fun(info.arg);

    return true;
}

static bool test_main_init_uses_vesc_frequency_and_optional_beeper_config(void) {
    MainProtocolFixture seed = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&seed));
    vesc_if_fake_set_eeprom_store_enabled(true);
    vesc_if_fake_set_eeprom_read_enabled(true);
    seed.data->float_conf.is_beeper_enabled = false;
    seed.data->float_conf.inputtilt_remote_type = INPUTTILT_PPM;
    refloat_main_write_cfg_to_eeprom(seed.data);
    main_protocol_fixture_stop(&seed);

    vesc_if_fake_set_cfg_int(CFG_PARAM_IMU_sample_rate, 500);
    lib_info info = {0};
    EXPECT_TRUE(init(&info));
    Data *d = info.arg;
    vesc_if_fake_set_arg(d);
    EXPECT_FLOAT_NEAR(d->imu_freq_tracker.frequency.value, 500.0f);
    EXPECT_TRUE(!d->float_conf.is_beeper_enabled);
    EXPECT_EQ_U32(d->float_conf.inputtilt_remote_type, INPUTTILT_PPM);

    d->float_conf.inputtilt_remote_type = INPUTTILT_UART;
    refloat_main_write_cfg_to_eeprom(d);
    info.stop_fun(d);

    info = (lib_info){0};
    EXPECT_TRUE(init(&info));
    d = info.arg;
    EXPECT_TRUE(!d->float_conf.is_beeper_enabled);
    EXPECT_EQ_U32(d->float_conf.inputtilt_remote_type, INPUTTILT_UART);

    d->float_conf.is_beeper_enabled = true;
    refloat_main_write_cfg_to_eeprom(d);
    info.stop_fun(d);

    info = (lib_info){0};
    EXPECT_TRUE(init(&info));
    d = info.arg;
    EXPECT_TRUE(d->float_conf.is_beeper_enabled);
    info.stop_fun(d);
    return true;
}
