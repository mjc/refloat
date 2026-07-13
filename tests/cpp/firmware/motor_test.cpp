#include "firmware_support.hpp"

namespace {

TEST_CASE("motor control current brake and tone", "[c]") {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);
    REQUIRE(isnan(mc.requested_current));
    REQUIRE(!mc.disabled);

    RefloatConfig cfg{};
    cfg.brake_current = 7.5f;
    cfg.startup_click_current = 2.0f;
    cfg.parking_brake_mode = PARKING_BRAKE_IDLE;
    motor_control_configure(&mc, &cfg, 1000u);
    CHECK_FLOAT_NEAR(mc.brake_current, 7.5f);
    CHECK_FLOAT_NEAR(mc.click_current, 2.0f);
    REQUIRE(mc.main_freq == 500u);

    Time time = {.now = 1000000u};
    motor_control_apply(&mc, 0.0f, STATE_DISABLED, &time);
    REQUIRE(mc.disabled);
    REQUIRE(vesc_if_fake_mc_set_current_calls() == 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    REQUIRE(vesc_if_fake_timeout_reset_calls() == 0);

    motor_control_apply(&mc, 0.0f, STATE_DISABLED, &time);
    REQUIRE(vesc_if_fake_mc_set_current_calls() == 1);

    motor_control_request_current(&mc, 4.25f);
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    REQUIRE(!mc.disabled);
    REQUIRE(isnan(mc.requested_current));
    REQUIRE(vesc_if_fake_timeout_reset_calls() == 1);
    REQUIRE(vesc_if_fake_mc_set_current_off_delay_calls() == 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current_off_delay(), 0.05f);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), 4.25f);

    motor_control_apply(&mc, 3000.0f, STATE_RUNNING, &time);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_brake_current(), 7.5f);

    time.now += 2000000u;
    motor_control_apply(&mc, 0.0f, STATE_RUNNING, &time);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);

    mc.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    timer_refresh(&time, &mc.brake_timer);
    motor_control_apply(&mc, 0.0f, STATE_RUNNING, &time);
    REQUIRE(mc.parking_brake_active);
    size_t duty_calls_before = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&mc, 0.0f, STATE_STARTUP, &time);
    REQUIRE(mc.parking_brake_active);
    REQUIRE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before + 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

    motor_control_play_tone(&mc, 250u, 1.5f);
    REQUIRE(mc.tone_ticks == 2u);
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), -1.5f);
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), 1.5f);

    motor_control_stop_tone(&mc);
    REQUIRE(mc.tone_ticks == 0u);
    REQUIRE(mc.tone_counter == 0u);

    motor_control_play_click(&mc);
    REQUIRE(mc.click_counter == 3u);
    REQUIRE(mc.tone_ticks > 0u);
}

TEST_CASE("motor control parking and tone edges", "[c]") {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg{};
    cfg.brake_current = 6.0f;
    cfg.startup_click_current = 0.0f;
    cfg.parking_brake_mode = PARKING_BRAKE_NEVER;
    motor_control_configure(&mc, &cfg, 1000u);

    Time time = {.now = 1000000u};
    timer_refresh(&time, &mc.brake_timer);
    motor_control_apply(&mc, 0.0f, STATE_READY, &time);
    REQUIRE(!mc.parking_brake_active);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_brake_current(), cfg.brake_current);

    cfg.parking_brake_mode = PARKING_BRAKE_IDLE;
    motor_control_configure(&mc, &cfg, 1000u);
    motor_control_apply(&mc, 100.0f, STATE_READY, &time);
    REQUIRE(!mc.parking_brake_active);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 2);

    motor_control_apply(&mc, 25.0f, STATE_READY, &time);
    REQUIRE(mc.parking_brake_active);
    REQUIRE(vesc_if_fake_mc_set_duty_calls() == 1);

    motor_control_apply(&mc, 25.0f, STATE_RUNNING, &time);
    REQUIRE(!mc.parking_brake_active);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 3);

    mc.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    timer_expire(&time, &mc.brake_timer, 1.1f);
    size_t current_calls_before_release = vesc_if_fake_mc_set_current_calls();
    size_t duty_calls_before_release = vesc_if_fake_mc_set_duty_calls();
    motor_control_apply(&mc, 0.0f, STATE_READY, &time);
    REQUIRE(vesc_if_fake_mc_set_current_calls() == current_calls_before_release + 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), 0.0f);
    REQUIRE(vesc_if_fake_mc_set_duty_calls() == duty_calls_before_release);

    motor_control_play_click(&mc);
    CHECK_U32(mc.click_counter, 0u);
    CHECK_U32(mc.tone_ticks, 0u);

    motor_control_play_tone(&mc, 250u, 1.0f);
    CHECK_U32(mc.tone_ticks, 2u);
    CHECK_U32(mc.tone_counter, 2u);
    motor_control_apply(&mc, 10.0f, STATE_RUNNING, &time);
    CHECK_U32(mc.tone_counter, 1u);
    motor_control_play_tone(&mc, 250u, 2.0f);
    CHECK_U32(mc.tone_counter, 1u);
    CHECK_FLOAT_NEAR(mc.tone_intensity, 2.0f);
    motor_control_play_tone(&mc, 500u, 3.0f);
    CHECK_U32(mc.tone_ticks, 1u);
    CHECK_U32(mc.tone_counter, 1u);
    CHECK_FLOAT_NEAR(mc.tone_intensity, 3.0f);
}

