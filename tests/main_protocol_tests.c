#include "conf/buffer.h"
#include "conf/conf_general.h"
#include "conf/confparser.h"
#include "data.h"
#include "vesc_if_fake.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "test_runner.h"

#define fatal_error_terminate refloat_main_fatal_error_terminate
#include "../src/main.c"
#undef fatal_error_terminate

#define refloat_main_can_engage can_engage
#define refloat_main_check_faults check_faults
#define refloat_main_calculate_setpoint_target calculate_setpoint_target
#define refloat_main_apply_noseangling apply_noseangling
#define refloat_main_get_setpoint_adjustment_speed get_setpoint_adjustment_speed
#define refloat_main_encode_extra_flags encode_extra_flags
#define refloat_main_encode_state_flags encode_state_flags
#define refloat_main_reset_runtime_vars reset_runtime_vars
#define refloat_main_engage engage
#define refloat_main_pid_control pid_control
#define refloat_main_read_cfg_from_eeprom read_cfg_from_eeprom
#define refloat_main_write_cfg_to_eeprom(data) write_cfg_to_eeprom((data), &(data)->float_conf)
#define refloat_main_reconfigure reconfigure
#define refloat_main_run_loop refloat_thd
#define refloat_main_run_aux_loop aux_thd

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef struct {
    lib_info info;
    Data *data;
} MainProtocolFixture;

static bool main_protocol_fixture_start(MainProtocolFixture *fixture) {
    vesc_if_fake_reset();

    if (!init(&fixture->info)) {
        return false;
    }
    if (fixture->info.arg == NULL || fixture->info.stop_fun == NULL) {
        return false;
    }
    vesc_if_fake_set_arg(fixture->info.arg);

    fixture->data = fixture->info.arg;
    return true;
}

static void main_protocol_fixture_stop(MainProtocolFixture *fixture) {
    fixture->info.stop_fun(fixture->info.arg);
}

static void main_protocol_stop_info(const lib_info *info) {
    info->stop_fun(info->arg);
}

// clang-format off
#include "c_tests/test_support.h"
#include "c_tests/main_protocol/main_gnss_protocol.c"
#include "c_tests/main_protocol/main_alerts_lights_protocol.c"
#include "c_tests/main_protocol/main_all_data_serialization.c"
#include "c_tests/main_protocol/main_beeper_state.c"
#include "c_tests/main_protocol/main_command_length_protocol.c"
#include "c_tests/main_protocol/main_fault_matrix.c"
#include "c_tests/main_protocol/main_flywheel_protocol.c"
#include "c_tests/main_protocol/main_lifecycle_protocol.c"
#include "c_tests/main_protocol/main_response_matrix.c"
#include "c_tests/main_protocol/main_runtime_behavior.c"
#include "c_tests/main_protocol/main_sequence_property.c"
// clang-format on

