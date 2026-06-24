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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_runner.h"

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


#include "c_tests/frequency_tracker/tests.c"
#include "c_tests/smooth_setpoint/tests.c"
#include "c_tests/footpad_sensor/tests.c"
#include "c_tests/charging/tests.c"
#include "c_tests/remote/tests.c"
#include "c_tests/imu/tests.c"
#include "c_tests/bms/tests.c"
#include "c_tests/booster/tests.c"
#include "c_tests/brake_tilt/tests.c"
#include "c_tests/tilt_balance/tests.c"
#include "c_tests/motor_control/tests.c"
#include "c_tests/motor_data/tests.c"
#include "c_tests/turn_tilt/tests.c"
#include "c_tests/reverse_stop/tests.c"
#include "c_tests/alert_tracker/tests.c"
#include "c_tests/konami/tests.c"
#include "c_tests/haptic_feedback/tests.c"
#include "c_tests/sma/tests.c"
#include "c_tests/lcm/tests.c"
#include "c_tests/circular_buffer/tests.c"
#include "c_tests/data_recorder/tests.c"
#include "c_tests/led_driver/tests.c"
#include "c_tests/led_strip/tests.c"

int main(void) {
    const TestCase tests[] = {        XFAIL_CASE("frequency tracker nonpositive dt", test_frequency_tracker_nonpositive_dt,
                "red test: frequency_tracker_update divides by dt directly, so zero or negative "
                "loop deltas can feed infinity or invalid negative frequencies into the EMA "
                "instead of being ignored or clamped to a safe positive sample"),
        TEST_CASE("footpad sensor", test_footpad_sensor),
        TEST_CASE("charging timeout boundaries", test_charging_timeout_boundaries),
        XFAIL_CASE("charging signed payload and invalid frame edges", test_charging_signed_payload_and_invalid_frame_edges,
                "red test: charging_state_request accepts signed negative charging voltage/current "
                "from an external module and marks the board charging instead of rejecting or "
                "sanitizing physically invalid telemetry"),
        TEST_CASE("remote branch cases", test_remote_branch_cases),
        TEST_CASE("remote uart and command timeout edges", test_remote_uart_and_command_timeout_edges),
        TEST_CASE("remote deadband invert and idle move edges", test_remote_deadband_invert_and_idle_move_edges),
        TEST_CASE("remote deadband and age boundaries", test_remote_deadband_and_age_boundaries),
        XFAIL_CASE("remote rejects invalid deadband config", test_remote_rejects_invalid_deadband_config,
                "red test: deserialized config can set inputtilt_deadband to 1.0 or higher, "
                "and remote_input currently divides by 1 - deadband instead of receiving "
                "sanitized bounds, so remote input and move speed can become nonfinite"),
        XFAIL_CASE("remote move torque nonfinite dt", test_remote_move_torque_nonfinite_dt,
                "red test: remote_get_move_torque integrates the move PID term with dt "
                "directly, so a nonpositive or nonfinite loop delta can corrupt the "
                "remote move torque state instead of being skipped as an invalid sample"),
        TEST_CASE("imu update edges", test_imu_update_edges),
        TEST_CASE("imu flywheel roll wrap boundaries", test_imu_flywheel_roll_wrap_boundaries),
        TEST_CASE("bms faults", test_bms_faults),
        TEST_CASE("bms threshold boundaries", test_bms_threshold_boundaries),
        TEST_CASE("bms faults clear on recovery", test_bms_faults_clear_on_recovery),
        XFAIL_CASE("bms startup grace waits for first sample", test_bms_startup_grace_waits_for_first_sample,
                "red test: bms_init seeds cell voltage/temperature fields with zeroes and "
                "bms_update suppresses only the connection fault during startup grace, so enabled "
                "BMS can report cell threshold faults before any real BMS sample arrives"),
        XFAIL_CASE("bms is fault none is false", test_bms_is_fault_none_is_false,
                "red test: bms_is_fault accepts BMSF_NONE from the public enum but shifts by "
                "fault_code - 1, so querying the no-fault sentinel can read a bogus bit instead "
                "of returning false"),
        TEST_CASE("led strip", test_led_strip),
        TEST_CASE("led driver setup and color encoding", test_led_driver_setup_and_color_encoding),
        TEST_CASE("led driver rejects invalid pin", test_led_driver_rejects_invalid_pin),
        XFAIL_CASE("led driver rejects invalid color order", test_led_driver_rejects_invalid_color_order,
                "red test: LED color order is config-driven, and led_driver_paint switches "
                "without a default before calling color_conv, so an invalid color order can "
                "leave an indeterminate function pointer instead of being rejected before setup"),
        XFAIL_CASE("led driver rejects oversized strip count", test_led_driver_rejects_oversized_strip_count,
                "red test: hardware LED strip counts are config-driven with a documented max "
                "of 30 per strip, but led_driver_setup currently trusts the strip length and "
                "can allocate/arm DMA for malformed oversized configs instead of rejecting them"),
        TEST_CASE("led driver alternate pins and noop paths", test_led_driver_alternate_pins_and_noop_paths),
        TEST_CASE("led driver full brightness color orders", test_led_driver_full_brightness_color_orders),
        TEST_CASE("data recorder requests", test_data_recorder_requests),
        XFAIL_CASE("data recorder experiment plot export", test_data_recorder_experiment_plot_export,
                "red test: data_recorder_send_experiment_plot iterates multi-byte Sample records "
                "through circular_buffer_iterate, which currently passes byte-offset item "
                "addresses instead of item_size-scaled addresses"),
        TEST_CASE("data recorder request edges", test_data_recorder_request_edges),
        TEST_CASE("data recorder decimation and sample flags", test_data_recorder_decimation_and_sample_flags),
        XFAIL_CASE("data recorder sample rate recomputes decimation", test_data_recorder_sample_rate_recomputes_decimation,
                "red test: data_recorder_set_sample_rate updates sample_rate but leaves "
                "decimation stale, so a higher measured IMU rate records too often and "
                "shortens the fixed buffer retention window instead of recomputing the "
                "same at-least-ten-second coverage used during initialization"),
        TEST_CASE("data recorder status and data serialization", test_data_recorder_status_and_data_serialization),
        XFAIL_CASE("data recorder rejects tiny backing buffer", test_data_recorder_rejects_tiny_backing_buffer,
                "red test: data_recorder_init trusts firmware-provided buffer length after "
                "the magic check, so a nonzero buffer smaller than one Sample produces "
                "sample_count == 0 and divides by sample_count while computing decimation "
                "instead of rejecting the recorder as unavailable"),
        XFAIL_CASE("data recorder data send pauses recording", test_data_recorder_data_send_pauses_recording,
                "red test: DATA_RECORD header requests pause recording before export, but "
                "direct DATA_RECORD data-send requests leave recording active while walking "
                "the circular buffer, so concurrent sampling can mutate the export window "
                "instead of snapshotting or pausing before serialization"),
        TEST_CASE("booster and brake tilt branch cases", test_booster_and_brake_tilt_branch_cases),
        TEST_CASE("brake tilt negative erpm downhill boundaries", test_brake_tilt_negative_erpm_downhill_boundaries),
        TEST_CASE("booster threshold boundary edges", test_booster_threshold_boundary_edges),
        TEST_CASE("booster threshold ramp and reset edges", test_booster_threshold_ramp_and_reset_edges),
        TEST_CASE("atr branch cases", test_atr_branch_cases),
        TEST_CASE("atr threshold speedboost and reset edges", test_atr_threshold_speedboost_and_reset_edges),
        XFAIL_CASE("atr zero accel ratio config", test_atr_zero_accel_ratio_config,
                "red test: deserialized config can set atr_amps_accel_ratio or "
                "atr_amps_decel_ratio to zero, and atr_update currently divides by the "
                "derived factors instead of receiving sanitized positive bounds"),        XFAIL_CASE("balance filter nonfinite dt", test_balance_filter_nonfinite_dt,
                "red test: balance_filter_update integrates gyro rates with dt directly and "
                "normalizes the result, so a nonfinite loop delta can poison quaternion state "
                "and derived roll/pitch/yaw instead of being ignored as an invalid sample"),
        TEST_CASE("torque tilt", test_torque_tilt_threshold_limit_regen_and_wheelslip),
        TEST_CASE("torque tilt sign strength and filter edges", test_torque_tilt_sign_strength_and_filter_edges),
        TEST_CASE("torque tilt negative limit and regen edges", test_torque_tilt_negative_limit_and_regen_edges),
        TEST_CASE("motor control current brake and tone", test_motor_control_current_brake_and_tone),
        TEST_CASE("motor control parking and tone edges", test_motor_control_parking_and_tone_edges),
        XFAIL_CASE("motor control zero tone frequency", test_motor_control_zero_tone_frequency,
                "red test: deserialized haptic tone config can set frequency to zero, and "
                "motor_control_play_tone divides main_freq by that frequency instead of "
                "ignoring or sanitizing the invalid tone"),
        TEST_CASE("motor control click lifecycle edges", test_motor_control_click_lifecycle_edges),
        TEST_CASE("motor control parking and moving threshold edges", test_motor_control_parking_and_moving_threshold_edges),
        TEST_CASE("motor data refresh update and alerts", test_motor_data_refresh_update_and_alerts),
        TEST_CASE("motor data fallback limits and direction edges", test_motor_data_fallback_limits_and_direction_edges),
        TEST_CASE("motor data init alert and saturation edges", test_motor_data_init_alert_and_saturation_edges),
        TEST_CASE("motor data forward direction threshold edges", test_motor_data_forward_direction_threshold_edges),
        TEST_CASE("motor data torque constant config edges", test_motor_data_torque_constant_config_edges),
        XFAIL_CASE("motor data erpm speed conversion", test_motor_data_erpm_speed_conversion,
                "red test: motor_data_update still uses mc_get_speed() every tick even when "
                "motor poles, gear ratio, and wheel diameter are available to compute speed "
                "from the already-fetched ERPM value"),
        XFAIL_CASE("motor data nonpositive dt", test_motor_data_nonpositive_dt,
                "red test: motor_data_update divides ERPM delta by dt before feeding the "
                "acceleration SMA, so zero, negative, or nonfinite loop deltas can inject "
                "Inf/NaN into derived motor state instead of being skipped as invalid samples"),
        TEST_CASE("turn tilt branch cases", test_turn_tilt_branch_cases),
        TEST_CASE("turn tilt aggregate and boost edges", test_turn_tilt_aggregate_and_boost_edges),
        TEST_CASE("turn tilt threshold boundary edges", test_turn_tilt_threshold_boundary_edges),
        XFAIL_CASE("turn tilt zero denominator config", test_turn_tilt_zero_denominator_config,
                "red test: deserialized config can set turntilt_erpm_boost_end or "
                "turntilt_yaw_aggregate to zero, and turn_tilt_configure/update currently "
                "divide by those values instead of receiving sanitized positive bounds"),
        XFAIL_CASE("turn tilt nonpositive dt", test_turn_tilt_nonpositive_dt,
                "red test: turn_tilt_aggregate divides yaw delta by dt before filtering, so "
                "zero, negative, or nonfinite loop deltas can be treated as real yaw-rate "
                "samples and mutate last-yaw/aggregate state instead of being ignored"),
        TEST_CASE("reverse stop update paths", test_reverse_stop_update_paths),
        TEST_CASE("reverse stop completion and timer edges", test_reverse_stop_completion_and_timer_edges),
        TEST_CASE("reverse stop progress clear and completed distance edges", test_reverse_stop_progress_clears_and_completed_distance_edges),
        TEST_CASE("alert tracker and fatal reset", test_alert_tracker_and_fatal_reset),
        XFAIL_CASE("alert tracker nonpersistent fatal clears when alert ends", test_alert_tracker_nonpersistent_fatal_clears_when_alert_ends,
                "red test: alert_tracker_finalize records the ended fatal alert but still uses "
                "the previous active_alert_mask when deciding whether nonpersistent fatal_error "
                "can clear, so fatal_error remains set one finalize too long"),
        XFAIL_CASE("alert tracker rejects invalid ids", test_alert_tracker_rejects_invalid_ids,
                "red test: alert_tracker_add accepts a caller-provided alert id but always sets "
                "the firmware-fault active mask and indexes alert_properties(id) without first "
                "validating that id is in the real alert range. Invalid ids should be ignored "
                "instead of activating ALERT_FW_FAULT or reading outside the properties table"),
        TEST_CASE("konami sequence and timeout", test_konami_sequence_and_timeout),
        TEST_CASE("konami boundary and idle inputs", test_konami_boundary_and_idle_inputs),
        TEST_CASE("konami single step sequence", test_konami_single_step_sequence),
        TEST_CASE("haptic feedback patterns", test_haptic_feedback_patterns),
        TEST_CASE("haptic feedback gating and strength edges", test_haptic_feedback_gating_and_strength_edges),
        TEST_CASE("haptic feedback shared strength scale", test_haptic_feedback_shared_strength_scale),
        TEST_CASE("haptic feedback pattern type change lockout", test_haptic_feedback_pattern_type_change_lockout),
        TEST_CASE("haptic feedback type selection edges", test_haptic_feedback_type_selection_edges),
        TEST_CASE("haptic feedback error pattern pause edges", test_haptic_feedback_error_pattern_pause_edges),        XFAIL_CASE("sma growth transition edges", test_sma_growth_transition_edges,
                "red test: sma_configure allocates the backing array, calls sma_reset while n is "
                "still zero, then sets n, so the initial averaging window can contain "
                "uninitialized values before the first full cycle"),
        XFAIL_CASE("sma allocation failure update", test_sma_allocation_failure_update,
                "red test: after sma_configure allocation failure, sma_update still indexes the "
                "null backing array and divides by n == 0 instead of treating the unconfigured "
                "filter as a safe no-op"),
        XFAIL_CASE("circular buffer pop index", test_circular_buffer_pop_index,
                "red test: circular_buffer_pop ignores the requested index after copying the item "
                "and always advances tail, so arbitrary-index removal returns the wrong sequence"),
        XFAIL_CASE("lcm payload clamp", test_lcm_light_ctrl_payload_clamps,
                "red test: lcm_light_ctrl_request stores len - 3 bytes into a fixed 64-byte "
                "payload buffer without bounding payload_size"),        XFAIL_CASE("smooth setpoint negative time constants", test_smooth_setpoint_negative_time_constants,
                "red test: deserialized config can set smooth-setpoint filter time constants "
                "negative, and smooth_setpoint_configure currently passes them into EMA alpha "
                "calculation instead of receiving sanitized positive bounds"),        TEST_CASE("lcm disabled responses are minimal", test_lcm_disabled_responses_are_minimal),
        TEST_CASE("lcm init configure and runtime brightness", test_lcm_init_configure_and_runtime_brightness),
        XFAIL_CASE("lcm configure requires initialized led config", test_lcm_configure_requires_initialized_led_config,
                "red test: external LCM startup can call lcm_configure before leds_setup assigns "
                "leds.cfg, and lcm_configure currently dereferences leds->cfg instead of treating "
                "the missing LED config as unavailable/default runtime state"),
        TEST_CASE("lcm poll response pitch payload and name edges", test_lcm_poll_response_pitch_payload_and_name_edges),
        XFAIL_CASE("lcm poll request respects name length", test_lcm_poll_request_name_length_bound,
                "red test: lcm_poll_request treats i == len as readable and only stops when "
                "i > len, so a non-NUL-terminated name can copy one byte past the declared "
                "request length"),
        XFAIL_CASE("lcm poll response saturates byte fields", test_lcm_poll_response_saturates_byte_fields,
                "red test: lcm_poll_response writes float telemetry directly into uint8_t "
                "packet slots, so out-of-range pitch can wrap instead of saturating to a "
                "stable byte value"),        XFAIL_CASE("lcm battery response nonfinite values are stable", test_lcm_battery_response_nonfinite_values_are_stable,
                "red test: lcm_get_battery_response serializes battery telemetry through "
                "buffer_append_float32_auto, which does not explicitly handle NaN or infinity "
                "before frexpf and float-to-uint32 conversion"),    };

    RUN_TEST_SUITE("summary", tests);
}
