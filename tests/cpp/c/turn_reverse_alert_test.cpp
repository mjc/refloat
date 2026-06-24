#include "../c_support.hpp"

namespace {

TEST_CASE("turn tilt branch cases", "[c]") {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {};
    cfg.turntilt_strength = 5.0f;
    cfg.turntilt_angle_limit = 8.0f;
    cfg.turntilt_start_angle = 5;
    cfg.turntilt_start_erpm = 1000;
    cfg.turntilt_erpm_boost = 50;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 30;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);
    REQUIRE(tt.boost_per_erpm > 0.0f);

    IMU imu = {.yaw = 179.0f};
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    imu.yaw = -179.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    REQUIRE(tt.yaw_change.value > 0.0f);

    imu.yaw = 120.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    REQUIRE(tt.yaw_change.value < 0.0f);
    CHECK_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);

    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 900.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    md.abs_erpm = 2500.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    REQUIRE(tt.target > 0.0f);
    REQUIRE(tt.target <= cfg.turntilt_angle_limit);

    md.erpm_sign = -1;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    REQUIRE(tt.target < 0.0f);
    REQUIRE(tt.target >= -cfg.turntilt_angle_limit);

    tt.setpoint.value = 4.0f;
    turn_tilt_update(&tt, &md, &cfg, true, 0.1f);
    REQUIRE(tt.setpoint.is_winddown);
    REQUIRE(tt.setpoint.value < 4.0f);

    cfg.turntilt_strength = 0.0f;
    tt.target = 3.0f;
    tt.setpoint.value = 2.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 3.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.value, 2.0f);

    turn_tilt_reset(&tt);
    CHECK_FLOAT_NEAR(tt.last_yaw_angle, 0.0f);
    CHECK_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);
    CHECK_FLOAT_NEAR(tt.setpoint.value, 0.0f);
}

TEST_CASE("turn tilt aggregate and boost edges", "[c]") {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {};
    cfg.turntilt_strength = 5.0f;
    cfg.turntilt_angle_limit = 20.0f;
    cfg.turntilt_start_angle = 2;
    cfg.turntilt_start_erpm = 500;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 10;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);
    CHECK_FLOAT_NEAR(tt.boost_per_erpm, 0.0005f);

    IMU imu = {.yaw = 0.0f};
    turn_tilt_aggregate(&tt, &imu, 1.0f);
    imu.yaw = 10.0f;
    turn_tilt_aggregate(&tt, &imu, 1.0f);
    CHECK_FLOAT_NEAR(tt.yaw_aggregate, 0.0f);

    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 1500.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    float low_speed_target = tt.target;
    REQUIRE(low_speed_target > 0.0f);
    REQUIRE(low_speed_target < cfg.turntilt_angle_limit);

    turn_tilt_reset(&tt);
    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    md.abs_erpm = 3000.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 2.0f);
    REQUIRE(tt.target > low_speed_target);

    tt.last_yaw_angle = -170.0f;
    tt.yaw_change.value = 0.0f;
    tt.yaw_aggregate = -30.0f;
    imu.yaw = 170.0f;
    turn_tilt_aggregate(&tt, &imu, 0.1f);
    REQUIRE(tt.yaw_change.value < 0.0f);
    REQUIRE(tt.yaw_aggregate < 0.0f);
}

TEST_CASE("turn tilt threshold boundary edges", "[c]") {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {};
    cfg.turntilt_strength = 10.0f;
    cfg.turntilt_angle_limit = 50.0f;
    cfg.turntilt_start_angle = 5;
    cfg.turntilt_start_erpm = 1000;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turntilt_yaw_aggregate = 20;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);

    MotorData md = {.abs_erpm = 1000.0f, .erpm_sign = 1, .forward = true};
    tt.yaw_change.value = 29.99f;
    tt.yaw_aggregate = 5.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    tt.yaw_change.value = 30.0f;
    tt.yaw_aggregate = 4.99f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    tt.yaw_aggregate = 5.0f;
    md.abs_erpm = 999.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.0f);

    md.abs_erpm = 1000.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 0.703125f);

    md.abs_erpm = 2000.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 1.0416667f);

    tt.yaw_aggregate = 100.0f;
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    CHECK_FLOAT_NEAR(tt.target, 1.6666667f);
}

