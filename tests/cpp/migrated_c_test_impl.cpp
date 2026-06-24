#include "filters/biquad.h"
#include "filters/ema.h"
#include "filters/sma.h"
#include "filters/smooth_setpoint.h"
#include "conf/buffer.h"
#include "conf/confparser.h"
#include "conf/confxml.h"
#include "frequency_tracker.h"
#include "alert_tracker.h"
#include "atr.h"
#include "bms.h"
#include "booster.h"
#include "brake_tilt.h"
#include "charging.h"
#include "balance_filter.h"
#include "data_recorder.h"
#include "footpad_sensor.h"
#include "haptic_feedback.h"
#include "imu.h"
#include "konami.h"
#include "lcm.h"
#include "led_driver.h"
#include "led_strip.h"
#include "lib/circular_buffer.h"
#include "lib/transitions.h"
#include "lib/utils.h"
#include "motor_control.h"
#include "reverse_stop.h"
#include "remote.h"
#include "pid.h"
#include "torque_tilt.h"
#include "turn_tilt.h"
#include "state.h"
#include "time.h"

#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void (*SignalHandler)(int);
SignalHandler signal(int signal_number, SignalHandler handler);

typedef struct {
    uint8_t lo;
    uint8_t hi;
} BufferItem;

void feedback_fakes_reset(void);
size_t feedback_fakes_play_tone_calls(void);
size_t feedback_fakes_stop_tone_calls(void);
size_t feedback_fakes_led_confirm_calls(void);
uint16_t feedback_fakes_last_frequency(void);
float feedback_fakes_last_intensity(void);
void lcm_fakes_set_runtime_status(bool enabled, bool headlights_enabled);

