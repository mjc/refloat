#include "../c_support.hpp"

namespace {

static RefloatConfig haptic_test_config(void) {
    RefloatConfig cfg = {};
    cfg.tiltback_duty = 0.5f;
    cfg.haptic.duty.frequency = 440;
    cfg.haptic.duty.strength = 0.7f;
    cfg.haptic.error.frequency = 880;
    cfg.haptic.error.strength = 0.9f;
    cfg.haptic.vibrate.frequency = 1200;
    cfg.haptic.vibrate.strength = 0.25f;
    cfg.haptic.min_strength = 0.4f;
    cfg.haptic.strength_curvature = 0.0f;
    cfg.haptic.max_strength_speed = 10.0f;
    cfg.haptic.duty_solid_offset = 0.1f;
    cfg.haptic.current_threshold = 0.0f;
    return cfg;
}

TEST_CASE("konami sequence and timeout", "[c]") {
    feedback_fakes_reset();
    Time time = {.now = 2000u};
    const FootpadSensorState sequence[] = {FS_LEFT, FS_RIGHT, FS_BOTH};
    Konami konami;
    konami_init(&konami, sequence, 3);

    FootpadSensor fs = {.state = FS_LEFT};
    Leds leds = {};

    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);
    REQUIRE(feedback_fakes_led_confirm_calls() == 0);

    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);

    time.now += 2000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);

    fs.state = FS_BOTH;
    time.now += 200u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);

    fs.state = FS_LEFT;
    time.now += 2000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);

    time.now += 6000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);

    time.now += 2000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);
    fs.state = FS_RIGHT;
    time.now += 2000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 2);
    fs.state = FS_BOTH;
    time.now += 2000u;
    REQUIRE(konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 1);
}

TEST_CASE("konami boundary and idle inputs", "[c]") {
    feedback_fakes_reset();
    const FootpadSensorState sequence[] = {FS_LEFT, FS_RIGHT};
    Konami konami;
    konami_init(&konami, sequence, 2);

    Time time = {.now = 10000u};
    FootpadSensor fs = {.state = FS_RIGHT};
    Leds leds = {};

    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 0);

    fs.state = FS_LEFT;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);
    CHECK_U32(konami.timer, time.now);

    time.now += 1500u;
    fs.state = FS_RIGHT;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);

    time.now += 1u;
    REQUIRE(konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 1);

    fs.state = FS_LEFT;
    time.now += 2000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);

    time.now += 5000u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 1);

    time.now += 1u;
    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 1);
}

TEST_CASE("konami single step sequence", "[c]") {
    feedback_fakes_reset();
    const FootpadSensorState sequence[] = {FS_BOTH};
    Konami konami;
    konami_init(&konami, sequence, 1);

    Time time = {.now = 2000u};
    FootpadSensor fs = {.state = FS_LEFT};
    Leds leds = {};

    REQUIRE(!konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 0);

    fs.state = FS_BOTH;
    REQUIRE(konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 1);

    REQUIRE(konami_check(&konami, &leds, &fs, &time));
    REQUIRE(konami.state == 0);
    REQUIRE(feedback_fakes_led_confirm_calls() == 2);
}

