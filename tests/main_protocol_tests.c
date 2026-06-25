#include "conf/buffer.h"
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

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
void refloat_main_fatal_error_terminate(void);
bool init(lib_info *info);

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

#include "c_tests/main_protocol/main_gnss_protocol.c"
#include "c_tests/main_protocol/main_alerts_lights_protocol.c"
#include "c_tests/main_protocol/main_command_length_protocol.c"
#include "c_tests/main_protocol/main_lifecycle_protocol.c"
#include "c_tests/main_protocol/main_all_data_serialization.c"

int main(void) {
    const TestCase tests[] = {
        TEST_CASE("main gnss protocol", test_main_gnss_protocol),
        XFAIL_CASE("main info handles optional gnss unavailable", test_main_info_handles_optional_gnss_unavailable,
                "red test: COMMAND_INFO calls VESC_IF->mc_gnss() and dereferences the result "
                "without checking whether the optional GNSS hook exists or returned data"),
        XFAIL_CASE("main realtime handles optional gnss unavailable", test_main_realtime_handles_optional_gnss_unavailable,
                "red test: realtime GNSS serialization calls VESC_IF->mc_gnss() and dereferences "
                "the result whenever a GNSS mask bit is requested, even if GNSS is unavailable"),
        TEST_CASE("main lights control protocol", test_main_lights_control_protocol),
        TEST_CASE("main alerts protocol", test_main_alerts_protocol),
        TEST_CASE("main invalid command protocol", test_main_invalid_command_protocol),
        XFAIL_CASE("main rejects lock and handtest packets without payload", test_main_rejects_short_lock_and_handtest_payloads,
                "red test: COMMAND_LOCK and COMMAND_HANDTEST dispatch with len == 2 and their "
                "handlers read cfg[0] even though the command did not provide a payload byte"),
        TEST_CASE("main init and stop lifecycle", test_main_init_and_stop_lifecycle),
        TEST_CASE("main fatal error terminate lifecycle", test_main_fatal_error_terminate_runs_stop_path),
        XFAIL_CASE("main init main thread spawn failure cleanup", test_main_init_main_thread_spawn_failure_cleans_up,
                "red test: init returns false when the main thread cannot be spawned but does not run "
                "the normal destroy/free path for the partially initialized Data allocation"),
        XFAIL_CASE("main init aux thread spawn failure cleanup", test_main_init_aux_thread_spawn_failure_cleans_up,
                "red test: init requests main-thread termination when the aux thread spawn fails but "
                "does not run the normal destroy/free path for the partially initialized Data allocation"),
        XFAIL_CASE("main init eeprom allocation failure uses defaults", test_main_init_eeprom_allocation_failure_uses_defaults,
                "red test: read_cfg_from_eeprom returns after temporary allocation failure without "
                "loading generated defaults, leaving freshly allocated Data with all-zero config"),
        XFAIL_CASE("main all data saturates oversized float16 fields", test_main_all_data_saturates_oversized_float16_fields,
                "red test: COMMAND_GET_ALLDATA serializes compact telemetry through "
                "buffer_append_float16, which casts scaled float values to int16_t without "
                "saturating oversized inputs"),
    };

    RUN_TEST_SUITE("main protocol summary", tests);
}
