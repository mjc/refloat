static bool test_main_init_and_stop_lifecycle(void) {
    vesc_if_fake_reset();

    lib_info info = {0};
    EXPECT_TRUE(init(&info));
    EXPECT_TRUE(info.arg != NULL);
    EXPECT_TRUE(info.stop_fun != NULL);
    vesc_if_fake_set_arg(info.arg);

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

    info.stop_fun(info.arg);

    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 2u);
    EXPECT_TRUE(vesc_if_fake_free_calls() > free_calls_before_stop);
    EXPECT_TRUE(vesc_if_fake_free_calls() <= malloc_calls);

    return true;
}

static bool test_main_fatal_error_terminate_runs_stop_path(void) {
    vesc_if_fake_reset();

    lib_info info = {0};
    EXPECT_TRUE(init(&info));
    EXPECT_TRUE(info.arg != NULL);
    EXPECT_TRUE(info.stop_fun != NULL);
    vesc_if_fake_set_arg(info.arg);

    size_t malloc_calls = vesc_if_fake_malloc_calls();
    size_t free_calls_before_stop = vesc_if_fake_free_calls();

    refloat_main_fatal_error_terminate();

    EXPECT_EQ_U32(vesc_if_fake_imu_set_read_callback_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_set_app_data_handler_calls(), 2u);
    EXPECT_EQ_U32(vesc_if_fake_conf_custom_clear_configs_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_request_terminate_calls(), 2u);
    EXPECT_TRUE(vesc_if_fake_free_calls() > free_calls_before_stop);
    EXPECT_TRUE(vesc_if_fake_free_calls() <= malloc_calls);

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