static sigjmp_buf motor_control_zero_tone_frequency_sigfpe_env;

enum {
    TEST_SIGFPE = 8
};

static void catch_motor_control_zero_tone_frequency_sigfpe(int signal_number) {
    unused(signal_number);
    siglongjmp(motor_control_zero_tone_frequency_sigfpe_env, 1);
}

TEST_CASE("motor control zero tone frequency", "[c][red]") {
    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg{};
    cfg.brake_current = 6.0f;
    cfg.startup_click_current = 0.0f;
    cfg.parking_brake_mode = PARKING_BRAKE_NEVER;
    motor_control_configure(&mc, &cfg, 1000u);

    SignalHandler previous_handler =
        signal(TEST_SIGFPE, catch_motor_control_zero_tone_frequency_sigfpe);
    if (sigsetjmp(motor_control_zero_tone_frequency_sigfpe_env, 1) != 0) {
        signal(TEST_SIGFPE, previous_handler);
        FAIL("unexpected signal while exercising C code");
    }

    motor_control_play_tone(&mc, 0u, 1.0f);
    signal(TEST_SIGFPE, previous_handler);

    CHECK_U32(mc.tone_ticks, 0u);
    CHECK_U32(mc.tone_counter, 0u);
    CHECK_FLOAT_NEAR(mc.tone_intensity, 0.0f);
}

TEST_CASE("motor control click lifecycle edges", "[c]") {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg{};
    cfg.brake_current = 5.0f;
    cfg.startup_click_current = 2.0f;
    cfg.parking_brake_mode = PARKING_BRAKE_NEVER;
    motor_control_configure(&mc, &cfg, 1000u);

    Time time = {.now = 1000000u};
    motor_control_play_click(&mc);
    CHECK_U32(mc.click_counter, 3u);
    CHECK_U32(mc.tone_ticks, 1u);
    CHECK_U32(mc.tone_counter, 1u);

    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    CHECK_U32(mc.click_counter, 2u);
    CHECK_U32(mc.tone_ticks, 1u);
    REQUIRE(mc.tone_high);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), 2.0f);

    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    CHECK_U32(mc.click_counter, 1u);
    CHECK_U32(mc.tone_ticks, 1u);
    REQUIRE(!mc.tone_high);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), -2.0f);

    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    CHECK_U32(mc.click_counter, 0u);
    CHECK_U32(mc.tone_ticks, 0u);
    CHECK_U32(mc.tone_counter, 0u);
    REQUIRE(!mc.tone_high);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_current(), -2.0f);

    size_t current_calls_after_click = vesc_if_fake_mc_set_current_calls();
    motor_control_apply(&mc, 100.0f, STATE_RUNNING, &time);
    REQUIRE(vesc_if_fake_mc_set_current_calls() == current_calls_after_click);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() > 0);
}

