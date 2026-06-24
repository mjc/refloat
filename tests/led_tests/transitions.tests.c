static bool test_leds_update_paints_and_fades_running_lights(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 1.0f, STATE_READY);
    update_leds_at(&fixture, FS_BOTH, 1.1f);

    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);
    EXPECT_FLOAT_NEAR(fixture.leds.last_updated, 1.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 1.0f / 3.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 1.0f / 3.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_status_confirm_respects_animation_window(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 10.0f, STATE_READY);

    leds_status_confirm(&fixture.leds);
    EXPECT_FLOAT_NEAR(fixture.leds.confirm_animation_start, 10.0f);

    vesc_if_fake_set_seconds(10.5f);
    leds_status_confirm(&fixture.leds);
    EXPECT_FLOAT_NEAR(fixture.leds.confirm_animation_start, 10.0f);

    vesc_if_fake_set_seconds(11.0f);
    leds_status_confirm(&fixture.leds);
    EXPECT_FLOAT_NEAR(fixture.leds.confirm_animation_start, 11.0f);

    static const float samples[] = {
        11.0f, 11.03f, 11.2f, 11.4f, 11.6f, 11.79f, 11.8f, 11.97f, 12.0f
    };
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        update_leds_at(&fixture, FS_NONE, samples[i]);
    }
    EXPECT_EQ_U32(led_driver_fake_paint_calls(), sizeof(samples) / sizeof(samples[0]));

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_startup_state_updates_timestamp_without_painting(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 3.0f, STATE_STARTUP);

    update_leds_at(&fixture, FS_BOTH, 3.5f);

    EXPECT_FLOAT_NEAR(fixture.leds.last_updated, 3.5f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 0);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_disabled_state_paints_disabled_animation_and_resets_fade(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 5.0f, STATE_READY);
    update_leds_at(&fixture, FS_NONE, 5.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.1f);

    fixture.state.state = STATE_DISABLED;
    update_leds_at(&fixture, FS_NONE, 5.2f);

    EXPECT_FLOAT_NEAR(fixture.leds.on_off_fade, 0.0f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_FLOAT_NEAR(fixture.leds.animation_start, 5.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_idle_time, 5.2f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 5.2f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_lifted_ready_board_blends_status_onto_front_strip(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 6.0f, STATE_READY);

    vesc_if_fake_set_imu(deg2rad(65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 6.1f);

    EXPECT_TRUE(fixture.leds.board_is_upright);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_blend, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.front_strip.brightness, fixture.cfg.front.brightness - 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.rear_strip.brightness, fixture.cfg.rear.brightness - 0.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_idle_time, 6.1f);

    fixture.cfg.lights_off_when_lifted = false;
    float animation_start = fixture.leds.animation_start;
    vesc_if_fake_set_imu(deg2rad(45.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 6.2f);

    EXPECT_TRUE(!fixture.leds.board_is_upright);
    EXPECT_FLOAT_NEAR(fixture.leds.status_on_front_blend, 0.9f);
    EXPECT_FLOAT_NEAR(fixture.leds.animation_start, animation_start);

    fixture.leds.board_is_upright = true;
    fixture.cfg.lights_off_when_lifted = true;
    update_leds_at(&fixture, FS_NONE, 6.25f);
    EXPECT_FLOAT_NEAR(fixture.leds.animation_start, 6.25f);

    vesc_if_fake_set_imu(deg2rad(-65.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    update_leds_at(&fixture, FS_NONE, 6.3f);
    EXPECT_TRUE(fixture.leds.board_is_upright);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_running_state_hides_sensor_indicators_when_configured(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 7.0f, STATE_READY);
    update_leds_at(&fixture, FS_BOTH, 7.1f);
    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 1.0f / 3.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 1.0f / 3.0f);

    fixture.state.state = STATE_RUNNING;
    update_leds_at(&fixture, FS_BOTH, 7.2f);

    EXPECT_FLOAT_NEAR(fixture.leds.left_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.right_sensor, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.split_distance, 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_running_entry_direction_and_headlight_transition(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 15.0f, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 15.1f);
    EXPECT_TRUE(!fixture.leds.headlights_on);
    EXPECT_TRUE(fixture.leds.direction_forward);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-3.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 2.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 15.2f);

    EXPECT_TRUE(!fixture.leds.direction_forward);
    EXPECT_FLOAT_NEAR(fixture.leds.dir_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_time, 15.2f);
    EXPECT_TRUE(!fixture.leds.headlights_on);
    EXPECT_TRUE(fixture.leds.front_time_target == &fixture.cfg.taillights);
    EXPECT_TRUE(fixture.leds.rear_time_target == &fixture.cfg.headlights);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 15.8f);
    EXPECT_TRUE(!fixture.leds.headlights_on);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, 0.2f);

    vesc_if_fake_set_motor_telemetry(0, 0, 4.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 16.3f);
    EXPECT_TRUE(fixture.leds.headlights_on);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_time, 0.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, 1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.split_distance, 4.0f);
    EXPECT_TRUE(fixture.leds.front_bar == &fixture.cfg.taillights);
    EXPECT_TRUE(fixture.leds.rear_bar == &fixture.cfg.headlights);

    leds_set_headlights_enabled(&fixture.leds, false);
    update_leds_at(&fixture, FS_NONE, 16.4f);
    EXPECT_TRUE(fixture.leds.front_time_target == &fixture.cfg.front);
    EXPECT_TRUE(fixture.leds.rear_time_target == &fixture.cfg.rear);

    fixture.state.state = STATE_READY;
    fixture.leds.headlights_on = true;
    fixture.leds.headlights_time = 0.0f;
    update_leds_at(&fixture, FS_NONE, 16.5f);
    EXPECT_TRUE(fixture.leds.headlights_time > 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_running_transition_repaints_each_tick(void) {
    reset_fakes(20.0f);

    LedsFixture fixture = leds_fixture_prepare(20.0f);
    fixture.cfg.headlights_transition = LED_TRANS_FADE_OUT_IN;
    fixture.cfg.direction_transition = LED_TRANS_MONO_CIPHER;
    leds_fixture_start(&fixture, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 20.1f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 1);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-4.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 3.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 20.2f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2);
    EXPECT_FLOAT_NEAR(fixture.leds.dir_trans.split, -1.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.headlights_trans.split, -1.0f);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.4f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 20.4f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 3);
    EXPECT_TRUE(fixture.leds.headlights_trans.split > -1.0f);
    EXPECT_TRUE(fixture.leds.headlights_trans.split < 1.0f);

    float split_after_first_transition_tick = fixture.leds.headlights_trans.split;
    vesc_if_fake_set_motor_telemetry(0, 0, 3.8f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 20.6f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 4);
    EXPECT_TRUE(fixture.leds.headlights_trans.split > split_after_first_transition_tick);
    EXPECT_TRUE(fixture.leds.headlights_trans.split < 1.0f);

    update_leds_at(&fixture, FS_NONE, 20.8f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 5);
    EXPECT_TRUE(fixture.leds.headlights_trans.split > 0.0f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_ignores_nonfinite_distance_for_direction_transition(void) {
    LedsFixture fixture;
    leds_fixture_setup(&fixture, 25.0f, STATE_RUNNING);
    fixture.leds.headlights_on = false;
    fixture.leds.headlights_time = 25.0f;
    fixture.leds.headlights_trans.split = 0.9f;
    fixture.leds.split_distance = 7.0f;
    fixture.leds.dir_trans.split = 1.0f;
    vesc_if_fake_set_motor_telemetry(0, 0, NAN, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);

    update_leds_at(&fixture, FS_NONE, 25.1f);

    EXPECT_FLOAT_NEAR(fixture.leds.split_distance, 7.0f);
    EXPECT_FLOAT_NEAR(fixture.leds.dir_trans.split, 1.0f);
    EXPECT_TRUE(isfinite(fixture.leds.split_distance));
    EXPECT_TRUE(isfinite(fixture.leds.dir_trans.split));

    fixture.leds.headlights_time = 0.0f;
    vesc_if_fake_set_motor_telemetry(0, 0, NAN, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 25.15f);
    EXPECT_FLOAT_NEAR(fixture.leds.split_distance, 7.0f);

    vesc_if_fake_set_motor_telemetry(0, 0, 6.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 25.2f);
    EXPECT_TRUE(!fixture.leds.direction_forward);

    fixture.leds.dir_trans.split = -1.0f;
    fixture.leds.split_distance = 6.0f;
    fixture.leds.direction_forward = false;
    vesc_if_fake_set_motor_telemetry(0, 0, 7.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 25.3f);
    EXPECT_TRUE(fixture.leds.direction_forward);

    fixture.state.state = STATE_READY;
    fixture.cfg.status_on_front_when_lifted = true;
    fixture.leds.status_on_front_blend = 1.0f;
    fixture.leds.status_strip.length = 0;
    fixture.leds.front_strip.length = 0;
    update_leds_at(&fixture, FS_NONE, 25.4f);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_cipher_transition_golden_colors(void) {
    reset_fakes(30.0f);

    LedsFixture fixture = leds_fixture_prepare(30.0f);
    fixture.cfg.headlights_transition = LED_TRANS_CIPHER;
    fixture.cfg.direction_transition = LED_TRANS_CIPHER;
    fixture.cfg.front = solid_bar(1.0f, COLOR_BLUE);
    fixture.cfg.rear = solid_bar(1.0f, COLOR_GREEN);
    fixture.cfg.headlights = solid_bar(1.0f, COLOR_WHITE_RGB);
    fixture.cfg.taillights = solid_bar(1.0f, COLOR_RED);
    fixture.cfg.status.idle_timeout = 0;
    leds_fixture_start(&fixture, STATE_READY);

    update_leds_at(&fixture, FS_NONE, 30.1f);

    fixture.state.state = STATE_RUNNING;
    vesc_if_fake_set_imu(deg2rad(-4.0f), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    vesc_if_fake_set_motor_telemetry(0, 0, 2.5f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 30.2f);

    vesc_if_fake_set_motor_telemetry(0, 0, 3.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f);
    update_leds_at(&fixture, FS_NONE, 30.6f);

    EXPECT_TRUE(fixture.leds.front_strip.length == 3);
    EXPECT_TRUE(fixture.leds.rear_strip.length == 3);
    EXPECT_TRUE(fixture.leds.front_strip.data[0] == 0x00000000u);
    EXPECT_TRUE(fixture.leds.front_strip.data[1] == 0x00274d2cu);
    EXPECT_TRUE(fixture.leds.front_strip.data[2] == 0x00264c23u);
    EXPECT_TRUE(fixture.leds.rear_strip.data[0] == 0x00264c23u);
    EXPECT_TRUE(fixture.leds.rear_strip.data[1] == 0x00274d2cu);
    EXPECT_TRUE(fixture.leds.rear_strip.data[2] == 0x00000000u);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 3);

    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_invalid_transition_falls_back_to_fade(void) {
    LedsFixture fixture;
    fixture = leds_fixture_prepare(1.0f);
    fixture.cfg.headlights_transition = (LedTransition) 255;
    fixture.cfg.direction_transition = (LedTransition) 255;
    leds_fixture_start(&fixture, STATE_RUNNING);

    update_leds_at(&fixture, FS_NONE, 1.1f);

    EXPECT_TRUE(led_driver_fake_paint_calls() == 1u);
    EXPECT_TRUE(fixture.leds.front_strip.data[0] != 0u);
    leds_fixture_destroy(&fixture);

    fixture = leds_fixture_prepare(2.0f);
    fixture.cfg.headlights_transition = LED_TRANS_MONO_CIPHER;
    fixture.cfg.direction_transition = LED_TRANS_MONO_CIPHER;
    fixture.cfg.headlights.mode = LED_ANIM_FADE;
    leds_fixture_start(&fixture, STATE_RUNNING);
    update_leds_at(&fixture, FS_NONE, 2.1f);
    update_leds_at(&fixture, FS_NONE, 3.2f);
    EXPECT_TRUE(led_driver_fake_paint_calls() == 2u);
    leds_fixture_destroy(&fixture);
    return true;
}

static bool test_leds_render_valid_runtime_states(void) {
    const RunState states[] = {STATE_STARTUP, STATE_READY, STATE_RUNNING, STATE_DISABLED};
    const float pitches[] = {-70.0f, 0.0f, 70.0f};
    const FootpadSensorState footpads[] = {FS_NONE, FS_LEFT, FS_RIGHT, FS_BOTH};

    for (size_t old_i = 0; old_i < 4; ++old_i) {
        for (size_t state_i = 0; state_i < 4; ++state_i) {
            for (size_t pitch_i = 0; pitch_i < 3; ++pitch_i) {
                for (size_t footpad_i = 0; footpad_i < 4; ++footpad_i) {
                    for (uint8_t variant = 0; variant < 16; ++variant) {
                        LedsFixture fixture;
                        leds_fixture_setup(&fixture, 40.0f, states[state_i]);
                        fixture.leds.state.state = states[old_i];
                        fixture.state.darkride = variant & 1;
                        fixture.cfg.lights_off_when_lifted = variant & 2;
                        fixture.cfg.status_on_front_when_lifted = variant & 4;
                        fixture.cfg.status.show_sensors_while_running = variant & 8;
                        fixture.leds.on_off_fade = (variant & 4) ? 1.0f : 0.0f;
                        fixture.leds.status_idle_blend = (variant & 8) ? 1.0f : 0.0f;
                        fixture.leds.status_utilization_blend = (variant & 4) ? 1.0f : 0.0f;
                        fixture.leds.left_sensor = (variant & 2) ? 1.0f : 0.0f;
                        fixture.leds.right_sensor = (variant & 1) ? 1.0f : 0.0f;
                        fixture.leds.headlights_on = variant & 8;
                        fixture.leds.headlights_time = (variant & 8) ? 39.5f : 0.0f;
                        fixture.leds.dir_trans.split = (variant & 4) ? 0.0f
                            : (variant & 2)                          ? -1.0f
                                                                     : 1.0f;
                        leds_set_enabled(&fixture.leds, variant & 1);
                        leds_set_headlights_enabled(&fixture.leds, variant & 2);
                        vesc_if_fake_set_imu(
                            deg2rad(pitches[pitch_i]), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
                        );
                        vesc_if_fake_set_motor_telemetry(
                            0, 0, 1.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f
                        );

                        update_leds_at(&fixture, footpads[footpad_i], 40.1f);
                        vesc_if_fake_set_motor_telemetry(
                            0, 0, -1.0f, 0, 0, 0, 0, 50.0f, 25.0f, 25.0f
                        );
                        update_leds_at(&fixture, footpads[footpad_i], 43.5f);
                        update_leds_at(&fixture, footpads[footpad_i], 43.6f);
                        update_leds_at(&fixture, footpads[footpad_i], 43.7f);
                        update_leds_at(&fixture, footpads[footpad_i], 43.8f);

                        EXPECT_TRUE(fixture.leds.led_data != NULL);
                        EXPECT_FLOAT_NEAR(fixture.leds.last_updated, 43.8f);
                        leds_fixture_destroy(&fixture);
                        EXPECT_EQ_U32(vesc_if_fake_free_calls(), vesc_if_fake_malloc_calls());
                    }
                }
            }
        }
    }

    return true;
}
