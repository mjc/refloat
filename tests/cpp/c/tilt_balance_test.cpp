#include "../c_support.hpp"

namespace {

static RefloatConfig default_booster_cfg(void) {
    RefloatConfig value_1{};
    value_1.booster_current = 10.0f;
    value_1.booster_angle = 2.0f;
    value_1.booster_ramp = 2.0f;
    value_1.brkbooster_current = 5.0f;
    value_1.brkbooster_angle = 2.0f;
    value_1.brkbooster_ramp = 2.0f;
    return value_1;
}

static RefloatConfig default_brake_tilt_cfg(void) {
    RefloatConfig value_2{};
    value_2.braketilt_strength = 10.0f;
    value_2.braketilt_lingering = 1.0f;
    value_2.atr.filter.time_constant = 0.1f;
    value_2.atr.filter.on_speed_time_constant = 0.1f;
    value_2.atr.filter.off_speed_time_constant = 0.1f;
    value_2.atr.filter.on_speed_limit = 100.0f;
    value_2.atr.filter.off_speed_limit = 50.0f;
    return value_2;
}

static RefloatConfig default_atr_edge_cfg(void) {
    RefloatConfig value_3{};
    value_3.atr.filter.time_constant = 0.05f;
    value_3.atr.filter.on_speed_time_constant = 0.05f;
    value_3.atr.filter.off_speed_time_constant = 0.05f;
    value_3.atr.filter.on_speed_limit = 100.0f;
    value_3.atr.filter.off_speed_limit = 100.0f;
    value_3.atr.transition_boost = 1.8f;
    value_3.atr_strength_up = 0.25f;
    value_3.atr_strength_down = 0.25f;
    value_3.atr_threshold_up = 5.0f;
    value_3.atr_threshold_down = 5.0f;
    value_3.atr_speed_boost = -0.5f;
    value_3.atr_angle_limit = 2.0f;
    value_3.atr_amps_accel_ratio = 1.0f;
    value_3.atr_amps_decel_ratio = 1.0f;
    return value_3;
}

static RefloatConfig default_torque_tilt_cfg(void) {
    RefloatConfig value_4{};
    value_4.torquetilt_strength = 3.0f;
    value_4.torquetilt_strength_regen = 2.0f;
    value_4.torquetilt_start_current = 2.0f;
    value_4.torquetilt_angle_limit = 5.0f;
    value_4.torque_tilt.filter.time_constant = 0.2f;
    value_4.torque_tilt.filter.on_speed_time_constant = 0.1f;
    value_4.torque_tilt.filter.off_speed_time_constant = 0.1f;
    value_4.torque_tilt.filter.on_speed_limit = 100.0f;
    value_4.torque_tilt.filter.off_speed_limit = 100.0f;
    return value_4;
}

static BalanceFilterData level_balance_filter(void) {
    BalanceFilterData value_5{};
    value_5.q0 = 1.0f;
    value_5.acc_mag = 1.0f;
    value_5.kp_pitch = 1.0f;
    value_5.kp_roll = 1.0f;
    value_5.kp_yaw = 1.0f;
    return value_5;
}

TEST_CASE("booster and brake tilt branch cases", "[c]") {
    Booster booster;
    booster_init(&booster);
    booster_configure(&booster, 100.0f);

    RefloatConfig cfg = default_booster_cfg();

    MotorData md = {.abs_erpm = 0.0f, .braking = false};
    booster_update(&booster, &md, &cfg, 2.5f);
    float ramped_accel = booster.torque.value;
    REQUIRE(ramped_accel > 0.0f);
    REQUIRE(ramped_accel < cfg.booster_current * TORQUE_CONSTANT_COMPAT);

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, 1.25f);
    REQUIRE(booster.torque.value > 0.0f);

    booster_reset(&booster);
    md.braking = true;
    md.abs_erpm = 0.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    float low_speed_brake = booster.torque.value;

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    REQUIRE(booster.torque.value < low_speed_brake);

