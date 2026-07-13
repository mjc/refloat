#include "alert_tracker.h"
#include "atr.h"
#include "balance_filter.h"
#include "bms.h"
#include "booster.h"
#include "brake_tilt.h"
#include "charging.h"
#include "conf/buffer.h"
#include "conf/confparser.h"
#include "conf/confxml.h"
#include "data_recorder.h"
#include "filters/biquad.h"
#include "filters/ema.h"
#include "filters/sma.h"
#include "filters/smooth_setpoint.h"
#include "footpad_sensor.h"
#include "frequency_tracker.h"
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
#include "pid.h"
#include "remote.h"
#include "reverse_stop.h"
#include "state.h"
#include "time.h"
#include "torque_tilt.h"
#include "turn_tilt.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "test_runner.h"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

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

#include "c_tests/alert_tracker/tests.c"
#include "c_tests/atr/tests.c"
#include "c_tests/balance_filter/tests.c"
#include "c_tests/biquad/tests.c"
#include "c_tests/bms/tests.c"
#include "c_tests/booster/tests.c"
#include "c_tests/brake_tilt/tests.c"
#include "c_tests/charging/tests.c"
#include "c_tests/circular_buffer/tests.c"
#include "c_tests/conf_buffer/tests.c"
#include "c_tests/data_recorder/tests.c"
#include "c_tests/ema/tests.c"
#include "c_tests/footpad_sensor/tests.c"
#include "c_tests/frequency_tracker/tests.c"
#include "c_tests/haptic_feedback/tests.c"
#include "c_tests/imu/tests.c"
#include "c_tests/konami/tests.c"
#include "c_tests/lcm/tests.c"
#include "c_tests/led_driver/tests.c"
#include "c_tests/led_strip/tests.c"
#include "c_tests/motor_control/tests.c"
#include "c_tests/motor_data/tests.c"
#include "c_tests/pid/tests.c"
#include "c_tests/remote/tests.c"
#include "c_tests/reverse_stop/tests.c"
#include "c_tests/sma/tests.c"
#include "c_tests/smooth_setpoint/tests.c"
#include "c_tests/state_time/tests.c"
#include "c_tests/torque_tilt/tests.c"
#include "c_tests/transitions/tests.c"
#include "c_tests/turn_tilt/tests.c"

