static RefloatConfig haptic_test_config(void) {
    RefloatConfig cfg = {0};
    cfg.tiltback_duty = 0.5f;
    cfg.haptic.duty.frequency = 440;
    cfg.haptic.duty.strength = 0.7f;
    cfg.haptic.error.frequency = 880;
    cfg.haptic.error.strength = 0.9f;
    cfg.haptic.vibrate.frequency = 120;
    cfg.haptic.vibrate.strength = 0.25f;
    cfg.haptic.min_strength = 0.4f;
    cfg.haptic.strength_curvature = 0.0f;
    cfg.haptic.max_strength_speed = 10.0f;
    cfg.haptic.duty_solid_offset = 0.1f;
    cfg.haptic.current_threshold = 0.0f;
    return cfg;
}

typedef struct {
    HapticFeedback hf;
    MotorControl mc;
    RefloatConfig cfg;
    State state;
    MotorData md;
    AlertTracker at;
    Time time;
} HapticFixture;

static void haptic_fixture_start(HapticFixture *fixture) {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    *fixture = (HapticFixture){0};
    fixture->cfg = haptic_test_config();
    haptic_feedback_init(&fixture->hf);
    motor_control_init(&fixture->mc);
    motor_control_configure(&fixture->mc, &fixture->cfg, 1000u);
    haptic_feedback_configure(&fixture->hf, &fixture->cfg);
    fixture->state = (State){.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_DUTY};
    fixture->md.speed = 5.0f;
    fixture->time.now = 1000u;
}

static bool test_haptic_feedback_patterns(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_NONE);
    EXPECT_TRUE(fixture.hf.can_change_type);
    EXPECT_FLOAT_NEAR(fixture.hf.duty_solid_threshold, 0.6f);
    EXPECT_TRUE(fixture.hf.str_poly_b > 0.0f);

    fixture.md.duty_cycle.value = 0.7f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_TRUE(vesc_if_fake_foc_play_tone_calls() == 1);
    EXPECT_TRUE(vesc_if_fake_last_foc_channel() == 0);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 440.0f);
    EXPECT_TRUE(
        vesc_if_fake_last_foc_voltage() >
        fixture.cfg.haptic.min_strength * fixture.cfg.haptic.duty.strength
    );
    EXPECT_TRUE(fixture.mc.tone_ticks > 0);
    EXPECT_FLOAT_NEAR(fixture.mc.tone_intensity, fixture.cfg.haptic.vibrate.strength * 0.7f);

    fixture.state.sat = SAT_NONE;
    fixture.at.fatal_error = true;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_FATAL);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 880.0f);

    fixture.at.fatal_error = false;
    fixture.state.sat = SAT_PB_TEMPERATURE;
    fixture.hf.can_change_type = true;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_TEMPERATURE);

    fixture.state.sat = SAT_PB_LOW_VOLTAGE;
    fixture.hf.can_change_type = true;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);

    fixture.state.sat = SAT_NONE;
    fixture.cfg.haptic.current_threshold = 0.5f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    fixture.hf.can_change_type = true;
    fixture.md.motor_current_saturation = 0.75f;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);

    fixture.state.sat = SAT_PB_DUTY;
    fixture.md.duty_cycle.value = 0.55f;
    fixture.hf.can_change_type = true;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_SPEED);

    fixture.time.now = fixture.hf.tone_timer + 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(!fixture.hf.is_playing);
    EXPECT_TRUE(fixture.mc.tone_ticks == 0);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 0.0f);

    fixture.state.mode = MODE_HANDTEST;
    fixture.time.now += 150000u;
    fixture.hf.can_change_type = true;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_NONE);

    return true;
}
static bool test_haptic_feedback_gates_strength_by_state(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.state.state = STATE_READY;
    fixture.at.fatal_error = true;
    fixture.md.duty_cycle.value = 0.8f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_NONE);
    EXPECT_TRUE(!fixture.hf.is_playing);
    EXPECT_TRUE(vesc_if_fake_foc_play_tone_calls() == 0);
    EXPECT_TRUE(fixture.mc.tone_ticks == 0);

    fixture.state.mode = MODE_HANDTEST;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_NONE);
    EXPECT_TRUE(vesc_if_fake_foc_play_tone_calls() == 0);

    fixture.state.mode = MODE_NORMAL;
    fixture.state.state = STATE_RUNNING;
    fixture.at.fatal_error = false;
    fixture.state.sat = SAT_NONE;
    fixture.cfg.haptic.current_threshold = 0.5f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_NONE);

    fixture.state.sat = SAT_PB_DUTY;
    fixture.cfg.haptic.duty.strength = 0.0f;
    fixture.cfg.haptic.vibrate.strength = 0.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_TRUE(vesc_if_fake_foc_play_tone_calls() == 0);
    EXPECT_TRUE(fixture.mc.tone_ticks == 0);

    haptic_feedback_init(&fixture.hf);
    fixture.cfg = haptic_test_config();
    fixture.cfg.haptic.max_strength_speed = 0.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    motor_control_configure(&fixture.mc, &fixture.cfg, 1000u);
    fixture.md.speed = 0.5f;
    fixture.time.now += 1000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    // Raw zero is bounded to the editor's 10 km/h minimum: 0.4 + (0.6 / 10) * 0.5 = 0.43.
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), fixture.cfg.haptic.duty.strength * 0.43f);
    EXPECT_FLOAT_NEAR(fixture.mc.tone_intensity, fixture.cfg.haptic.vibrate.strength * 0.43f);

    return true;
}