TEST_CASE("haptic feedback patterns", "[c]") {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    HapticFeedback hf;
    haptic_feedback_init(&hf);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_NONE);
    REQUIRE(hf.can_change_type);

    RefloatConfig cfg = {};
    cfg.tiltback_duty = 0.5f;
    cfg.haptic.duty.frequency = 440;
    cfg.haptic.duty.strength = 0.7f;
    cfg.haptic.error.frequency = 880;
    cfg.haptic.error.strength = 0.9f;
    cfg.haptic.vibrate.frequency = 1200;
    cfg.haptic.vibrate.strength = 0.25f;
    cfg.haptic.min_strength = 0.4f;
    cfg.haptic.strength_curvature = 0.0f;
    cfg.haptic.max_strength_speed = 10.0f;
    cfg.haptic.duty_solid_offset = 0.1f;
    cfg.haptic.current_threshold = 0.0f;
    haptic_feedback_configure(&hf, &cfg);
    CHECK_FLOAT_NEAR(hf.duty_solid_threshold, 0.6f);
    REQUIRE(hf.str_poly_b > 0.0f);

    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_DUTY};
    MotorData md = {};
    md.speed = 5.0f;
    md.duty_cycle.value = 0.7f;
    MotorControl mc;
    motor_control_init(&mc);
    motor_control_configure(&mc, &cfg, 1000u);
    AlertTracker at = {};
    Time time = {.now = 1000u};

    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    REQUIRE(hf.is_playing);
    REQUIRE(vesc_if_fake_foc_play_tone_calls() == 1);
    REQUIRE(vesc_if_fake_last_foc_channel() == 0);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 440.0f);
    REQUIRE(vesc_if_fake_last_foc_voltage() > cfg.haptic.min_strength * cfg.haptic.duty.strength);
    REQUIRE(mc.tone_ticks > 0);
    CHECK_FLOAT_NEAR(mc.tone_intensity, cfg.haptic.vibrate.strength * 0.7f);

    state.sat = SAT_NONE;
    at.fatal_error = true;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_FATAL);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), 880.0f);

    at.fatal_error = false;
    state.sat = SAT_PB_TEMPERATURE;
    hf.can_change_type = true;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_TEMPERATURE);

    state.sat = SAT_PB_LOW_VOLTAGE;
    hf.can_change_type = true;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);

    state.sat = SAT_NONE;
    cfg.haptic.current_threshold = 0.5f;
    haptic_feedback_configure(&hf, &cfg);
    hf.can_change_type = true;
    md.motor_current_saturation = 0.75f;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);

    state.sat = SAT_PB_DUTY;
    md.duty_cycle.value = 0.55f;
    hf.can_change_type = true;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_SPEED);

    time.now = hf.tone_timer + 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(!hf.is_playing);
    REQUIRE(mc.tone_ticks == 0);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 0.0f);

    state.mode = MODE_HANDTEST;
    time.now += 150000u;
    hf.can_change_type = true;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_NONE);
}

TEST_CASE("haptic feedback gating and strength edges", "[c]") {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    HapticFeedback hf;
    haptic_feedback_init(&hf);
    RefloatConfig cfg = haptic_test_config();
    haptic_feedback_configure(&hf, &cfg);

    MotorControl mc;
    motor_control_init(&mc);
    motor_control_configure(&mc, &cfg, 1000u);
    State state = {.state = STATE_READY, .mode = MODE_NORMAL, .sat = SAT_PB_DUTY};
    MotorData md = {};
    md.speed = 5.0f;
    md.duty_cycle.value = 0.8f;
    AlertTracker at = {.fatal_error = true};
    Time time = {.now = 1000u};

    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_NONE);
    REQUIRE(!hf.is_playing);
    REQUIRE(vesc_if_fake_foc_play_tone_calls() == 0);
    REQUIRE(mc.tone_ticks == 0);

    state.state = STATE_RUNNING;
    state.mode = MODE_HANDTEST;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_NONE);
    REQUIRE(vesc_if_fake_foc_play_tone_calls() == 0);

    state.mode = MODE_NORMAL;
    at.fatal_error = false;
    cfg.haptic.duty.strength = 0.0f;
    cfg.haptic.vibrate.strength = 0.0f;
    haptic_feedback_configure(&hf, &cfg);
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    REQUIRE(hf.is_playing);
    REQUIRE(vesc_if_fake_foc_play_tone_calls() == 0);
    REQUIRE(mc.tone_ticks == 0);

    haptic_feedback_init(&hf);
    cfg = haptic_test_config();
    cfg.haptic.max_strength_speed = 0.0f;
    haptic_feedback_configure(&hf, &cfg);
    motor_control_configure(&mc, &cfg, 1000u);
    md.speed = 0.5f;
    time.now += 1000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), cfg.haptic.duty.strength * 0.7f);
    CHECK_FLOAT_NEAR(mc.tone_intensity, cfg.haptic.vibrate.strength * 0.7f);
}

TEST_CASE("haptic feedback shared strength scale", "[c]") {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    HapticFeedback hf;
    haptic_feedback_init(&hf);
    RefloatConfig cfg = haptic_test_config();
    cfg.haptic.min_strength = 0.25f;
    cfg.haptic.strength_curvature = 0.0f;
    cfg.haptic.max_strength_speed = 20.0f;
    cfg.haptic.duty.strength = 0.8f;
    cfg.haptic.vibrate.strength = 0.3f;
    haptic_feedback_configure(&hf, &cfg);

    MotorControl mc;
    motor_control_init(&mc);
    motor_control_configure(&mc, &cfg, 1000u);

    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_DUTY};
    MotorData md = {.speed = -10.0f};
    md.duty_cycle.value = 0.8f;
    AlertTracker at = {};
    Time time = {.now = 1000u};

    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);

    float expected_scale = 0.625f;
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    REQUIRE(hf.is_playing);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), cfg.haptic.duty.strength * expected_scale);
    CHECK_FLOAT_NEAR(mc.tone_intensity, cfg.haptic.vibrate.strength * expected_scale);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), cfg.haptic.duty.frequency);
    CHECK_U32(mc.tone_ticks, 1u);
}