TEST_CASE("motor control parking and moving threshold edges", "[c]") {
    vesc_if_fake_reset();

    MotorControl mc;
    motor_control_init(&mc);

    RefloatConfig cfg{};
    cfg.brake_current = 4.0f;
    cfg.startup_click_current = 0.0f;
    cfg.parking_brake_mode = PARKING_BRAKE_ALWAYS;
    motor_control_configure(&mc, &cfg, 1000u);

    Time time = {.now = 1000000u};
    timer_refresh(&time, &mc.brake_timer);
    motor_control_apply(&mc, 1999.0f, STATE_READY, &time);
    REQUIRE(mc.parking_brake_active);
    REQUIRE(vesc_if_fake_mc_set_duty_calls() == 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_duty(), 0.0f);

    motor_control_apply(&mc, 2000.0f, STATE_READY, &time);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 1);
    CHECK_FLOAT_NEAR(vesc_if_fake_last_brake_current(), cfg.brake_current);

    mc.parking_brake_mode = PARKING_BRAKE_NEVER;
    mc.brake_timer = time.now - 1u;
    motor_control_apply(&mc, ERPM_MOVING_THRESHOLD, STATE_READY, &time);
    CHECK_U32(mc.brake_timer, time.now - 1u);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 2);

    motor_control_apply(&mc, ERPM_MOVING_THRESHOLD + 0.1f, STATE_READY, &time);
    CHECK_U32(mc.brake_timer, time.now);
    REQUIRE(vesc_if_fake_mc_set_brake_current_calls() == 3);
}

TEST_CASE("motor data refresh update and alerts", "[c]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    CHECK_FLOAT_NEAR(md.speed_constant, 0.0f);

    vesc_if_fake_set_cfg_int(CFG_PARAM_si_battery_cells, 15);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -30.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 45.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -12.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 18.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);

    motor_data_refresh_motor_config(&md, 3.2f, 4.2f);
    CHECK_FLOAT_NEAR(md.lv_threshold, 48.0f);
    CHECK_FLOAT_NEAR(md.hv_threshold, 63.0f);
    CHECK_FLOAT_NEAR(md.current_min, 30.0f);
    CHECK_FLOAT_NEAR(md.current_max, 45.0f);
    CHECK_FLOAT_NEAR(md.battery_current_min, 12.0f);
    CHECK_FLOAT_NEAR(md.battery_current_max, 18.0f);
    CHECK_FLOAT_NEAR(md.mosfet_temp_max, 77.0f);
    CHECK_FLOAT_NEAR(md.motor_temp_max, 87.0f);
    CHECK_FLOAT_NEAR(md.duty_max_with_margin, 0.90f);
    REQUIRE(md.speed_constant > 0.0f);

    motor_data_configure(&md, 0.0f, 100.0f);
    vesc_if_fake_set_motor_telemetry(
        -1200.0f, 5.0f, 12.5f, -10.0f, -8.0f, -0.4f, -6.0f, 50.0f, 61.0f, 72.0f
    );
    motor_data_update(&md, 0.02f);

    CHECK_FLOAT_NEAR(md.erpm, -1200.0f);
    CHECK_FLOAT_NEAR(md.abs_erpm, 1200.0f);
    REQUIRE(md.erpm_sign == -1);
    CHECK_FLOAT_NEAR(md.speed, 18.0f);
    CHECK_FLOAT_NEAR(md.distance, 12.5f);
    REQUIRE(md.braking);
    REQUIRE(md.duty_raw >= 0.0f);
    REQUIRE(md.motor_current_saturation > 0.0f);
    REQUIRE(md.battery_current_saturation > 0.0f);
    CHECK_FLOAT_NEAR(md.batt_voltage, 50.0f);
    CHECK_FLOAT_NEAR(md.mosfet_temp, 61.0f);
    CHECK_FLOAT_NEAR(md.motor_temp, 72.0f);
    CHECK_FLOAT_NEAR(
        motor_data_get_current_saturation(&md),
        fmaxf(md.motor_current_saturation, md.battery_current_saturation)
    );

    float current = motor_data_torque_to_current(&md, 2.0f);
    CHECK_FLOAT_NEAR(current, 2.0f * md.speed_constant);

    AlertTracker at;
    alert_tracker_init(&at);
    Time time = {.now = 1000u};
    vesc_if_fake_set_fault(FAULT_CODE_ABS_OVER_CURRENT);
    motor_data_evaluate_alerts(&md, &at, &time);
    REQUIRE(at.fatal_error);
    REQUIRE(at.fw_fault_code == FAULT_CODE_ABS_OVER_CURRENT);

    motor_data_destroy(&md);
}