static bool test_haptic_feedback_shared_strength_scale(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.min_strength = 0.25f;
    fixture.cfg.haptic.strength_curvature = 0.0f;
    fixture.cfg.haptic.max_strength_speed = 20.0f;
    fixture.cfg.haptic.duty.strength = 0.8f;
    fixture.cfg.haptic.vibrate.strength = 0.3f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    motor_control_configure(&fixture.mc, &fixture.cfg, 1000u);
    fixture.state = (State){.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_DUTY};
    fixture.md.speed = -10.0f;
    fixture.md.duty_cycle.value = 0.8f;

    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );

    float expected_scale = 0.625f;
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_FLOAT_NEAR(
        vesc_if_fake_last_foc_voltage(), fixture.cfg.haptic.duty.strength * expected_scale
    );
    EXPECT_FLOAT_NEAR(
        fixture.mc.tone_intensity, fixture.cfg.haptic.vibrate.strength * expected_scale
    );
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), fixture.cfg.haptic.duty.frequency);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 4u);

    return true;
}

static bool test_haptic_feedback_stops_outputs_when_runtime_config_disables_them(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.md.duty_cycle.value = 0.8f;

    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.mc.tone_ticks > 0);
    EXPECT_TRUE(vesc_if_fake_last_foc_voltage() > 0.0f);

    fixture.cfg.haptic.duty.strength = 0.0f;
    fixture.cfg.haptic.vibrate.strength = 0.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    fixture.time.now += 1000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );

    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 0.0f);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 0u);
    return true;
}

static bool test_haptic_feedback_pattern_type_change_lockout(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.state.sat = SAT_PB_LOW_VOLTAGE;
    fixture.time.now = 2000u;

    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_TRUE(fixture.hf.can_change_type);

    fixture.time.now = fixture.hf.tone_timer + 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    EXPECT_TRUE(!fixture.hf.is_playing);
    EXPECT_TRUE(!fixture.hf.can_change_type);

    fixture.at.fatal_error = true;
    fixture.state.sat = SAT_NONE;
    fixture.time.now += 10000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    EXPECT_TRUE(!fixture.hf.can_change_type);

    fixture.time.now = fixture.hf.tone_timer + 801000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.can_change_type);

    fixture.time.now += 1000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_FATAL);

    return true;
}

static bool test_haptic_feedback_selects_configured_pattern(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.state.sat = SAT_PB_SPEED;
    fixture.md.speed = -3.0f;
    fixture.time.now = 3000u;

    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_SPEED);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), fixture.cfg.haptic.duty.frequency);

    fixture.state.sat = SAT_PB_HIGH_VOLTAGE;
    fixture.hf.can_change_type = true;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), fixture.cfg.haptic.error.frequency);

    fixture.state.sat = SAT_PB_ERROR;
    fixture.hf.can_change_type = true;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);

    haptic_feedback_init(&fixture.hf);
    fixture.state.sat = SAT_NONE;
    fixture.cfg.haptic.current_threshold = 0.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_NONE);

    fixture.cfg.haptic.current_threshold = 0.5f;
    fixture.md.motor_current_saturation = 0.75f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);

    haptic_feedback_init(&fixture.hf);
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    fixture.state.sat = SAT_PB_DUTY;
    fixture.md.duty_cycle.value = 0.8f;
    size_t foc_calls = vesc_if_fake_foc_play_tone_calls();
    fake_vesc_if.foc_play_tone = NULL;
    fixture.time.now += 100000u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_TRUE(vesc_if_fake_foc_play_tone_calls() == foc_calls);
    EXPECT_TRUE(fixture.mc.tone_ticks > 0);

    return true;
}