TEST_CASE("haptic feedback pattern type change lockout", "[c]") {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    HapticFeedback hf;
    haptic_feedback_init(&hf);
    RefloatConfig cfg = haptic_test_config();
    haptic_feedback_configure(&hf, &cfg);

    MotorControl mc;
    motor_control_init(&mc);
    motor_control_configure(&mc, &cfg, 1000u);
    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_LOW_VOLTAGE};
    MotorData md = {.speed = 2.0f};
    AlertTracker at = {};
    Time time = {.now = 2000u};

    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    REQUIRE(hf.is_playing);
    REQUIRE(hf.can_change_type);

    time.now = hf.tone_timer + 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    REQUIRE(!hf.is_playing);
    REQUIRE(!hf.can_change_type);

    at.fatal_error = true;
    state.sat = SAT_NONE;
    time.now += 10000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    REQUIRE(!hf.can_change_type);

    time.now = hf.tone_timer + 801000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.can_change_type);

    time.now += 1000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_FATAL);
}

TEST_CASE("haptic feedback type selection edges", "[c]") {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    HapticFeedback hf;
    haptic_feedback_init(&hf);
    RefloatConfig cfg = haptic_test_config();
    haptic_feedback_configure(&hf, &cfg);

    MotorControl mc;
    motor_control_init(&mc);
    motor_control_configure(&mc, &cfg, 1000u);
    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_SPEED};
    MotorData md = {.speed = -3.0f};
    AlertTracker at = {};
    Time time = {.now = 3000u};

    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_SPEED);
    REQUIRE(hf.is_playing);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), cfg.haptic.duty.frequency);

    state.sat = SAT_PB_HIGH_VOLTAGE;
    hf.can_change_type = true;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), cfg.haptic.error.frequency);

    state.sat = SAT_PB_ERROR;
    hf.can_change_type = true;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_VOLTAGE);

    haptic_feedback_init(&hf);
    haptic_feedback_configure(&hf, &cfg);
    state.sat = SAT_PB_DUTY;
    md.duty_cycle.value = 0.8f;
    fake_vesc_if.foc_play_tone = NULL;
    time.now += 100000u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_DUTY_CONTINUOUS);
    REQUIRE(hf.is_playing);
    REQUIRE(vesc_if_fake_foc_play_tone_calls() == 3);
    REQUIRE(mc.tone_ticks > 0);
}

TEST_CASE("haptic feedback error pattern pause edges", "[c]") {
    feedback_fakes_reset();
    vesc_if_fake_reset();

    HapticFeedback hf;
    haptic_feedback_init(&hf);
    RefloatConfig cfg = haptic_test_config();
    haptic_feedback_configure(&hf, &cfg);

    MotorControl mc;
    motor_control_init(&mc);
    motor_control_configure(&mc, &cfg, 1000u);
    State state = {.state = STATE_RUNNING, .mode = MODE_NORMAL, .sat = SAT_PB_TEMPERATURE};
    MotorData md = {.speed = 0.0f};
    AlertTracker at = {};
    Time time = {.now = 4000u};

    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.type_playing == HAPTIC_FEEDBACK_ERROR_TEMPERATURE);
    REQUIRE(hf.is_playing);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), cfg.haptic.error.frequency);

    time.now = hf.tone_timer + 1100u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(!hf.is_playing);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_voltage(), 0.0f);

    time.now = hf.tone_timer + 2100u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.is_playing);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_foc_frequency(), cfg.haptic.error.frequency);

    time.now = hf.tone_timer + 3100u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(!hf.is_playing);
    size_t calls_before_skipped_beat = vesc_if_fake_foc_play_tone_calls();

    time.now = hf.tone_timer + 4100u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(!hf.is_playing);
    REQUIRE(vesc_if_fake_foc_play_tone_calls() == calls_before_skipped_beat);

    time.now = hf.tone_timer + 6100u;
    haptic_feedback_update(&hf, &mc, &state, &md, &at, &time);
    REQUIRE(hf.is_playing);
    REQUIRE(hf.can_change_type);
}

}  // namespace