TEST_CASE("motor data fallback limits and direction edges", "[c]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 0.5f, 100.0f);

    vesc_if_fake_set_cfg_int(CFG_PARAM_si_battery_cells, 0);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 0.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 50.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 55.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.8f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.0f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 0);

    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    CHECK_FLOAT_NEAR(md.lv_threshold, 42.0f);
    CHECK_FLOAT_NEAR(md.hv_threshold, 60.0f);
    CHECK_FLOAT_NEAR(md.speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);
    CHECK_FLOAT_NEAR(md.mosfet_temp_max, 47.0f);
    CHECK_FLOAT_NEAR(md.motor_temp_max, 52.0f);
    CHECK_FLOAT_NEAR(md.duty_max_with_margin, 0.75f);

    vesc_if_fake_set_motor_telemetry(
        0.0f, 0.0f, 1.0f, 12.0f, 20.0f, 0.25f, 8.0f, 54.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, 0.02f);
    CHECK_FLOAT_NEAR(md.motor_current_saturation, 0.0f);
    CHECK_FLOAT_NEAR(md.battery_current_saturation, 0.0f);
    REQUIRE(md.forward);

    md.filt_current.value = 25.0f;
    md.speed_constant = 1.0f;
    vesc_if_fake_set_motor_telemetry(
        -100.0f, 0.0f, 1.5f, 12.0f, 25.0f, 0.10f, -8.0f, 50.0f, 41.0f, 46.0f
    );
    for (size_t i = 0; i < 8; ++i) {
        motor_data_update(&md, 0.02f);
    }
    REQUIRE(md.torque > 18.0f);
    REQUIRE(md.forward);
    REQUIRE(!md.braking);
    CHECK_FLOAT_NEAR(md.motor_current_saturation, 0.0f);
    CHECK_FLOAT_NEAR(md.battery_current_saturation, 0.0f);

    motor_data_reset(&md);
    CHECK_FLOAT_NEAR(md.duty_cycle.value, 0.0f);
    CHECK_FLOAT_NEAR(md.acceleration.value, 0.0f);
    CHECK_FLOAT_NEAR(md.filt_current.value, 0.0f);

    motor_data_destroy(&md);
}