    BrakeTilt bt;
    brake_tilt_init(&bt);
    cfg.braketilt_strength = 0.0f;
    cfg.braketilt_lingering = 1.0f;
    cfg.atr.filter.time_constant = 0.1f;
    cfg.atr.filter.on_speed_time_constant = 0.1f;
    cfg.atr.filter.off_speed_time_constant = 0.1f;
    cfg.atr.filter.on_speed_limit = 100.0f;
    cfg.atr.filter.off_speed_limit = 50.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);
    CHECK_FLOAT_NEAR(bt.factor, 0.0f);

    ATR atr = {};
    md = {};
    md.braking = true;
    md.abs_erpm = 3000.0f;
    md.erpm = 3000.0f;
    md.erpm_sign = 1;
    md.forward = true;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, 0.0f);

    cfg.braketilt_strength = 10.0f;
    cfg.braketilt_lingering = 2.0f;
    brake_tilt_configure(&bt, &cfg, 100.0f);
    REQUIRE(bt.factor < 0.0f);
    CHECK_FLOAT_NEAR(bt.setpoint.off_speed_up, cfg.atr.filter.off_speed_limit / 2.0f);

    atr.accel_diff = 0.0f;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    REQUIRE(bt.target > 0.0f);
    float flat_target = bt.target;

    atr.accel_diff = -2.0f;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    REQUIRE(bt.target > 0.0f);
    REQUIRE(bt.target < flat_target);

    atr.accel_diff = -3.0f;
    brake_tilt_update(&bt, &md, &atr, false, -6.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, 0.0f);

    brake_tilt_update(&bt, &md, &atr, false, 6.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, 0.0f);

    bt.setpoint.value = 4.0f;
    brake_tilt_update(&bt, &md, &atr, true, -6.0f, 0.1f);
    REQUIRE(bt.setpoint.is_winddown);
    REQUIRE(bt.setpoint.value < 4.0f);

    brake_tilt_reset(&bt);
    CHECK_FLOAT_NEAR(bt.target, 0.0f);
    CHECK_FLOAT_NEAR(bt.setpoint.value, 0.0f);
}

TEST_CASE("brake tilt negative erpm downhill boundaries", "[c]") {
    BrakeTilt bt;
    brake_tilt_init(&bt);

    RefloatConfig cfg = default_brake_tilt_cfg();
    brake_tilt_configure(&bt, &cfg, 100.0f);

    MotorData md{};
    md.braking = true;
    md.abs_erpm = 2000.0f;
    md.erpm = -2000.0f;
    md.erpm_sign = -1;
    md.forward = false;
    ATR atr = {.accel_diff = 2.0f};

    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, 0.0f);

    md.abs_erpm = 2001.0f;
    md.erpm = -1000.0f;
    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, -2.0f);

    md.erpm = -1001.0f;
    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, -1.0f);

    atr.accel_diff = 3.0f;
    brake_tilt_update(&bt, &md, &atr, false, 5.0f, 0.1f);
    CHECK_FLOAT_NEAR(bt.target, 0.0f);
}

TEST_CASE("booster threshold boundary edges", "[c]") {
    Booster booster;
    booster_init(&booster);
    booster.torque.alpha = 1.0f;

    RefloatConfig cfg = default_booster_cfg();
    cfg.booster_current = 8.0f;
    cfg.booster_angle = 2.0f;
    cfg.booster_ramp = 2.0f;
    cfg.brkbooster_current = 4.0f;
    cfg.brkbooster_angle = 3.0f;
    cfg.brkbooster_ramp = 2.0f;

    MotorData md = {.abs_erpm = 3000.0f, .braking = false};
    booster_update(&booster, &md, &cfg, 2.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, 4.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, cfg.booster_current * TORQUE_CONSTANT_COMPAT);

    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, 1.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, -1.5f);
    CHECK_FLOAT_NEAR(booster.torque.value, -2.0f * TORQUE_CONSTANT_COMPAT);

    md.braking = true;
    booster_update(&booster, &md, &cfg, -5.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, -8.0f * TORQUE_CONSTANT_COMPAT);

    md.abs_erpm = 23000.0f;
    booster_update(&booster, &md, &cfg, 5.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, 8.0f * TORQUE_CONSTANT_COMPAT);
}