int main(void) {
    const TestCase tests[] = {
        TEST_CASE(
            "frequency tracker preserves rate for invalid dt", test_frequency_tracker_nonpositive_dt
        ),
        TEST_CASE(
            "frequency tracker reconfigures after settling",
            test_frequency_tracker_reconfigures_after_settling
        ),
        TEST_CASE("ema calculate alpha and reset", test_ema_calculate_alpha_and_reset),
        TEST_CASE("transition clamp and shape", test_transitions_clamp_and_shape),
        TEST_CASE("rate limit rejects invalid inputs", test_rate_limit_rejects_invalid_inputs),
        TEST_CASE("app data overflow skips send", test_send_app_data_skips_send_after_overflow),
        TEST_CASE("biquad filter modes and reset", test_biquad_filter_modes_and_reset),
        TEST_CASE(
            "pid update scales and integral limit", test_pid_update_scales_and_integral_limit
        ),
        TEST_CASE(
            "pid valid inputs preserve finite state", test_pid_valid_inputs_preserve_finite_state
        ),
        TEST_CASE("pid bounds control gains", test_pid_bounds_control_gains),
        TEST_CASE("footpad sensor", test_footpad_sensor),
        TEST_CASE(
            "footpad sensor rejects negative thresholds",
            test_footpad_sensor_rejects_negative_thresholds
        ),
        TEST_CASE("charging timeout boundaries", test_charging_timeout_boundaries),
        TEST_CASE(
            "charging decodes signed payloads and rejects malformed frames",
            test_charging_decodes_signed_payloads_and_rejects_malformed_frames
        ),
        TEST_CASE(
            "charging timeout survives VESC tick wrap",
            test_charging_timeout_survives_vesc_tick_wrap
        ),
        TEST_CASE(
            "remote maps command input to motion intent",
            test_remote_maps_command_input_to_motion_intent
        ),
        TEST_CASE(
            "remote expires UART and command input", test_remote_expires_uart_and_command_input
        ),
        TEST_CASE(
            "remote revoked move config clears stale target",
            test_remote_revoked_move_config_clears_stale_target
        ),
        TEST_CASE(
            "remote applies deadband, inversion, and idle move",
            test_remote_applies_deadband_inversion_and_idle_move
        ),
        TEST_CASE("remote maps input age and deadband", test_remote_maps_input_age_and_deadband),
        TEST_CASE("remote update respects darkride", test_remote_update_respects_darkride),
        TEST_CASE(
            "remote rejects invalid deadband config", test_remote_rejects_invalid_deadband_config
        ),
        TEST_CASE("remote rejects negative tilt limit", test_remote_rejects_negative_tilt_limit),
        TEST_CASE("remote bounds input limits", test_remote_bounds_input_limits),
        TEST_CASE("remote move torque nonfinite dt", test_remote_move_torque_nonfinite_dt),
        TEST_CASE("remote invalid input is quarantined", test_remote_invalid_input_is_quarantined),
        TEST_CASE("imu init resets state", test_imu_init_resets_state),
        TEST_CASE(
            "imu balance pitch sign and unit handoff", test_imu_balance_pitch_sign_and_unit_handoff
        ),
        TEST_CASE(
            "imu pitch rate transform and darkride", test_imu_pitch_rate_transform_and_darkride
        ),
        TEST_CASE(
            "imu flywheel pitch and roll transform boundaries",
            test_imu_flywheel_pitch_and_roll_transform_boundaries
        ),
        TEST_CASE("imu flywheel roll wrap boundaries", test_imu_flywheel_roll_wrap_boundaries),
        TEST_CASE("imu rejects nonfinite VESC inputs", test_imu_rejects_nonfinite_vesc_inputs),
        TEST_CASE(
            "imu rejects nonfinite balance filter inputs",
            test_imu_rejects_nonfinite_balance_filter_inputs
        ),
        TEST_CASE("bms reports telemetry faults", test_bms_reports_telemetry_faults),
        TEST_CASE(
            "bms timeout and configuration contract", test_bms_timeout_and_configuration_contract
        ),
        TEST_CASE(
            "bms rejects hostile voltage thresholds", test_bms_rejects_hostile_voltage_thresholds
        ),
        TEST_CASE("bms bounds hostile upper thresholds", test_bms_bounds_hostile_upper_thresholds),
        TEST_CASE("bms valid telemetry stays clear", test_bms_valid_telemetry_stays_clear),
        TEST_CASE("bms faults clear on recovery", test_bms_faults_clear_on_recovery),
        TEST_CASE(
            "bms startup grace waits for first sample",
            test_bms_startup_grace_waits_for_first_sample
        ),
        TEST_CASE("bms is fault none is false", test_bms_is_fault_none_is_false),
        TEST_CASE("led strip", test_led_strip),
        TEST_CASE("led driver setup and color encoding", test_led_driver_setup_and_color_encoding),
        TEST_CASE("led driver rejects invalid pin", test_led_driver_rejects_invalid_pin),
        TEST_CASE(
            "led driver rejects invalid color order", test_led_driver_rejects_invalid_color_order
        ),
        TEST_CASE(
            "led driver accepts strip above legacy limit",
            test_led_driver_accepts_strip_above_legacy_limit
        ),
        TEST_CASE(
            "led driver setup fails when malloc fails",
            test_led_driver_setup_fails_when_malloc_fails
        ),
        TEST_CASE(
            "led driver alternate pins and noop paths",
            test_led_driver_alternate_pins_and_noop_paths
        ),
        TEST_CASE(
            "led driver full brightness color orders", test_led_driver_full_brightness_color_orders
        ),
        TEST_CASE("data recorder requests", test_data_recorder_requests),
        TEST_CASE(
            "data recorder experiment plot export", test_data_recorder_experiment_plot_export
        ),
        TEST_CASE(
            "data recorder validates request range", test_data_recorder_validates_request_range
        ),
        TEST_CASE(
            "data recorder decimation and sample flags",
            test_data_recorder_decimation_and_sample_flags
        ),
        TEST_CASE(
            "data recorder sample rate recomputes decimation",
            test_data_recorder_sample_rate_recomputes_decimation
        ),
        TEST_CASE(
            "data recorder zero sample rate status is defined",
            test_data_recorder_zero_sample_rate_status_is_defined
        ),
        TEST_CASE(
            "data recorder status and data serialization",
            test_data_recorder_status_and_data_serialization
        ),
        TEST_CASE(
            "data recorder data payload is bounded", test_data_recorder_data_payload_is_bounded
        ),
        TEST_CASE(
            "data recorder incompatible magic", test_data_recorder_rejects_incompatible_magic
        ),
        TEST_CASE(
            "data recorder initializes valid buffer", test_data_recorder_initializes_valid_buffer
        ),
        TEST_CASE(
            "data recorder timestamp monotonicity",
            test_data_recorder_makes_timestamps_strictly_increasing
        ),
        TEST_CASE(
            "data recorder rejects tiny backing buffer",
            test_data_recorder_rejects_tiny_backing_buffer
        ),
        TEST_CASE(
            "data recorder data send pauses recording",
            test_data_recorder_data_send_pauses_recording
        ),
        TEST_CASE(
            "booster and brake tilt shape motor command",
            test_booster_and_brake_tilt_shape_motor_command
        ),
        TEST_CASE(
            "brake tilt handles downhill negative erpm",
            test_brake_tilt_handles_downhill_negative_erpm
        ),
        TEST_CASE(
            "brake tilt rejects negative strength", test_brake_tilt_rejects_negative_strength
        ),
        TEST_CASE("brake tilt bounds high strength", test_brake_tilt_bounds_high_strength),
        TEST_CASE(
            "booster applies speed and brake torque", test_booster_applies_speed_and_brake_torque
        ),
        TEST_CASE("booster ramps and resets with speed", test_booster_ramps_and_resets_with_speed),
        TEST_CASE("booster bounds control config", test_booster_bounds_control_config),
        TEST_CASE(
            "ATR applies tiltback and acceleration response",
            test_atr_applies_tiltback_and_acceleration_response
        ),
        TEST_CASE("ATR applies speed boost and reset", test_atr_applies_speed_boost_and_reset),
        TEST_CASE(
            "ATR keeps zero acceleration ratios finite", test_atr_zero_accel_ratio_stays_finite
        ),
        TEST_CASE("ATR clamps hostile control config", test_atr_clamps_hostile_control_config),
        TEST_CASE("ATR clamps upper control config", test_atr_clamps_upper_control_config),
        TEST_CASE("balance filter config asymmetry", test_balance_filter_configure_asymmetric_kp),
        TEST_CASE("balance filter bounds gains", test_balance_filter_bounds_gains),
        TEST_CASE(
            "balance filter valid samples stay finite",
            test_balance_filter_valid_samples_stay_finite
        ),
        TEST_CASE(
            "balance filter stationary level stability",
            test_balance_filter_stationary_level_stability
        ),
        TEST_CASE(
            "balance filter tiny accel skips feedback",
            test_balance_filter_tiny_accel_skips_feedback
        ),
        TEST_CASE(
            "balance filter clamps pitch estimate", test_balance_filter_clamps_pitch_estimate
        ),
        TEST_CASE("balance filter nonpositive dt", test_balance_filter_nonfinite_dt),
        TEST_CASE(
            "balance filter rejects nonfinite samples",
            test_balance_filter_rejects_nonfinite_samples
        ),
        TEST_CASE(
            "torque tilt initializes, limits, and winds down",
            test_torque_tilt_initializes_limits_and_winds_down
        ),
        TEST_CASE(
            "torque tilt follows torque sign and strength",
            test_torque_tilt_follows_torque_sign_and_strength
        ),
        TEST_CASE(
            "torque tilt applies regen and lower limits",
            test_torque_tilt_applies_regen_and_lower_limits
        ),
        TEST_CASE(
            "torque tilt rejects negative control config",
            test_torque_tilt_rejects_negative_control_config
        ),
        TEST_CASE("torque tilt bounds upper config", test_torque_tilt_bounds_upper_config),
        TEST_CASE(
            "motor control current brake and tone", test_motor_control_current_brake_and_tone
        ),
        TEST_CASE(
            "motor control handles parking and tone timing",
            test_motor_control_handles_parking_and_tone_timing
        ),
        TEST_CASE(
            "motor control invalid parking mode fails open",
            test_motor_control_invalid_parking_mode_fails_open
        ),
        TEST_CASE(
            "balance control nominal request through fake VESC",
            test_balance_control_nominal_request_through_fake_vesc
        ),
        TEST_CASE(
            "balance control current limits and modes",
            test_balance_control_current_limits_and_modes
        ),
        TEST_CASE(
            "balance control darkride and traction control",
            test_balance_control_darkride_and_traction_control
        ),
        TEST_CASE(
            "balance control softstart limits pitch-based current",
            test_balance_control_softstart_limits_pitch_based_current
        ),
        TEST_CASE(
            "balance control invalid balance estimate reaches motor command",
            test_balance_control_invalid_balance_estimate_reaches_motor_command
        ),
        TEST_CASE("motor control zero tone frequency", test_motor_control_zero_tone_frequency),
        TEST_CASE(
            "motor control saturates long tone period",
            test_motor_control_saturates_long_tone_period
        ),
        TEST_CASE(
            "motor control completes click lifecycle", test_motor_control_completes_click_lifecycle
        ),
        TEST_CASE(
            "motor control switches parking at motion threshold",
            test_motor_control_switches_parking_at_motion_threshold
        ),
        TEST_CASE(
            "motor control bounds configured currents",
            test_motor_control_bounds_configured_currents
        ),
        TEST_CASE(
            "motor control disable discards pending current",
            test_motor_control_disable_discards_pending_current
        ),
        TEST_CASE(
            "motor data refresh update and alerts", test_motor_data_refresh_update_and_alerts
        ),
        TEST_CASE(
            "motor data falls back to limits and direction",
            test_motor_data_falls_back_to_limits_and_direction
        ),
        TEST_CASE(
            "motor data initializes alerts and saturates fields",
            test_motor_data_initializes_alerts_and_saturates_fields
        ),
        TEST_CASE(
            "motor data derives forward direction from speed",
            test_motor_data_derives_forward_direction_from_speed
        ),
        TEST_CASE(
            "motor data applies torque constant configuration",
            test_motor_data_applies_torque_constant_configuration
        ),
        TEST_CASE(
            "motor data rejects nonfinite VESC limits",
            test_motor_data_rejects_nonfinite_vesc_limits
        ),
        TEST_CASE("motor data erpm speed conversion", test_motor_data_erpm_speed_conversion),
        TEST_CASE("motor data nonpositive dt", test_motor_data_nonpositive_dt),
        TEST_CASE(
            "motor data bounds current filter cutoff", test_motor_data_bounds_current_filter_cutoff
        ),
        TEST_CASE(
            "motor data isolates invalid telemetry fields",
            test_motor_data_invalid_telemetry_is_quarantined
        ),
        TEST_CASE("turn tilt blends turn response", test_turn_tilt_blends_turn_response),
        TEST_CASE(
            "turn tilt aggregates inputs and boost", test_turn_tilt_aggregates_inputs_and_boost
        ),
        TEST_CASE(
            "turn tilt activates at configured threshold",
            test_turn_tilt_activates_at_configured_threshold
        ),
        TEST_CASE("turn tilt zero denominator config", test_turn_tilt_zero_denominator_config),
        TEST_CASE(
            "turn tilt rejects negative angle limit", test_turn_tilt_rejects_negative_angle_limit
        ),
        TEST_CASE("turn tilt bounds upper config", test_turn_tilt_bounds_upper_config),
        TEST_CASE("turn tilt nonpositive dt", test_turn_tilt_nonpositive_dt),
        TEST_CASE("reverse stop update paths", test_reverse_stop_update_paths),
        TEST_CASE(
            "reverse stop completes after configured duration",
            test_reverse_stop_completes_after_configured_duration
        ),
        TEST_CASE(
            "reverse stop clears progress at completed distance",
            test_reverse_stop_clears_progress_at_completed_distance
        ),
        TEST_CASE("state transitions and compatibility", test_state_transitions_and_compatibility),
        TEST_CASE("time updates and clock fallback", test_time_updates_and_clock_fallback),
        TEST_CASE("alert tracker and fatal reset", test_alert_tracker_and_fatal_reset),
        TEST_CASE(
            "alert tracker nonpersistent fatal clears when alert ends",
            test_alert_tracker_nonpersistent_fatal_clears_when_alert_ends
        ),
        TEST_CASE("alert tracker rejects invalid ids", test_alert_tracker_rejects_invalid_ids),
        TEST_CASE("konami sequence and timeout", test_konami_sequence_and_timeout),
        TEST_CASE("konami boundary and idle inputs", test_konami_boundary_and_idle_inputs),
        TEST_CASE("konami single step sequence", test_konami_single_step_sequence),
        TEST_CASE("haptic feedback patterns", test_haptic_feedback_patterns),
        TEST_CASE(
            "haptic feedback gates strength by state", test_haptic_feedback_gates_strength_by_state
        ),
        TEST_CASE(
            "haptic feedback shared strength scale", test_haptic_feedback_shared_strength_scale
        ),
        TEST_CASE(
            "haptic feedback runtime disable stops outputs",
            test_haptic_feedback_stops_outputs_when_runtime_config_disables_them
        ),
        TEST_CASE(
            "haptic feedback pattern type change lockout",
            test_haptic_feedback_pattern_type_change_lockout
        ),
        TEST_CASE(
            "haptic feedback selects configured pattern",
            test_haptic_feedback_selects_configured_pattern
        ),
        TEST_CASE(
            "haptic feedback pauses error pattern", test_haptic_feedback_pauses_error_pattern
        ),
        TEST_CASE(
            "haptic feedback bounds Bluetooth config values",
            test_haptic_feedback_bounds_bluetooth_config_values
        ),
        TEST_CASE(
            "SMA applies first pending resize without output jump",
            test_sma_applies_first_pending_resize_without_output_jump
        ),
        TEST_CASE("sma rejects invalid inputs", test_sma_rejects_invalid_inputs),
        TEST_CASE(
            "SMA preserves steady output across resize",
            test_sma_preserves_steady_output_across_resize
        ),
        TEST_CASE("sma allocation failure update", test_sma_allocation_failure_update),
        TEST_CASE("circular buffer pop index", test_circular_buffer_pop_index),
        TEST_CASE(
            "circular buffer zero capacity is inert", test_circular_buffer_zero_capacity_is_inert
        ),
        TEST_CASE(
            "circular buffer push/get order property", test_circular_buffer_push_get_order_property
        ),
        TEST_CASE("buffer integer round-trip property", test_buffer_integer_roundtrip_property),
        TEST_CASE("buffer float encoding property", test_buffer_float_encoding_property),
        TEST_CASE("buffer strings and scaled float32", test_buffer_strings_and_scaled_float32),
        TEST_CASE("lcm payload clamp", test_lcm_light_ctrl_payload_clamps),
        TEST_CASE(
            "smooth setpoint negative time constants", test_smooth_setpoint_negative_time_constants
        ),
        TEST_CASE(
            "smooth setpoint rejects negative speed limits",
            test_smooth_setpoint_rejects_negative_speed_limits
        ),
        TEST_CASE("smooth setpoint winddown restart", test_smooth_setpoint_winddown_restart),
        TEST_CASE(
            "smooth setpoint rejects invalid update inputs",
            test_smooth_setpoint_rejects_invalid_update_inputs
        ),
        TEST_CASE("lcm disabled responses are minimal", test_lcm_disabled_responses_are_minimal),
        TEST_CASE(
            "lcm init configure and runtime brightness",
            test_lcm_init_configure_and_runtime_brightness
        ),
        TEST_CASE(
            "lcm configure requires initialized led config",
            test_lcm_configure_requires_initialized_led_config
        ),
        TEST_CASE(
            "LCM poll encodes pitch, name, and response",
            test_lcm_poll_encodes_pitch_name_and_response
        ),
        TEST_CASE("lcm enabled response contracts", test_lcm_enabled_response_contracts),
        TEST_CASE("lcm poll request respects name length", test_lcm_poll_request_name_length_bound),
        TEST_CASE("lcm configure saturates brightness", test_lcm_configure_saturates_brightness),
        TEST_CASE(
            "lcm poll response saturates byte fields", test_lcm_poll_response_saturates_byte_fields
        ),
        TEST_CASE(
            "lcm battery response nonfinite values are stable",
            test_lcm_battery_response_nonfinite_values_are_stable
        ),
    };

    RUN_TEST_SUITE("summary", tests);
}