TEST_CASE("turn tilt zero denominator config", "[c][red]") {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {};
    cfg.turntilt_strength = 5.0f;
    cfg.turntilt_angle_limit = 20.0f;
    cfg.turntilt_start_angle = 2;
    cfg.turntilt_start_erpm = 500;
    cfg.turntilt_erpm_boost = 100;
    cfg.turntilt_erpm_boost_end = 0;
    cfg.turntilt_yaw_aggregate = 0;
    cfg.turn_tilt.filter.time_constant = 0.1f;

    turn_tilt_configure(&tt, &cfg, 100.0f);
    REQUIRE(isfinite(tt.boost_per_erpm));

    tt.yaw_change.value = 72.0f;
    tt.yaw_aggregate = 45.0f;
    MotorData md = {.abs_erpm = 1500.0f, .erpm_sign = 1, .forward = true};
    turn_tilt_update(&tt, &md, &cfg, false, 0.1f);
    REQUIRE(isfinite(tt.target));
    REQUIRE(isfinite(tt.setpoint.value));
}

TEST_CASE("turn tilt nonpositive dt", "[c][red]") {
    TurnTilt tt;
    turn_tilt_init(&tt);

    RefloatConfig cfg = {};
    cfg.turntilt_erpm_boost_end = 2000;
    cfg.turn_tilt.filter.time_constant = 0.1f;
    turn_tilt_configure(&tt, &cfg, 100.0f);

    tt.last_yaw_angle = 5.0f;
    tt.yaw_change.value = 12.0f;
    tt.yaw_aggregate = 8.0f;

    IMU imu = {.yaw = 35.0f};
    turn_tilt_aggregate(&tt, &imu, 0.0f);
    CHECK_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    CHECK_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    CHECK_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);

    imu.yaw = -20.0f;
    turn_tilt_aggregate(&tt, &imu, -0.02f);
    CHECK_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    CHECK_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    CHECK_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);

    imu.yaw = 90.0f;
    turn_tilt_aggregate(&tt, &imu, NAN);
    CHECK_FLOAT_NEAR(tt.last_yaw_angle, 5.0f);
    REQUIRE(isfinite(tt.yaw_change.value));
    CHECK_FLOAT_NEAR(tt.yaw_change.value, 12.0f);
    CHECK_FLOAT_NEAR(tt.yaw_aggregate, 8.0f);
}

TEST_CASE("reverse stop update paths", "[c]") {
    ReverseStop rs;
    reverse_stop_init(&rs);
    reverse_stop_configure(&rs, 100.0f);

    Time time = {.now = 1000000u};
    reverse_stop_reset(&rs, 5.0f);
    REQUIRE(!reverse_stop_active(&rs));

    reverse_stop_update(&rs, 6.0f, 0.0f, 0.0f, &time, false);
    CHECK_FLOAT_NEAR(rs.start_distance, 6.0f);
    CHECK_FLOAT_NEAR(rs.target_setpoint, 0.0f);

    reverse_stop_update(&rs, 5.95f, -300.0f, 0.0f, &time, true);
    CHECK_FLOAT_NEAR(rs.target_setpoint, 17.0f);
    CHECK_FLOAT_NEAR(rs.start_setpoint, 0.0f);
    REQUIRE(rs.target_distance < 0.0f);
    REQUIRE(reverse_stop_active(&rs));
    CHECK_FLOAT_NEAR(reverse_stop_setpoint(&rs), 0.0f);
    timer_refresh(&time, &rs.timer);

    reverse_stop_update(&rs, 5.75f, -300.0f, 12.0f, &time, true);
    REQUIRE(rs.progress.value > 0.0f);
    REQUIRE(reverse_stop_setpoint(&rs) > 0.0f);
    REQUIRE(!reverse_stop_stop(&rs, &time));

    ema_reset(&rs.progress, 1.0f);
    REQUIRE(reverse_stop_stop(&rs, &time));

    reverse_stop_update(&rs, 5.95f, -300.0f, 17.0f, &time, true);
    CHECK_FLOAT_NEAR(rs.target_setpoint, 0.0f);
    CHECK_FLOAT_NEAR(rs.start_setpoint, 17.0f);
    REQUIRE(rs.target_distance > 0.0f);
    REQUIRE(reverse_stop_active(&rs));

    timer_expire(&time, &rs.timer, 3.1f);
    REQUIRE(reverse_stop_stop(&rs, &time));
}