TEST_CASE("booster threshold ramp and reset edges", "[c]") {
    Booster booster;
    booster_init(&booster);
    booster_configure(&booster, 1000.0f);

    RefloatConfig cfg = default_booster_cfg();
    cfg.booster_current = 8.0f;
    cfg.booster_angle = 3.0f;
    cfg.booster_ramp = 2.0f;
    cfg.brkbooster_current = 6.0f;
    cfg.brkbooster_angle = 4.0f;
    cfg.brkbooster_ramp = 2.0f;

    MotorData md = {.abs_erpm = 0.0f, .braking = false};
    booster_update(&booster, &md, &cfg, 3.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, -4.0f);
    REQUIRE(booster.torque.value < 0.0f);
    REQUIRE(booster.torque.value > -cfg.booster_current * TORQUE_CONSTANT_COMPAT);

    booster_reset(&booster);
    CHECK_FLOAT_NEAR(booster.torque.value, 0.0f);

    booster_update(&booster, &md, &cfg, 6.0f);
    float low_speed_accel = booster.torque.value;
    REQUIRE(low_speed_accel > 0.0f);

    booster_reset(&booster);
    md.abs_erpm = 3000.0f;
    booster_update(&booster, &md, &cfg, 6.0f);
    CHECK_FLOAT_NEAR(booster.torque.value, low_speed_accel);

    booster_reset(&booster);
    md.abs_erpm = 3000.0f;
    booster_update(&booster, &md, &cfg, 4.0f);
    float threshold_accel = booster.torque.value;

    booster_reset(&booster);
    md.abs_erpm = 3001.0f;
    booster_update(&booster, &md, &cfg, 4.0f);
    REQUIRE(booster.torque.value > threshold_accel);

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, 2.0f);
    REQUIRE(booster.torque.value > 0.0f);
    REQUIRE(booster.torque.value < low_speed_accel);

    booster_reset(&booster);
    md.braking = true;
    md.abs_erpm = 3000.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    float threshold_brake = booster.torque.value;

    booster_reset(&booster);
    md.abs_erpm = 3001.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    REQUIRE(booster.torque.value < threshold_brake);

    booster_reset(&booster);
    md.abs_erpm = 13000.0f;
    booster_update(&booster, &md, &cfg, -5.0f);
    REQUIRE(booster.torque.value < threshold_brake);
}