int main(void) {
    const TestCase tests[] = {
        TEST_CASE("main gnss protocol", test_main_gnss_protocol),
        TEST_CASE(
            "main info handles optional gnss unavailable",
            test_main_info_handles_optional_gnss_unavailable
        ),
        TEST_CASE(
            "main realtime handles optional gnss unavailable",
            test_main_realtime_handles_optional_gnss_unavailable
        ),
        TEST_CASE(
            "main realtime handles optional foc id unavailable",
            test_main_realtime_handles_optional_foc_id_unavailable
        ),
        TEST_CASE("main lights control protocol", test_main_lights_control_protocol),
        TEST_CASE("main alerts protocol", test_main_alerts_protocol),
        TEST_CASE("main invalid command protocol", test_main_invalid_command_protocol),
        TEST_CASE("main info rejects unknown LED mode", test_main_info_rejects_unknown_led_mode),
        TEST_CASE("main engagement conditions", test_main_can_engage_conditions),
        TEST_CASE("main fault stop conditions", test_main_fault_stop_conditions),
        TEST_CASE("main flywheel and quickstop faults", test_main_flywheel_and_quickstop_faults),
        TEST_CASE("main state flag encoding", test_main_state_flag_encoding),
        TEST_CASE("main reset and engage runtime state", test_main_reset_and_engage_runtime_state),
        TEST_CASE(
            "main PID control modes and safety limits",
            test_main_pid_control_modes_and_safety_limits
        ),
        TEST_CASE(
            "main setpoint target enforces safety limits",
            test_main_setpoint_target_enforces_safety_limits
        ),
        TEST_CASE(
            "main setpoint target handles guard transitions",
            test_main_setpoint_target_handles_guard_transitions
        ),
        TEST_CASE("main rejects hostile safety config", test_main_rejects_hostile_safety_config),
        TEST_CASE("main bounds hostile safety limits", test_main_bounds_hostile_safety_limits),
        TEST_CASE(
            "main bounds fault and setpoint rate config",
            test_main_bounds_fault_and_setpoint_rate_config
        ),
        TEST_CASE("main EEPROM config round trip", test_main_eeprom_config_round_trip),
        TEST_CASE(
            "main rejects zero variable tiltback rate",
            test_main_rejects_zero_variable_tiltback_rate
        ),
        TEST_CASE("main startup loop reaches ready", test_main_startup_loop_reaches_ready),
        TEST_CASE(
            "main running loop keeps command finite", test_main_running_loop_keeps_command_finite
        ),
        TEST_CASE(
            "main special modes ignore input tilt", test_main_special_modes_ignore_input_tilt
        ),
        TEST_CASE(
            "main ready loop engages with both sensors",
            test_main_ready_loop_engages_with_both_sensors
        ),
        TEST_CASE("main ready loop toggles headlights", test_main_ready_loop_toggles_headlights),
        TEST_CASE("main ready loop alert timers", test_main_ready_loop_alert_timers),
        TEST_CASE(
            "main running loop stops on open switch", test_main_running_loop_stops_on_open_switch
        ),
        TEST_CASE(
            "main loop processes valid state inputs", test_main_loop_processes_valid_state_inputs
        ),
        TEST_CASE(
            "main loop handles targeted state transitions",
            test_main_loop_handles_targeted_state_transitions
        ),
        TEST_CASE(
            "main loop replays bounded event sequences",
            test_main_loop_replays_bounded_event_sequences
        ),
        TEST_CASE("main auxiliary loop runs one iteration", test_main_aux_loop_runs_one_iteration),
        TEST_CASE(
            "main rejects truncated command headers", test_main_rejects_truncated_command_headers
        ),
        TEST_CASE(
            "main rejects lock and handtest packets without payload",
            test_main_rejects_short_lock_and_handtest_payloads
        ),
        TEST_CASE(
            "main commands respect packet lengths", test_main_commands_respect_packet_lengths
        ),
        TEST_CASE(
            "main flywheel command guards and stop", test_main_flywheel_command_guards_and_stop
        ),
        TEST_CASE("main flywheel command configuration", test_main_flywheel_command_configuration),
        TEST_CASE("main flywheel footpad abort option", test_main_flywheel_footpad_abort_option),
        TEST_CASE("main ready flywheel abort paths", test_main_ready_flywheel_abort_paths),
        TEST_CASE(
            "main flywheel uses tight fault angles", test_main_flywheel_uses_tight_fault_angles
        ),
        TEST_CASE(
            "main flywheel reconfigures cached controls",
            test_main_flywheel_reconfigures_cached_controls
        ),
        TEST_CASE("main darkride faults stop safely", test_main_darkride_fault_matrix),
        TEST_CASE(
            "main switch and reverse-stop faults stop safely",
            test_main_switch_and_reverse_stop_fault_matrix
        ),
        TEST_CASE("main angle faults stop safely", test_main_angle_fault_matrix),
        TEST_CASE("main fault guard boundaries", test_main_fault_guard_boundaries),
        TEST_CASE("main all-data reports runtime mode", test_main_all_data_mode_matrix),
        TEST_CASE(
            "main info reports version and capabilities",
            test_main_info_version_and_capability_matrix
        ),
        TEST_CASE("main realtime reports internal state", test_main_internal_realtime_state_matrix),
        TEST_CASE(
            "main legacy realtime maps extended beeps to error",
            test_main_legacy_realtime_maps_extended_beeps_to_error
        ),
        TEST_CASE("main beeper state machine", test_main_beeper_state_machine),
        TEST_CASE(
            "main disabling beeper cancels output state",
            test_main_disabling_beeper_cancels_output_state
        ),
        TEST_CASE(
            "main Bluetooth tuning preserves legacy ranges", test_main_bluetooth_tuning_ranges
        ),
        TEST_CASE(
            "main tune defaults resets all tune families",
            test_main_tune_defaults_resets_all_tune_families
        ),
        TEST_CASE(
            "main EEPROM writes commit signature last and verify data",
            test_main_eeprom_write_commits_signature_last_and_verifies_data
        ),
        TEST_CASE(
            "main rejects config writes while running",
            test_main_rejects_config_writes_while_running
        ),
        TEST_CASE(
            "main serialized configuration keeps control outputs finite",
            test_main_serialized_configuration_keeps_control_outputs_finite
        ),
        TEST_CASE(
            "main rejects config commands in special modes",
            test_main_rejects_config_commands_in_special_modes
        ),
        TEST_CASE(
            "main remote respects ready state guards", test_main_remote_respects_ready_state_guards
        ),
        TEST_CASE(
            "main restore reconfigures live subsystems",
            test_main_restore_reconfigures_live_subsystems
        ),
        TEST_CASE(
            "main lock reconfigures restored values", test_main_lock_reconfigures_restored_values
        ),
        TEST_CASE("main init and stop lifecycle", test_main_init_and_stop_lifecycle),
        TEST_CASE(
            "main init OOM and stop without threads", test_main_init_oom_and_stop_without_threads
        ),
        TEST_CASE("main registered package callbacks", test_main_registered_package_callbacks),
        TEST_CASE(
            "main fatal error terminate lifecycle", test_main_fatal_error_terminate_runs_stop_path
        ),
        TEST_CASE(
            "main init main thread spawn failure cleanup",
            test_main_init_main_thread_spawn_failure_cleans_up
        ),
        TEST_CASE(
            "main init aux thread spawn failure cleanup",
            test_main_init_aux_thread_spawn_failure_cleans_up
        ),
        TEST_CASE(
            "main init eeprom allocation failure uses defaults",
            test_main_init_eeprom_allocation_failure_uses_defaults
        ),
        TEST_CASE(
            "main init uses VESC frequency and optional beeper config",
            test_main_init_uses_vesc_frequency_and_optional_beeper_config
        ),
        TEST_CASE(
            "main all data saturates oversized float16 fields",
            test_main_all_data_saturates_oversized_float16_fields
        ),
    };

    RUN_TEST_SUITE("main protocol summary", tests);
}