TEST_CASE("reverse stop completion and timer edges", "[c]") {
    ReverseStop rs;
    reverse_stop_init(&rs);
    reverse_stop_configure(&rs, 100.0f);

    Time time = {.now = 2000000u};
    reverse_stop_reset(&rs, 10.0f);

    reverse_stop_update(&rs, 9.9f, -250.0f, 16.99f, &time, true);
    CHECK_FLOAT_NEAR(rs.target_setpoint, 17.0f);
    CHECK_FLOAT_NEAR(rs.target_distance, 0.0f);
    CHECK_FLOAT_NEAR(rs.progress.value, 1.0f);
    REQUIRE(reverse_stop_stop(&rs, &time));

    reverse_stop_reset(&rs, 20.0f);
    reverse_stop_update(&rs, 19.8f, -300.0f, 0.0f, &time, true);
    REQUIRE(reverse_stop_active(&rs));
    ema_reset(&rs.progress, 0.25f);
    timer_refresh(&time, &rs.timer);
    REQUIRE(!reverse_stop_stop(&rs, &time));

    timer_expire(&time, &rs.timer, 2.4f);
    REQUIRE(!reverse_stop_stop(&rs, &time));
    timer_expire(&time, &rs.timer, 2.6f);
    REQUIRE(reverse_stop_stop(&rs, &time));

    reverse_stop_reset(&rs, 30.0f);
    reverse_stop_update(&rs, 30.5f, -300.0f, 0.0f, &time, true);
    CHECK_FLOAT_NEAR(rs.target_setpoint, 0.0f);
    REQUIRE(!reverse_stop_active(&rs));
}

TEST_CASE("reverse stop progress clear and completed distance edges", "[c]") {
    ReverseStop rs;
    reverse_stop_init(&rs);
    reverse_stop_configure(&rs, 100.0f);

    Time time = {.now = 3000000u};
    reverse_stop_reset(&rs, 40.0f);
    reverse_stop_update(&rs, 39.7f, -300.0f, 0.0f, &time, true);
    REQUIRE(reverse_stop_active(&rs));
    REQUIRE(rs.target_distance < 0.0f);

    rs.progress.alpha = 1.0f;
    rs.current_distance = rs.target_distance;
    ema_reset(&rs.progress, 0.5f);
    timer_expire(&time, &rs.timer, 10.0f);
    reverse_stop_update(&rs, rs.start_distance + rs.target_distance, -300.0f, 4.0f, &time, true);
    CHECK_FLOAT_NEAR(rs.progress.value, 1.0f);
    CHECK_FLOAT_NEAR(rs.target_distance, 0.0f);
    CHECK_FLOAT_NEAR(rs.current_distance, 0.0f);
    REQUIRE(rs.timer == time.now);

    float completed_start = rs.start_distance;
    reverse_stop_update(&rs, completed_start + 0.5f, -300.0f, 0.0f, &time, true);
    CHECK_FLOAT_NEAR(rs.start_distance, completed_start + 0.5f);
    REQUIRE(reverse_stop_active(&rs));
    CHECK_FLOAT_NEAR(rs.target_distance, 0.0f);

    reverse_stop_update(&rs, rs.start_distance - 0.01f, -300.0f, 0.0f, &time, true);
    CHECK_FLOAT_NEAR(rs.start_distance, completed_start + 0.5f);
    REQUIRE(reverse_stop_active(&rs));
}