TEST_CASE("atr branch cases", "[c]") {
    ATR atr;
    atr_init(&atr);
    CHECK_FLOAT_NEAR(atr.accel_diff, 0.0f);
    CHECK_FLOAT_NEAR(atr.speed_boost, 0.0f);
    CHECK_FLOAT_NEAR(atr.transition_boost, 1.0f);

    RefloatConfig cfg{};
    cfg.atr.filter.time_constant = 0.1f;
    cfg.atr.filter.on_speed_time_constant = 0.1f;
    cfg.atr.filter.off_speed_time_constant = 0.1f;
    cfg.atr.filter.on_speed_limit = 100.0f;
    cfg.atr.filter.off_speed_limit = 100.0f;
    cfg.atr.transition_boost = 2.0f;
    cfg.atr_strength_up = 1.0f;
    cfg.atr_strength_down = 0.5f;
    cfg.atr_threshold_up = 0.1f;
    cfg.atr_threshold_down = 0.2f;
    cfg.atr_speed_boost = 0.6f;
    cfg.atr_angle_limit = 4.0f;
    cfg.atr_amps_accel_ratio = 1.0f;
    cfg.atr_amps_decel_ratio = 1.5f;
    atr_configure(&atr, &cfg, 100.0f);
    REQUIRE(atr.speed_boost_mult < 1.0f / 3000.0f);
    REQUIRE(atr.ad_alpha1 > 0.0f);
    REQUIRE(atr.ad_alpha2 > 0.0f);
    REQUIRE(atr.ad_alpha3 > 0.0f);

    MotorData md{};
    md.torque = 20.0f;
    md.erpm_sign = 1;
    md.abs_erpm = 100.0f;
    md.forward = true;
    md.braking = false;
    md.acceleration.value = 0.0f;
    atr.accel_diff = 3.0f;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(atr.accel_diff, 0.0f);
    CHECK_FLOAT_NEAR(atr.speed_boost, 0.0f);

    md.abs_erpm = 5000.0f;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    REQUIRE(atr.accel_diff > 0.0f);
    REQUIRE(atr.speed_boost > 0.0f);
    REQUIRE(atr.target > 0.0f);
    REQUIRE(atr.target <= cfg.atr_angle_limit);

    md.braking = true;
    md.torque = -20.0f;
    md.erpm_sign = -1;
    md.forward = false;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(atr.speed_boost, 0.0f);
    REQUIRE(fabsf(atr.target) <= cfg.atr_angle_limit);

    atr.setpoint.value = 3.0f;
    ema_reset(&atr.transition_target, -3.0f);
    md.braking = false;
    md.torque = -25.0f;
    md.erpm_sign = 1;
    md.forward = true;
    md.abs_erpm = 5000.0f;
    atr_update(&atr, &md, &cfg, false, 0.1f);
    REQUIRE(atr.transition_boost >= 1.0f);
    REQUIRE(atr.transition_boost <= cfg.atr.transition_boost);

    atr.setpoint.value = 2.0f;
    atr_update(&atr, &md, &cfg, true, 0.1f);
    REQUIRE(atr.setpoint.is_winddown);
    REQUIRE(atr.setpoint.value < 2.0f);
    CHECK_FLOAT_NEAR(atr.transition_target.value, atr.setpoint.value);

    atr_reset(&atr);
    CHECK_FLOAT_NEAR(atr.accel_diff, 0.0f);
    CHECK_FLOAT_NEAR(atr.speed_boost, 0.0f);
    CHECK_FLOAT_NEAR(atr.target, 0.0f);
    CHECK_FLOAT_NEAR(atr.transition_boost, 1.0f);
    CHECK_FLOAT_NEAR(atr.setpoint.value, 0.0f);
}

