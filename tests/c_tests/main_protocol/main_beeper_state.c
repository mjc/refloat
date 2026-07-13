static bool test_main_beeper_state_machine(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->time.now = 1000;

    d->beeper_enabled = false;
    beep_alert(d, 1, true);
    beep_on(d, false);
    beeper_update(d);
    EXPECT_EQ_U32(d->beep_num_left, 0u);

    d->beeper_enabled = true;
    beep_alert(d, 1, true);
    EXPECT_EQ_U32(d->beep_num_left, 3u);
    EXPECT_FLOAT_NEAR(d->beep_duration, 0.25f);

    beep_alert(d, 3, false);
    EXPECT_EQ_U32(d->beep_num_left, 3u);
    beeper_update(d);
    EXPECT_EQ_U32(d->beep_num_left, 3u);

    d->time.now += 3000;
    beeper_update(d);
    EXPECT_EQ_U32(d->beep_num_left, 2u);
    d->time.now += 3000;
    beeper_update(d);
    EXPECT_EQ_U32(d->beep_num_left, 1u);
    d->time.now += 3000;
    beeper_update(d);
    EXPECT_EQ_U32(d->beep_num_left, 0u);

    beep_off(d, false);
    beep_on(d, false);
    d->beep_num_left = 1;
    beep_off(d, false);
    beep_on(d, false);
    beep_off(d, true);
    beep_on(d, true);

    main_protocol_fixture_stop(&fixture);
    return true;
}

static bool test_main_disabling_beeper_cancels_output_state(void) {
    enum {
        COMMAND_TUNE_OTHER = 6
    };
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;
    d->beeper_enabled = true;
    d->float_conf.is_beeper_enabled = true;
    d->beep_num_left = 5;
    d->duty_beeping = true;
    beep_on(d, true);

    uint8_t disable[14] = {101, COMMAND_TUNE_OTHER};
    vesc_if_fake_invoke_app_data_handler(disable, sizeof(disable));

    EXPECT_TRUE(!d->beeper_enabled);
    EXPECT_EQ_U32(d->beep_num_left, 0u);
    EXPECT_TRUE(!d->duty_beeping);

    main_protocol_fixture_stop(&fixture);
    return true;
}