static bool test_haptic_feedback_pauses_error_pattern(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.state.sat = SAT_PB_TEMPERATURE;
    fixture.md.speed = 0.0f;
    fixture.time.now = 4000u;

    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_ERROR_TEMPERATURE);
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), fixture.cfg.haptic.error.frequency);

    fixture.time.now = fixture.hf.tone_timer + 1100u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(!fixture.hf.is_playing);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 0.0f);

    fixture.time.now = fixture.hf.tone_timer + 2100u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), fixture.cfg.haptic.error.frequency);

    fixture.time.now = fixture.hf.tone_timer + 3100u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(!fixture.hf.is_playing);
    size_t calls_before_skipped_beat = vesc_if_fake_foc_play_tone_calls();

    fixture.time.now = fixture.hf.tone_timer + 4100u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(!fixture.hf.is_playing);
    EXPECT_TRUE(vesc_if_fake_foc_play_tone_calls() == calls_before_skipped_beat);

    fixture.time.now = fixture.hf.tone_timer + 6100u;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.is_playing);
    EXPECT_TRUE(fixture.hf.can_change_type);

    return true;
}

static bool test_haptic_feedback_bounds_bluetooth_config_values(void) {
    HapticFixture fixture;
    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.min_strength = 0.0f;
    fixture.cfg.haptic.strength_curvature = 0.0f;
    fixture.cfg.haptic.max_strength_speed = UINT8_MAX;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    EXPECT_FLOAT_NEAR(fixture.hf.str_poly_b, 0.01f);
    EXPECT_FLOAT_NEAR(fixture.hf.str_poly_c, 0.0f);

    fixture.cfg.tiltback_duty = -1.0f;
    fixture.cfg.haptic.duty_solid_offset = 0.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    EXPECT_FLOAT_NEAR(fixture.hf.duty_solid_threshold, 0.0f);

    fixture.cfg.tiltback_duty = 0.8f;
    fixture.cfg.haptic.duty_solid_offset = -1.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    EXPECT_FLOAT_NEAR(fixture.hf.duty_solid_threshold, 0.8f);

    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.current_threshold = 2.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    fixture.state.sat = SAT_NONE;
    fixture.md.motor_current_saturation = 1.5f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_TRUE(fixture.hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);

    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.duty.frequency = 0;
    fixture.cfg.haptic.duty.strength = 100.0f;
    fixture.cfg.haptic.vibrate.frequency = 1;
    fixture.cfg.haptic.vibrate.strength = 100.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    motor_control_configure(&fixture.mc, &fixture.cfg, 1000u);
    fixture.md.speed = 100.0f;
    fixture.md.duty_cycle.value = 0.9f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 300.0f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 12.0f);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 50u);
    EXPECT_FLOAT_NEAR(fixture.mc.tone_intensity, 25.0f);

    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.duty.frequency = UINT16_MAX;
    fixture.cfg.haptic.vibrate.frequency = UINT16_MAX;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    motor_control_configure(&fixture.mc, &fixture.cfg, 1000u);
    fixture.md.speed = 100.0f;
    fixture.md.duty_cycle.value = 0.9f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 1000.0f);
    EXPECT_EQ_U32(fixture.mc.tone_ticks, 2u);

    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.error.frequency = 0;
    fixture.cfg.haptic.error.strength = 100.0f;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    fixture.state.sat = SAT_PB_LOW_VOLTAGE;
    fixture.md.speed = 100.0f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 300.0f);
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 12.0f);

    haptic_fixture_start(&fixture);
    fixture.cfg.haptic.error.frequency = UINT16_MAX;
    haptic_feedback_configure(&fixture.hf, &fixture.cfg);
    fixture.state.sat = SAT_PB_LOW_VOLTAGE;
    fixture.md.speed = 100.0f;
    haptic_feedback_update(
        &fixture.hf, &fixture.mc, &fixture.state, &fixture.md, &fixture.at, &fixture.time
    );
    EXPECT_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 1000.0f);
    return true;
}