TEST_CASE("atr threshold speedboost and reset edges", "[c]") {
    ATR atr;
    atr_init(&atr);

    RefloatConfig cfg = default_atr_edge_cfg();
    atr_configure(&atr, &cfg, 200.0f);
    CHECK_FLOAT_NEAR(atr.speed_boost_mult, 1.0f / 3000.0f);

    MotorData md{};
    md.torque = 9.0f * TORQUE_CONSTANT_COMPAT;
    md.erpm_sign = 1;
    md.abs_erpm = 2000.0f;
    md.forward = true;
    md.braking = false;
    md.acceleration.value = 0.0f;

    md.abs_erpm = 250.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    CHECK_FLOAT_NEAR(atr.accel_diff, 0.0f);

    atr_reset(&atr);
    md.abs_erpm = 251.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    CHECK_FLOAT_NEAR(atr.accel_diff, atr.ad_alpha3);

    atr_reset(&atr);
    md.abs_erpm = 1001.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    CHECK_FLOAT_NEAR(atr.accel_diff, atr.ad_alpha2);

    atr_reset(&atr);
    md.abs_erpm = 2001.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    CHECK_FLOAT_NEAR(atr.accel_diff, atr.ad_alpha1);

    atr_reset(&atr);
    md.abs_erpm = 2000.0f;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    CHECK_FLOAT_NEAR(atr.speed_boost, 0.0f);
    CHECK_FLOAT_NEAR(atr.target, 0.0f);

    cfg.atr_threshold_up = 0.0f;
    md.abs_erpm = 9000.0f;
    md.torque = 30.0f * TORQUE_CONSTANT_COMPAT;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    REQUIRE(atr.speed_boost < 0.0f);
    REQUIRE(atr.target >= -cfg.atr_angle_limit);
    REQUIRE(atr.target <= cfg.atr_angle_limit);

    atr.accel_diff = 1.2f;
    atr.speed_boost = -0.2f;
    atr.target = 1.5f;
    atr.transition_boost = cfg.atr.transition_boost;
    atr.setpoint.value = 1.0f;
    ema_reset(&atr.transition_target, 1.0f);
    atr_reset(&atr);
    CHECK_FLOAT_NEAR(atr.accel_diff, 0.0f);
    CHECK_FLOAT_NEAR(atr.speed_boost, 0.0f);
    CHECK_FLOAT_NEAR(atr.target, 0.0f);
    CHECK_FLOAT_NEAR(atr.transition_boost, 1.0f);
    CHECK_FLOAT_NEAR(atr.transition_target.value, 0.0f);
    CHECK_FLOAT_NEAR(atr.setpoint.value, 0.0f);
}

TEST_CASE("atr zero accel ratio config", "[c][red]") {
    ATR atr;
    atr_init(&atr);

    RefloatConfig cfg = default_atr_edge_cfg();
    cfg.atr.transition_boost = 1.5f;
    cfg.atr_strength_up = 1.0f;
    cfg.atr_strength_down = 1.0f;
    cfg.atr_threshold_up = 0.0f;
    cfg.atr_threshold_down = 0.0f;
    cfg.atr_speed_boost = 0.0f;
    cfg.atr_angle_limit = 10.0f;
    cfg.atr_amps_accel_ratio = 0.0f;
    cfg.atr_amps_decel_ratio = 0.0f;
    atr_configure(&atr, &cfg, 100.0f);

    MotorData md{};
    md.torque = 20.0f;
    md.erpm_sign = 1;
    md.abs_erpm = 4000.0f;
    md.forward = true;
    md.braking = false;
    md.acceleration.value = 0.0f;

    atr_update(&atr, &md, &cfg, false, 0.01f);
    REQUIRE(isfinite(atr.accel_diff));
    REQUIRE(isfinite(atr.target));
    REQUIRE(isfinite(atr.setpoint.value));

    md.braking = true;
    md.torque = -20.0f;
    md.erpm_sign = -1;
    md.forward = false;
    atr_update(&atr, &md, &cfg, false, 0.01f);
    REQUIRE(isfinite(atr.accel_diff));
    REQUIRE(isfinite(atr.target));
    REQUIRE(isfinite(atr.setpoint.value));
}