static bool buffer_item_eq(BufferItem lhs, BufferItem rhs) {
    return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
}

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_FLOAT_NEAR(actual, expected)                                                         \
    do {                                                                                           \
        float actual__ = (actual);                                                                 \
        float expected__ = (expected);                                                             \
        if (fabsf(actual__ - expected__) > 1e-4f) {                                                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_U32(actual, expected)                                                                \
    do {                                                                                           \
        uint32_t actual__ = (actual);                                                              \
        uint32_t expected__ = (expected);                                                          \
        if (actual__ != expected__) {                                                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#include "migrated_c_tests/smooth_state_time.inc"
#include "migrated_c_tests/input_remote_imu.inc"
#include "migrated_c_tests/bms_leds_data.inc"
#include "migrated_c_tests/tilt_balance.inc"
#include "migrated_c_tests/motor.inc"
#include "migrated_c_tests/turn_reverse_alert.inc"
#include "migrated_c_tests/konami_haptic.inc"
#include "migrated_c_tests/pid_filters_lcm.inc"

typedef bool (*MigratedCTestFn)(void);

typedef struct {
    const char *name;
    MigratedCTestFn fn;
} MigratedCTest;

static const MigratedCTest migrated_c_tests[] = {
    {"frequency tracker nonpositive dt", test_frequency_tracker_nonpositive_dt},
    {"footpad sensor", test_footpad_sensor},
    {"charging timeout boundaries", test_charging_timeout_boundaries},
    {"charging signed payload and invalid frame edges", test_charging_signed_payload_and_invalid_frame_edges},
    {"remote branch cases", test_remote_branch_cases},
    {"remote uart and command timeout edges", test_remote_uart_and_command_timeout_edges},
    {"remote deadband invert and idle move edges", test_remote_deadband_invert_and_idle_move_edges},
    {"remote deadband and age boundaries", test_remote_deadband_and_age_boundaries},
    {"remote rejects invalid deadband config", test_remote_rejects_invalid_deadband_config},
    {"remote move torque nonfinite dt", test_remote_move_torque_nonfinite_dt},
    {"imu update edges", test_imu_update_edges},
    {"imu flywheel roll wrap boundaries", test_imu_flywheel_roll_wrap_boundaries},
    {"bms faults", test_bms_faults},
    {"bms threshold boundaries", test_bms_threshold_boundaries},
    {"bms faults clear on recovery", test_bms_faults_clear_on_recovery},
    {"bms startup grace waits for first sample", test_bms_startup_grace_waits_for_first_sample},
    {"bms is fault none is false", test_bms_is_fault_none_is_false},
    {"led strip", test_led_strip},
    {"led driver setup and color encoding", test_led_driver_setup_and_color_encoding},
    {"led driver rejects invalid pin", test_led_driver_rejects_invalid_pin},
    {"led driver rejects invalid color order", test_led_driver_rejects_invalid_color_order},
    {"led driver rejects oversized strip count", test_led_driver_rejects_oversized_strip_count},
    {"led driver alternate pins and noop paths", test_led_driver_alternate_pins_and_noop_paths},
    {"led driver full brightness color orders", test_led_driver_full_brightness_color_orders},
    {"data recorder requests", test_data_recorder_requests},
    {"data recorder experiment plot export", test_data_recorder_experiment_plot_export},
    {"data recorder request edges", test_data_recorder_request_edges},
    {"data recorder decimation and sample flags", test_data_recorder_decimation_and_sample_flags},
    {"data recorder sample rate recomputes decimation", test_data_recorder_sample_rate_recomputes_decimation},
    {"data recorder status and data serialization", test_data_recorder_status_and_data_serialization},
    {"data recorder rejects tiny backing buffer", test_data_recorder_rejects_tiny_backing_buffer},
    {"data recorder data send pauses recording", test_data_recorder_data_send_pauses_recording},
    {"booster and brake tilt branch cases", test_booster_and_brake_tilt_branch_cases},
    {"brake tilt negative erpm downhill boundaries", test_brake_tilt_negative_erpm_downhill_boundaries},
    {"booster threshold boundary edges", test_booster_threshold_boundary_edges},
    {"booster threshold ramp and reset edges", test_booster_threshold_ramp_and_reset_edges},
    {"atr branch cases", test_atr_branch_cases},
    {"atr threshold speedboost and reset edges", test_atr_threshold_speedboost_and_reset_edges},
    {"atr zero accel ratio config", test_atr_zero_accel_ratio_config},
    {"balance filter nonfinite dt", test_balance_filter_nonfinite_dt},
    {"torque tilt", test_torque_tilt_threshold_limit_regen_and_wheelslip},
    {"torque tilt sign strength and filter edges", test_torque_tilt_sign_strength_and_filter_edges},
    {"torque tilt negative limit and regen edges", test_torque_tilt_negative_limit_and_regen_edges},
    {"motor control current brake and tone", test_motor_control_current_brake_and_tone},
    {"motor control parking and tone edges", test_motor_control_parking_and_tone_edges},
    {"motor control zero tone frequency", test_motor_control_zero_tone_frequency},
    {"motor control click lifecycle edges", test_motor_control_click_lifecycle_edges},
    {"motor control parking and moving threshold edges", test_motor_control_parking_and_moving_threshold_edges},
    {"motor data refresh update and alerts", test_motor_data_refresh_update_and_alerts},
    {"motor data fallback limits and direction edges", test_motor_data_fallback_limits_and_direction_edges},
    {"motor data init alert and saturation edges", test_motor_data_init_alert_and_saturation_edges},
    {"motor data forward direction threshold edges", test_motor_data_forward_direction_threshold_edges},
    {"motor data torque constant config edges", test_motor_data_torque_constant_config_edges},
    {"motor data erpm speed conversion", test_motor_data_erpm_speed_conversion},
    {"motor data nonpositive dt", test_motor_data_nonpositive_dt},
    {"turn tilt branch cases", test_turn_tilt_branch_cases},
    {"turn tilt aggregate and boost edges", test_turn_tilt_aggregate_and_boost_edges},
    {"turn tilt threshold boundary edges", test_turn_tilt_threshold_boundary_edges},
    {"turn tilt zero denominator config", test_turn_tilt_zero_denominator_config},
    {"turn tilt nonpositive dt", test_turn_tilt_nonpositive_dt},
    {"reverse stop update paths", test_reverse_stop_update_paths},
    {"reverse stop completion and timer edges", test_reverse_stop_completion_and_timer_edges},
    {"reverse stop progress clear and completed distance edges", test_reverse_stop_progress_clears_and_completed_distance_edges},
    {"alert tracker and fatal reset", test_alert_tracker_and_fatal_reset},
    {"alert tracker nonpersistent fatal clears when alert ends", test_alert_tracker_nonpersistent_fatal_clears_when_alert_ends},
    {"alert tracker rejects invalid ids", test_alert_tracker_rejects_invalid_ids},
    {"konami sequence and timeout", test_konami_sequence_and_timeout},
    {"konami boundary and idle inputs", test_konami_boundary_and_idle_inputs},
    {"konami single step sequence", test_konami_single_step_sequence},
    {"haptic feedback patterns", test_haptic_feedback_patterns},
    {"haptic feedback gating and strength edges", test_haptic_feedback_gating_and_strength_edges},
    {"haptic feedback shared strength scale", test_haptic_feedback_shared_strength_scale},
    {"haptic feedback pattern type change lockout", test_haptic_feedback_pattern_type_change_lockout},
    {"haptic feedback type selection edges", test_haptic_feedback_type_selection_edges},
    {"haptic feedback error pattern pause edges", test_haptic_feedback_error_pattern_pause_edges},
    {"sma growth transition edges", test_sma_growth_transition_edges},
    {"sma allocation failure update", test_sma_allocation_failure_update},
    {"circular buffer pop index", test_circular_buffer_pop_index},
    {"lcm payload clamp", test_lcm_light_ctrl_payload_clamps},
    {"smooth setpoint negative time constants", test_smooth_setpoint_negative_time_constants},
    {"lcm disabled responses are minimal", test_lcm_disabled_responses_are_minimal},
    {"lcm init configure and runtime brightness", test_lcm_init_configure_and_runtime_brightness},
    {"lcm configure requires initialized led config", test_lcm_configure_requires_initialized_led_config},
    {"lcm poll response pitch payload and name edges", test_lcm_poll_response_pitch_payload_and_name_edges},
    {"lcm poll request respects name length", test_lcm_poll_request_name_length_bound},
    {"lcm poll response saturates byte fields", test_lcm_poll_response_saturates_byte_fields},
    {"lcm battery response nonfinite values are stable", test_lcm_battery_response_nonfinite_values_are_stable},
};

bool refloat_run_migrated_c_test(const char *name) {
    for (size_t i = 0; i < sizeof(migrated_c_tests) / sizeof(migrated_c_tests[0]); ++i) {
        if (strcmp(migrated_c_tests[i].name, name) == 0) {
            return migrated_c_tests[i].fn();
        }
    }

    fprintf(stderr, "unknown migrated C test: %s\n", name);
    return false;
}