TEST_CASE("motor data init alert and saturation edges", "[c]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    CHECK_FLOAT_NEAR(md.erpm, 0.0f);
    CHECK_FLOAT_NEAR(md.abs_erpm, 0.0f);
    CHECK_FLOAT_NEAR(md.last_erpm, 0.0f);
    REQUIRE(md.erpm_sign == 1);
    REQUIRE(md.forward);
    REQUIRE(!md.braking);
    CHECK_FLOAT_NEAR(md.motor_current_saturation, 0.0f);
    CHECK_FLOAT_NEAR(md.battery_current_saturation, 0.0f);

    AlertTracker at;
    alert_tracker_init(&at);
    Time time = {.now = 2000u};
    vesc_if_fake_set_fault(FAULT_CODE_NONE);
    motor_data_evaluate_alerts(&md, &at, &time);
    REQUIRE(!at.fatal_error);
    REQUIRE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));

    motor_data_configure(&md, 200.0f, 100.0f);
    md.speed_constant = 1.0f;
    md.current_min = 10.0f;
    md.current_max = 20.0f;
    md.battery_current_min = 5.0f;
    md.battery_current_max = 1000.0f;

    vesc_if_fake_set_motor_telemetry(
        -300.0f, 1.0f, 0.25f, 12.0f, 30.0f, 0.2f, 20.0f, 55.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    REQUIRE(!md.forward);
    REQUIRE(!md.braking);

    md.motor_current_saturation = 0.7f;
    md.battery_current_saturation = 0.2f;
    CHECK_FLOAT_NEAR(motor_data_get_current_saturation(&md), md.motor_current_saturation);

    md.current_min = 1000.0f;
    md.battery_current_min = 1.0f;
    vesc_if_fake_set_motor_telemetry(
        300.0f, 1.0f, 0.50f, -12.0f, -30.0f, 0.2f, -20.0f, 55.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    REQUIRE(md.forward);
    REQUIRE(md.braking);

    md.motor_current_saturation = 0.1f;
    md.battery_current_saturation = 0.8f;
    CHECK_FLOAT_NEAR(motor_data_get_current_saturation(&md), md.battery_current_saturation);

    motor_data_destroy(&md);
}

TEST_CASE("motor data forward direction threshold edges", "[c]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 20.0f, 100.0f);
    md.speed_constant = 1.0f;
    md.current_min = 0.0f;
    md.current_max = 0.0f;
    md.battery_current_min = 0.0f;
    md.battery_current_max = 0.0f;
    md.filt_current.a0 = 1.0f;
    md.filt_current.a1 = 0.0f;
    md.filt_current.a2 = 0.0f;
    md.filt_current.b1 = 0.0f;
    md.filt_current.b2 = 0.0f;
    md.filt_current.z1 = 0.0f;
    md.filt_current.z2 = 0.0f;

    vesc_if_fake_set_motor_telemetry(
        -250.0f, 0.0f, 0.0f, 18.0f, 18.0f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    CHECK_FLOAT_NEAR(md.torque, 18.0f);
    REQUIRE(md.forward);

    vesc_if_fake_set_motor_telemetry(
        -251.0f, 0.0f, 0.0f, 18.0f, 18.0f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    CHECK_FLOAT_NEAR(md.torque, 18.0f);
    REQUIRE(!md.forward);

    vesc_if_fake_set_motor_telemetry(
        -250.0f, 0.0f, 0.0f, 17.99f, 17.99f, 0.0f, 0.0f, 50.0f, 40.0f, 41.0f
    );
    motor_data_update(&md, 0.02f);
    REQUIRE(md.torque < 18.0f);
    REQUIRE(!md.forward);

    motor_data_destroy(&md);
}

TEST_CASE("motor data torque constant config edges", "[c]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);

    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    CHECK_FLOAT_NEAR(md.speed_constant, 1.0f / (1.5f * 0.5f * 14.0f * 0.006f));
    CHECK_FLOAT_NEAR(motor_data_torque_to_current(&md, 3.0f), 3.0f * md.speed_constant);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.012f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 20);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    CHECK_FLOAT_NEAR(md.speed_constant, 1.0f / (1.5f * 0.5f * 20.0f * 0.012f));

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.001f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 20);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    CHECK_FLOAT_NEAR(md.speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);

    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.012f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 0);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);
    CHECK_FLOAT_NEAR(md.speed_constant, 1.0f / TORQUE_CONSTANT_COMPAT);

    motor_data_destroy(&md);
}

TEST_CASE("motor data erpm speed conversion", "[c][red]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 20.0f, 100.0f);

    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_min, -30.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_current_max, 45.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_min, -12.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_in_current_max, 18.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_fet_start, 80.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_temp_motor_start, 90.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_l_max_duty, 0.95f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_foc_motor_flux_linkage, 0.006f);
    vesc_if_fake_set_cfg_int(CFG_PARAM_si_motor_poles, 14);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_gear_ratio, 1.0f);
    vesc_if_fake_set_cfg_float(CFG_PARAM_si_wheel_diameter, 0.28f);
    motor_data_refresh_motor_config(&md, 42.0f, 60.0f);

    vesc_if_fake_set_motor_telemetry(
        1200.0f, 0.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, 0.02f);

    float mechanical_rpm = 1200.0f / (14.0f * 0.5f);
    float expected_kph = mechanical_rpm * (float) M_PI * 0.28f * 60.0f / 1000.0f;
    CHECK_FLOAT_NEAR(md.speed, expected_kph);

    motor_data_destroy(&md);
}

TEST_CASE("motor data nonpositive dt", "[c][red]") {
    vesc_if_fake_reset();

    MotorData md;
    motor_data_init(&md);
    motor_data_configure(&md, 20.0f, 100.0f);
    md.speed_constant = 1.0f;
    md.last_erpm = 1000.0f;

    vesc_if_fake_set_motor_telemetry(
        1500.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );

    motor_data_update(&md, 0.0f);
    REQUIRE(isfinite(md.acceleration.value));

    vesc_if_fake_set_motor_telemetry(
        1250.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, -0.02f);
    REQUIRE(isfinite(md.acceleration.value));

    vesc_if_fake_set_motor_telemetry(
        1750.0f, 1.0f, 1.0f, 4.0f, 4.0f, 0.1f, 2.0f, 50.0f, 40.0f, 45.0f
    );
    motor_data_update(&md, NAN);
    REQUIRE(isfinite(md.acceleration.value));

    motor_data_destroy(&md);
}

}  // namespace