TEST_CASE("balance filter nonfinite dt", "[c][red]") {
    BalanceFilterData bf = level_balance_filter();

    float gyro[3] = {0.5f, -0.25f, 0.125f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    balance_filter_update(&bf, gyro, accel, NAN);

    REQUIRE(isfinite(bf.q0));
    REQUIRE(isfinite(bf.q1));
    REQUIRE(isfinite(bf.q2));
    REQUIRE(isfinite(bf.q3));
    REQUIRE(isfinite(balance_filter_get_roll(&bf)));
    REQUIRE(isfinite(balance_filter_get_pitch(&bf)));
    REQUIRE(isfinite(balance_filter_get_yaw(&bf)));

    float norm = sqrtf(bf.q0 * bf.q0 + bf.q1 * bf.q1 + bf.q2 * bf.q2 + bf.q3 * bf.q3);
    REQUIRE(fabsf(norm - 1.0f) < 0.00001f);
}

TEST_CASE("torque tilt", "[c]") {
    TorqueTilt tt;
    torque_tilt_init(&tt);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    RefloatConfig cfg = default_torque_tilt_cfg();
    torque_tilt_configure(&tt, &cfg, 100.0f);

    MotorData md = {};
    md.forward = true;
    md.braking = false;
    md.torque = 1.5f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.value, 0.0f);

    md.torque = 4.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, cfg.torquetilt_angle_limit);
    REQUIRE(tt.setpoint.value > 0.0f);

    md.braking = true;
    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, -2.0f);

    tt.setpoint.value = 4.0f;
    torque_tilt_update(&tt, &md, &cfg, true, 0.1f);
    REQUIRE(tt.setpoint.is_winddown);
    REQUIRE(tt.setpoint.value < 4.0f);

    torque_tilt_reset(&tt);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.value, 0.0f);
    REQUIRE(!tt.setpoint.is_winddown);
}

TEST_CASE("torque tilt sign strength and filter edges", "[c]") {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_strength = 1.5f;
    cfg.torquetilt_strength_regen = 0.0f;
    cfg.torquetilt_start_current = 1.0f;
    cfg.torquetilt_angle_limit = 10.0f;
    cfg.torque_tilt.filter.time_constant = 0.25f;
    cfg.torque_tilt.filter.on_speed_time_constant = 0.15f;
    cfg.torque_tilt.filter.off_speed_time_constant = 0.2f;
    cfg.torque_tilt.filter.on_speed_limit = 7.0f;
    cfg.torque_tilt.filter.off_speed_limit = 3.0f;
    torque_tilt_configure(&tt, &cfg, 100.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.on_speed_up, 7.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.off_speed_up, 3.0f);

    MotorData md = {};
    md.forward = false;
    md.braking = false;
    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, -3.0f);
    REQUIRE(tt.setpoint.value < 0.0f);

    md.forward = true;
    md.torque = cfg.torquetilt_start_current * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    md.torque = (cfg.torquetilt_start_current + 0.5f) * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.75f);

    md.braking = true;
    md.torque = -4.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    cfg.torquetilt_strength_regen = 3.0f;
    cfg.torquetilt_start_current = 5.0f;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    md.torque = 20.0f * TORQUE_CONSTANT_COMPAT;
    md.braking = false;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, cfg.torquetilt_angle_limit);

    tt.target = 4.0f;
    tt.setpoint.value = 3.0f;
    torque_tilt_reset(&tt);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.value, 0.0f);
}

TEST_CASE("torque tilt negative limit and regen edges", "[c]") {
    TorqueTilt tt;
    torque_tilt_init(&tt);

    RefloatConfig cfg = default_torque_tilt_cfg();
    cfg.torquetilt_strength = 2.0f;
    cfg.torquetilt_strength_regen = 1.5f;
    cfg.torquetilt_start_current = 1.0f;
    cfg.torquetilt_angle_limit = 4.0f;
    cfg.torque_tilt.filter.time_constant = 0.1f;
    cfg.torque_tilt.filter.on_speed_time_constant = 0.1f;
    cfg.torque_tilt.filter.off_speed_time_constant = 0.1f;
    cfg.torque_tilt.filter.on_speed_limit = 100.0f;
    cfg.torque_tilt.filter.off_speed_limit = 100.0f;
    torque_tilt_configure(&tt, &cfg, 100.0f);

    MotorData md{};
    md.forward = false;
    md.braking = false;
    md.torque = -20.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, -cfg.torquetilt_angle_limit);
    REQUIRE(tt.setpoint.value < 0.0f);

    md.forward = true;
    md.braking = true;
    md.torque = 3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 3.0f);

    md.torque = -3.0f * TORQUE_CONSTANT_COMPAT;
    torque_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, -3.0f);
}

}  // namespace