TEST_CASE("alert tracker and fatal reset", "[c]") {
    AlertTracker at;
    Time time = {.now = 1000u};
    RefloatConfig cfg = {.persistent_fatal_error = false};

    alert_tracker_init(&at);
    REQUIRE(at.persistent_fatal_error);
    REQUIRE(at.active_alert_mask == 0);
    REQUIRE(at.new_active_alert_mask == 0);
    REQUIRE(at.fatal_error == false);

    alert_tracker_configure(&at, &cfg);
    REQUIRE(!at.persistent_fatal_error);

    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    REQUIRE(at.fatal_error);
    REQUIRE(at.fw_fault_code == FAULT_CODE_ABS_OVER_CURRENT);
    REQUIRE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));

    alert_tracker_finalize(&at, &time);
    REQUIRE(alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    REQUIRE(at.new_active_alert_mask == 0);
    REQUIRE(circular_buffer_size(&at.alert_buffer) == 1);
    AlertRecord record = {};
    circular_buffer_get(&at.alert_buffer, 0, &record);
    REQUIRE(record.active);
    REQUIRE(record.id == ALERT_FW_FAULT);
    REQUIRE(record.code == FAULT_CODE_ABS_OVER_CURRENT);
    REQUIRE(record.time == 1000u);

    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    alert_tracker_finalize(&at, &time);
    REQUIRE(circular_buffer_size(&at.alert_buffer) == 1);

    time.now += 5u;
    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_OVER_TEMP_MOTOR);
    alert_tracker_finalize(&at, &time);
    REQUIRE(circular_buffer_size(&at.alert_buffer) == 2);
    circular_buffer_get(&at.alert_buffer, 1, &record);
    REQUIRE(record.active);
    REQUIRE(record.code == FAULT_CODE_OVER_TEMP_MOTOR);
    REQUIRE(at.fw_fault_code == FAULT_CODE_OVER_TEMP_MOTOR);

    time.now += 10u;
    alert_tracker_finalize(&at, &time);
    REQUIRE(at.fw_fault_code == FAULT_CODE_NONE);
    REQUIRE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    REQUIRE(circular_buffer_size(&at.alert_buffer) == 3);
    circular_buffer_get(&at.alert_buffer, 2, &record);
    REQUIRE(!record.active);
    REQUIRE(record.id == ALERT_FW_FAULT);

    cfg.persistent_fatal_error = true;
    alert_tracker_configure(&at, &cfg);
    time.now += 10u;
    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    alert_tracker_finalize(&at, &time);
    time.now += 10u;
    alert_tracker_finalize(&at, &time);
    REQUIRE(at.fatal_error);
    alert_tracker_clear_fatal(&at);
    REQUIRE(!at.fatal_error);

    for (uint8_t i = 0; i < ALERT_TRACKER_SIZE + 3; ++i) {
        time.now += 1u;
        alert_tracker_add(&at, &time, ALERT_FW_FAULT, i);
        alert_tracker_finalize(&at, &time);
    }
    REQUIRE(circular_buffer_size(&at.alert_buffer) == ALERT_TRACKER_SIZE);
    REQUIRE(alert_tracker_properties(ALERT_FW_FAULT)->type == ATYPE_FATAL);
}

TEST_CASE("alert tracker nonpersistent fatal clears when alert ends", "[c][red]") {
    AlertTracker at;
    Time time = {.now = 1000u};
    RefloatConfig cfg = {.persistent_fatal_error = false};

    alert_tracker_init(&at);
    alert_tracker_configure(&at, &cfg);

    alert_tracker_add(&at, &time, ALERT_FW_FAULT, FAULT_CODE_ABS_OVER_CURRENT);
    alert_tracker_finalize(&at, &time);
    REQUIRE(at.fatal_error);
    REQUIRE(alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));

    time.now += 10u;
    alert_tracker_finalize(&at, &time);
    REQUIRE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    REQUIRE(!at.fatal_error);
}

TEST_CASE("alert tracker rejects invalid ids", "[c][red]") {
    AlertTracker at;
    Time time = {.now = 2000u};

    alert_tracker_init(&at);

    alert_tracker_add(&at, &time, ALERT_NONE, 99);
    alert_tracker_finalize(&at, &time);
    REQUIRE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    REQUIRE(!at.fatal_error);
    REQUIRE(circular_buffer_size(&at.alert_buffer) == 0);

    time.now += 1u;
    alert_tracker_add(&at, &time, ALERT_LAST + 1, 99);
    alert_tracker_finalize(&at, &time);
    REQUIRE(!alert_tracker_is_alert_active(&at, ALERT_FW_FAULT));
    REQUIRE(!at.fatal_error);
    REQUIRE(circular_buffer_size(&at.alert_buffer) == 0);
}

}  // namespace
